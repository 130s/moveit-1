/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Jon Binney, Ioan Sucan */

#ifndef MOVEIT_OCCUPANCY_MAP_MONITOR_
#define MOVEIT_OCCUPANCY_MAP_MONITOR_

#include <vector>
#include <string>
#include <ros/ros.h>
#include <tf/tf.h>
#include <moveit_msgs/SaveMap.h>
#include <moveit_msgs/LoadMap.h>
#include <moveit/occupancy_map_monitor/occupancy_map.h>
#include <moveit/occupancy_map_monitor/occupancy_map_updater.h>
#include <boost/thread/mutex.hpp>

namespace occupancy_map_monitor
{

// The type for the shape handle should be the same as the type of the mesh handle
typedef mesh_filter::MeshHandle ShapeHandle;

class OccupancyMapMonitor
{
public:
  
  OccupancyMapMonitor(const boost::shared_ptr<tf::Transformer> &tf,
                      const std::string &map_frame = "", double map_resolution = 0.0); 
  OccupancyMapMonitor(double map_resolution = 0.0);

  ~OccupancyMapMonitor();
  
  /** @brief start the monitor (will begin updating the octomap */
  void startMonitor();
  
  void stopMonitor();
  
  /** @brief Get a pointer to the underlying octree for this monitor. Lock the tree before reading or writing using this
   *  pointer. The value of this pointer stays the same throughout the existance of the monitor instance. */
  const OccMapTreePtr& getOcTreePtr()
  {
    return tree_;
  }

  /** @brief Get a const pointer to the underlying octree for this monitor. Lock the
   *  tree before reading this pointer */
  const OccMapTreeConstPtr& getOcTreePtr() const
  {
    return tree_const_;
  }
    
  const std::string& getMapFrame() const
  {
    return map_frame_;
  }
  
  void setMapFrame(const std::string &frame);
  
  double getMapResolution() const
  {
    return map_resolution_;    
  }
  
  const boost::shared_ptr<tf::Transformer>& getTFClient() const
  {
    return tf_;
  }
  
  ShapeHandle excludeShape(const shapes::ShapeConstPtr &shape);
  
  /** @brief Set the callback to trigger when updates to the maintained octomap are received */
  void setUpdateCallback(const boost::function<void()> &update_callback)
  {
    update_callback_ = update_callback;
    setUpdatersCallback();
  }

  void setTransformCallback(const boost::function<bool (ShapeHandle, Eigen::Affine3d&)>& transform_callback);
  
private:

  void initialize();

  /** @brief Save the current octree to a binary file */
  bool saveMapCallback(moveit_msgs::SaveMap::Request& request, moveit_msgs::SaveMap::Response& response);

  /** @brief Load octree from a binary file (gets rid of current octree data) */
  bool loadMapCallback(moveit_msgs::LoadMap::Request& request, moveit_msgs::LoadMap::Response& response);
  
  void setUpdatersCallback();
  
  bool getShapeTransform(std::size_t index, mesh_filter::MeshHandle h, Eigen::Affine3d &transform) const;
  
  boost::shared_ptr<tf::Transformer> tf_;
  std::string map_frame_;
  double map_resolution_;
  boost::mutex parameters_lock_;
  
  OccMapTreePtr tree_;
  OccMapTreeConstPtr tree_const_;
  
  std::vector<OccupancyMapUpdaterPtr> map_updaters_;
  boost::function<void()> update_callback_;
  std::vector<std::map<mesh_filter::MeshHandle, ShapeHandle> > mesh_handles_;
  boost::function<bool(ShapeHandle, Eigen::Affine3d&)> shape_transform_callback_;
  
  std::size_t mesh_handle_count_;
  
  ros::NodeHandle root_nh_;
  ros::NodeHandle nh_;
  ros::ServiceServer save_map_srv_;
  ros::ServiceServer load_map_srv_;
};

}

#endif /* MOVEIT_OCCUPANCY_MAP_MONITOR_H_ */
