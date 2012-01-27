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

#include <moveit_visualization_ros/interactive_marker_helper_functions.h>
#include <moveit_visualization_ros/planning_visualization.h>
#include <kinematic_constraints/utils.h>
#include <planning_models/conversions.h>

namespace moveit_visualization_ros {

PlanningVisualization::PlanningVisualization(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                             boost::shared_ptr<interactive_markers::InteractiveMarkerServer>& interactive_marker_server,
                                             ros::Publisher& marker_publisher)
  : ompl_interface_(planning_scene)
{
  group_visualization_.reset(new KinematicsStartGoalVisualization(planning_scene,
                                                                  interactive_marker_server,
                                                                  "right_arm",
                                                                  "pr2_arm_kinematics/PR2ArmKinematicsPlugin",
                                                                  marker_publisher));


  group_visualization_->addMenuEntry("Plan", boost::bind(&PlanningVisualization::generatePlan, this));
  joint_trajectory_visualization_.reset(new JointTrajectoryVisualization(planning_scene,
                                                                         marker_publisher));
} 

void PlanningVisualization::updatePlanningScene(const planning_scene::PlanningSceneConstPtr& planning_scene) {
  ompl_interface_.updatePlanningScene(planning_scene);
  group_visualization_->updatePlanningScene(planning_scene);
  joint_trajectory_visualization_->updatePlanningScene(planning_scene);
}

void PlanningVisualization::addMenuEntry(const std::string& name, 
                                         const boost::function<void(void)>& callback)
{
  group_visualization_->addMenuEntry(name, callback);
}


void PlanningVisualization::generatePlan(void) {

  ROS_INFO_STREAM("Getting request to plan");

  const planning_models::KinematicState& start_state = group_visualization_->getStartState();

  const planning_models::KinematicState& goal_state = group_visualization_->getGoalState();
  
  moveit_msgs::GetMotionPlan::Request req;
  moveit_msgs::GetMotionPlan::Response res;

  req.motion_plan_request.group_name = "right_arm";
  planning_models::kinematicStateToRobotState(start_state,req.motion_plan_request.start_state);
  req.motion_plan_request.goal_constraints.push_back(kinematic_constraints::constructGoalConstraints(goal_state.getJointStateGroup("right_arm"),
                                                                                                     .001, .001));
  req.motion_plan_request.num_planning_attempts = 1;
  req.motion_plan_request.allowed_planning_time = ros::Duration(3.0);

  std_msgs::ColorRGBA col;
  col.a = .8;
  col.b = 1.0;

  ompl_interface_.solve(req, res);
  ROS_INFO_STREAM("Trajectory has " << res.trajectory.joint_trajectory.points.size() << " joint names " << res.trajectory.joint_trajectory.joint_names.size());
  joint_trajectory_visualization_->setTrajectory(start_state,
                                                 res.trajectory.joint_trajectory,
                                                 col);

  joint_trajectory_visualization_->playCurrentTrajectory();
}

}
