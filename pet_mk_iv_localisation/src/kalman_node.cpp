#include "kalman_node.h"

#include <string>
#include <memory>

#include <ros/ros.h>
#include <tf2_ros/transform_broadcaster.h>

#include <sensor_msgs/Imu.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Vector3Stamped.h>

#include <ugl/math/vector.h>
#include <ugl/math/quaternion.h>

#include <ugl_ros/convert_tf2.h>

#include "kalman_filter.h"

#include "startup_utility.h"

namespace pet
{

KalmanNode::KalmanNode(ros::NodeHandle& nh, ros::NodeHandle& nh_private)
    : m_nh(nh)
    , m_nh_private(nh_private)
    , m_base_frame(nh_private.param<std::string>("base_frame", "base_link"))
    , m_map_frame(nh_private.param<std::string>("map_frame", "map"))
{
    // TODO: Make topics configurable through ROS parameters.
    m_imu_sub       = m_nh.subscribe("imu", 10, &KalmanNode::imu_cb, this);
    m_sonar_sub     = m_nh.subscribe("dist_sensors", 10, &KalmanNode::sonar_cb, this);
    m_pose_pub      = m_nh.advertise<geometry_msgs::PoseStamped>("pose_filtered", 10);
    m_velocity_pub  = m_nh.advertise<geometry_msgs::Vector3Stamped>("vel_filtered", 10);

    const double frequency = m_nh_private.param<double>("frequency", 10.0);
    m_timer = m_nh.createTimer(1.0/frequency, &KalmanNode::timer_cb, this, false, false);

    initialise_kalman_filter();

    // TODO: Measure accelerometer bias.

    utility::wait_for_message<sensor_msgs::Imu>(m_imu_sub);
}

void KalmanNode::start()
{
    m_previous_imu_time = ros::Time::now();
    m_timer.start();
    ROS_INFO("Timer started!");
}

void KalmanNode::initialise_kalman_filter()
{
    const double x0     = m_nh_private.param<double>("initial/x", 0.0);
    const double y0     = m_nh_private.param<double>("initial/y", 0.0);
    const double theta0 = m_nh_private.param<double>("initial/theta", 0.0);

    const ugl::Vector<2> pos0 = ugl::Vector<2>{x0, y0};
    const ugl::Vector<2> vel0 = ugl::Vector<2>::Zero();

    m_kalman_filter = KalmanFilter(theta0, pos0, vel0);
}

void KalmanNode::timer_cb(const ros::TimerEvent& e)
{
    // TODO: Update the filter with measuremnts going from oldest to newest.

    publish_tf(e.current_real);
    publish_pose(e.current_real);
    publish_velocity(e.current_real);
}

void KalmanNode::imu_cb(const sensor_msgs::Imu& msg)
{
    m_imu_queue.emplace(msg);
}

void KalmanNode::sonar_cb(const pet_mk_iv_msgs::DistanceMeasurement& msg)
{
    // NOTE: Remember to ignore side-sonars.
    // TODO: Check time difference and push velocity measurement to 'm_sonar_queue'.
}

void KalmanNode::publish_tf(const ros::Time& stamp)
{
    m_tf_msg.header.stamp = stamp;
    m_tf_msg.header.frame_id = m_map_frame;
    m_tf_msg.child_frame_id = m_base_frame;

    const auto& pos = m_kalman_filter.position();
    m_tf_msg.transform.translation.x = pos.x();
    m_tf_msg.transform.translation.y = pos.y();

    const double yaw = m_kalman_filter.heading();
    const ugl::Vector3 axis = ugl::Vector3::UnitZ();
    m_tf_msg.transform.rotation = tf2::toMsg(ugl::math::to_quat(yaw, axis));

    m_tf_broadcaster.sendTransform(m_tf_msg);
}

void KalmanNode::publish_pose(const ros::Time& stamp)
{
    m_pose_msg.header.stamp = stamp;
    m_pose_msg.header.frame_id = m_map_frame;

    const auto& pos = m_kalman_filter.position();
    m_pose_msg.pose.position.x = pos.x();
    m_pose_msg.pose.position.y = pos.y();

    const double yaw = m_kalman_filter.heading();
    const ugl::Vector3 axis = ugl::Vector3::UnitZ();
    m_tf_msg.transform.rotation = tf2::toMsg(ugl::math::to_quat(yaw, axis));

    m_pose_pub.publish(m_pose_msg);
}

void KalmanNode::publish_velocity(const ros::Time& stamp)
{
    m_vel_msg.header.stamp = stamp;
    m_vel_msg.header.frame_id = m_map_frame;

    const auto& vel = m_kalman_filter.velocity();
    m_vel_msg.vector.x = vel.x();
    m_vel_msg.vector.y = vel.y();

    m_velocity_pub.publish(m_vel_msg);
}

} // namespace pet

int main(int argc, char** argv)
{
    ros::init(argc, argv, "kalman_node");
    ros::NodeHandle nh("");
    ros::NodeHandle nh_private("~");

    ROS_INFO("Initialising node...");
    pet::KalmanNode node(nh, nh_private);
    ROS_INFO("Node initialisation done.");

    node.start();
    ros::spin();
}