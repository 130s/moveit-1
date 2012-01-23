/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Author: E. Gil Jones

#include <moveit_visualization_ros/joint_trajectory_visualization.h>

namespace moveit_visualization_ros
{

JointTrajectoryVisualization::JointTrajectoryVisualization(boost::shared_ptr<planning_scene_monitor::PlanningSceneMonitor>& planning_scene_monitor,
                                                           ros::Publisher& marker_publisher)
  : planning_scene_monitor_(planning_scene_monitor),
    marker_publisher_(marker_publisher),
    current_state_(planning_scene_monitor_->getPlanningScene()->getCurrentState())
{
  
}; 

void JointTrajectoryVisualization::setTrajectory(const planning_models::KinematicState& start_state,
                                                 const trajectory_msgs::JointTrajectory& traj,
                                                 const std_msgs::ColorRGBA& color)
{
  current_state_ = start_state;
  current_joint_trajectory_ = traj;
  marker_color_ = color;
}

void JointTrajectoryVisualization::playCurrentTrajectory()
{
  playback_start_time_ = ros::WallTime::now();
  current_point_ = 0;

  const planning_models::KinematicModel::LinkModel* lm = planning_scene_monitor_->getPlanningScene()->getKinematicModel()->getJointModel(current_joint_trajectory_.joint_names[0])->getChildLinkModel();
  
  link_model_names_ = planning_scene_monitor_->getPlanningScene()->getKinematicModel()->getChildLinkModelNames(lm);

  playback_thread_ = new boost::thread(boost::bind(&JointTrajectoryVisualization::advanceTrajectory, this));
}

void JointTrajectoryVisualization::advanceTrajectory() {
  while(ros::ok() && current_point_ < current_joint_trajectory_.points.size()) {
    std::map<std::string, double> joint_state;
    for(unsigned int i = 0; i < current_joint_trajectory_.joint_names.size(); i++) {
      joint_state[current_joint_trajectory_.joint_names[i]] =
        current_joint_trajectory_.points[current_point_].positions[i];
    }
    current_state_.setStateValues(joint_state);
    visualization_msgs::MarkerArray arr;
    current_state_.getRobotMarkers(marker_color_,
                                   "joint_trajectory",
                                   ros::Duration(0.0),
                                   arr,
                                   link_model_names_);
    marker_publisher_.publish(arr);
    current_point_++;
    ros::WallDuration(.05).sleep();
  }
}

}
