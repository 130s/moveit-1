/*
 * Copyright (c) 2011, Willow Garage, Inc.
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

#include <moveit_visualization_ros/kinematics_start_goal_visualization.h>

namespace moveit_visualization_ros
{

KinematicsStartGoalVisualization::KinematicsStartGoalVisualization(boost::shared_ptr<planning_scene_monitor::PlanningSceneMonitor>& planning_scene_monitor,
                                                                   boost::shared_ptr<interactive_markers::InteractiveMarkerServer>& interactive_marker_server, 
                                                                   const std::string& group_name, 
                                                                   const std::string& kinematics_solver_name,
                                                                   ros::Publisher& marker_publisher)
{
  std_msgs::ColorRGBA good_color;
  good_color.g = good_color.a = 1.0;

  std_msgs::ColorRGBA bad_color;
  bad_color.r = bad_color.a = 1.0;
  
  start_.reset(new KinematicsGroupVisualization(planning_scene_monitor,
                                                interactive_marker_server,
                                                group_name,
                                                "start_position",
                                                kinematics_solver_name,
                                                good_color,
                                                bad_color,
                                                marker_publisher));

  goal_.reset(new KinematicsGroupVisualization(planning_scene_monitor,
                                              interactive_marker_server,
                                              group_name,
                                              "end_position",
                                              kinematics_solver_name,
                                              good_color,
                                              bad_color,
                                              marker_publisher));
}

}
