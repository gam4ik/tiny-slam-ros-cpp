#ifndef __RVIZ_GRID_VIEWER_H
#define __RVIZ_GRID_VIEWER_H

#include <string>
#include <vector>
#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/OccupancyGrid.h>

#include "../core/state_data.h"
#include "../core/maps/grid_map.h"

/**
 * \brief The class publishes information about robot's map and location
 *  in ROS-compatible format so it can be shown by rviz.
 */
class RvizGridViewer {
public: // method
/**
 * Initializes a map and robot's position publisher.
 * \param pub A map publisher to ROS.
 */
  RvizGridViewer(ros::Publisher pub):
    _map_pub(pub) {}

/**
 * Publishes a robot state as TF message.
 * \param r A robot state in internal format.
 */
  void show_robot_pose(const RobotState &r) {
    tf::Transform t;
    t.setOrigin(tf::Vector3(r.x, r.y, 0.0));
    tf::Quaternion q;
    q.setRPY(0, 0, r.theta);
    t.setRotation(q);
    _tf_brcst.sendTransform(
      tf::StampedTransform(t, ros::Time::now(),
                           "odom_combined", "robot_pose"));
  }

/**
 * Publishes given GridMap as a ROS message.
 * \param map A grid map in framework's internal format.
 */
  void show_map(const GridMap &map) {
    // TODO: move map publishing rate to parameter
    if ((ros::Time::now() - _last_pub_time).toSec() < 5.0) {
      return;
    }

    nav_msgs::OccupancyGrid map_msg;
    map_msg.info.map_load_time = ros::Time::now();
    map_msg.info.width = map.width();
    map_msg.info.height = map.height();
    map_msg.info.resolution = map.scale();
    // move map to the middle
    nav_msgs::MapMetaData &info = map_msg.info;
    info.origin.position.x = -info.resolution * map.map_center_x();
    info.origin.position.y = -info.resolution * map.map_center_y();
    info.origin.position.z = 0;

    for (const auto &row : map.cells()) {
      for (const auto &cell : row) {
        int cell_value = cell->value() == -1 ? -1 : cell->value() * 100;
        map_msg.data.push_back(cell_value);
      }
    }

    _map_pub.publish(map_msg);
    _last_pub_time = ros::Time::now();
  }

private: // fields
  ros::Publisher _map_pub;
  ros::Time _last_pub_time;
  tf::TransformBroadcaster _tf_brcst;
};

#endif
