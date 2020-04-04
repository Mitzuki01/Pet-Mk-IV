#include <Arduino.h>

#include "ros.h"
#include <ros/time.h>
#include <ros/duration.h>

#include "timer.h"
#include "engines.h"
#include "line_followers.h"
#include "dist_sensors.h"
#include "chatter.h"

static const ros::Duration kEngineCallbackInterval(0, 20'000'000);      // 20 ms -> 50 Hz
static const ros::Duration kLfCallbackInterval(0, 10'000'000);          // 10 ms -> 100 Hz
static const ros::Duration kDistSensorCallbackInterval(0, 33'000'000);  // 33 ms -> ~30 Hz

pet::ros::NodeHandle nh;
Timer<4> timer(nh);

void setup()
{
    nh.initNode();
    delay(1);

    nh.loginfo("Arduino starting...");
    nh.spinOnce();

    enginesSetup();
    lineFollowerSetup();
    distSensorSetup();
    chatterSetup();

    timer.register_callback(enginesUpdate, kEngineCallbackInterval);
    timer.register_callback(lineFollowerUpdate, kLfCallbackInterval);
    timer.register_callback(distSensorUpdate, kDistSensorCallbackInterval);
    timer.register_callback(chatterUpdate, ros::Duration(0, 500'000'000));

    nh.loginfo("Arduino setup done!");
}

void loop()
{
    nh.spinOnce();
    timer.spin_once();
}