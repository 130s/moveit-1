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

#ifndef _PLACE_EVALUATION_VISUALIZATION_H_
#define _PLACE_EVALUATION_VISUALIZATION_H_

#include <grasp_place_evaluation/place_evaluator_fast.h>
#include <interactive_markers/interactive_marker_server.h>
#include <kinematics_plugin_loader/kinematics_plugin_loader.h>
#include <moveit_visualization_ros/joint_trajectory_visualization.h>

namespace moveit_manipulation_visualization {

class PlaceEvaluationVisualization {
  
public:

  PlaceEvaluationVisualization(const planning_scene::PlanningSceneConstPtr& planning_scene,
                               boost::shared_ptr<interactive_markers::InteractiveMarkerServer>& interactive_marker_server,
                               boost::shared_ptr<kinematics_plugin_loader::KinematicsPluginLoader>& kinematics_plugin_loader,
                               ros::Publisher& marker_publisher);
  
  ~PlaceEvaluationVisualization() {};
  
  void updatePlanningScene(const planning_scene::PlanningSceneConstPtr& planning_scene) {
    planning_scene_ = planning_scene;
  }
  
  void removeAllMarkers();

  void resetPlaceExecutionInfo();

  void evaluatePlaceLocations(const std::string& group_name,
                              const moveit_manipulation_msgs::PlaceGoal& goal,
                              const planning_models::KinematicState* seed_state,
                              const std::vector<geometry_msgs::PoseStamped>& place_locations);

  void showPlacePose(unsigned int num,
                     bool show_place,
                     bool show_preplace,
                     bool show_retreat);
  
  void playInterpolatedTrajectories(unsigned int num,
                                    bool play_approach,
                                    bool play_retreat);
  

  unsigned int getEvaluationInfoSize() const {
    return last_place_evaluation_info_.size();
  }
  
  bool getEvaluatedPlace(unsigned int num,
                         grasp_place_evaluation::PlaceExecutionInfo& grasp) const;
  
  boost::shared_ptr<moveit_visualization_ros::JointTrajectoryVisualization>& getJointTrajectoryVisualization() {
    return joint_trajectory_visualization_;
  }

protected:

  void playInterpolatedTrajectoriesThread(unsigned int num,
                                          bool play_approach,
                                          bool play_retreat);

  planning_scene::PlanningSceneConstPtr planning_scene_;
  ros::Publisher marker_publisher_;
  visualization_msgs::MarkerArray last_marker_array_;
  
  grasp_place_evaluation::PlaceExecutionInfoVector last_place_evaluation_info_;
  boost::shared_ptr<grasp_place_evaluation::PlaceEvaluatorFast> place_evaluator_fast_;

  boost::shared_ptr<moveit_visualization_ros::JointTrajectoryVisualization> joint_trajectory_visualization_;
};

}
#endif
