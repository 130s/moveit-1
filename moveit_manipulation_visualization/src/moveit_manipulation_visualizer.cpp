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

#include <moveit_manipulation_visualization/moveit_manipulation_visualizer.h>

namespace moveit_manipulation_visualization {

MoveItManipulationVisualizer::MoveItManipulationVisualizer() :
  MoveItVisualizer() 
{
  
  grasp_evaluation_visualization_dialog_ = new GraspEvaluationVisualizationDialog(main_window_,
                                                                                  planning_scene_monitor_->getPlanningScene(),
                                                                                  interactive_marker_server_,
                                                                                  kinematics_plugin_loader_,
                                                                                  vis_marker_array_publisher_);
  grasp_evaluation_visualization_dialog_->planningGroupChanged(QString(pv_->getCurrentGroup().c_str()));
  

  iov_->addMenuEntry("Attempt To Grasp",
                     boost::bind(&MoveItManipulationVisualizer::attemptToGrasp, this, _1));
 
  QObject::connect(planning_group_selection_menu_, SIGNAL(groupSelected(const QString&)),
                   grasp_evaluation_visualization_dialog_, SLOT(planningGroupChanged(const QString&)));

  QObject::connect(grasp_evaluation_visualization_dialog_, 
                   SIGNAL(requestSetGoalState(const std::string&, const planning_models::KinematicState*)),
                   pv_.get(),
                   SLOT(setGoalStateRequested(const std::string&, const planning_models::KinematicState*)));
  QObject::connect(grasp_evaluation_visualization_dialog_,
                   SIGNAL(requestPlanGeneration(bool)),
                   pv_.get(),
                   SLOT(generatePlanRequested(bool)));
  QObject::connect(pv_.get(),
                   SIGNAL(planGenerated(const std::string&, const trajectory_msgs::JointTrajectory&)),
                   grasp_evaluation_visualization_dialog_,
                   SLOT(planGenerationFinished(const std::string&, const trajectory_msgs::JointTrajectory&)));
  QObject::connect(pv_.get(),
                   SIGNAL(planFailed(moveit_msgs::MoveItErrorCodes&)),
                   grasp_evaluation_visualization_dialog_,
                   SLOT(planGenerationFailed(moveit_msgs::MoveItErrorCodes&)));
  
}

void MoveItManipulationVisualizer::updatePlanningScene(planning_scene::PlanningSceneConstPtr planning_scene)
{
  MoveItVisualizer::updatePlanningScene(planning_scene);
  grasp_evaluation_visualization_dialog_->updatePlanningScene(planning_scene);
}



void MoveItManipulationVisualizer::attemptToGrasp(const std::string& obj) {
  grasp_evaluation_visualization_dialog_->show();
  //boost::thread(boost::bind(&MoveItManipulationVisualizer::attemptToGraspThread, this, obj));
}

// void MoveItManipulationVisualizer::attemptToGraspThread(const std::string& obj) {
//   moveit_msgs::CollisionObject co;
//   if(!current_diff_->getCollisionObjectMsg(obj, 
//                                            co)) {
//     ROS_WARN_STREAM("Don't appear to have object " << obj << " in planning scene");
//     return;
//   }

//   moveit_manipulation_msgs::PickupGoal goal;
//   goal.arm_name = pv_->getCurrentGroup();
//   goal.collision_object_name = obj;
  
//   goal.target.collision_name = obj;
//   goal.target.reference_frame_id = co.header.frame_id;

//   goal.lift.direction.vector.z = 1;
//   goal.lift.desired_distance = .1;

//   std::vector<moveit_manipulation_msgs::Grasp> grasps;
//   grasps.resize(1);
//   grasps[0].grasp_pose = co.poses[0];
//   grasps[0].grasp_pose.position.x -= ((co.shapes[0].dimensions[0]/2.0)+.15); 
//   grasps[0].desired_approach_distance = .12;
//   grasps[0].min_approach_distance = .12;
//   if(pv_->getCurrentGroup() == "right_arm") {
//     grasps[0].pre_grasp_posture.name.push_back("r_gripper_r_finger_joint");
//     grasps[0].pre_grasp_posture.name.push_back("r_gripper_l_finger_joint");
//     grasps[0].pre_grasp_posture.name.push_back("r_gripper_r_finger_tip_joint");
//     grasps[0].pre_grasp_posture.name.push_back("r_gripper_l_finger_tip_joint");
//   } else {
//     grasps[0].pre_grasp_posture.name.push_back("l_gripper_r_finger_joint");
//     grasps[0].pre_grasp_posture.name.push_back("l_gripper_l_finger_joint");
//     grasps[0].pre_grasp_posture.name.push_back("l_gripper_r_finger_tip_joint");
//     grasps[0].pre_grasp_posture.name.push_back("l_gripper_l_finger_tip_joint");
//   }
//   grasps[0].pre_grasp_posture.position.push_back(.35);
//   grasps[0].pre_grasp_posture.position.push_back(.35);
//   grasps[0].pre_grasp_posture.position.push_back(.35);
//   grasps[0].pre_grasp_posture.position.push_back(.35);
//   grasps[0].grasp_posture.name = grasps[0].pre_grasp_posture.name;
//   grasps[0].grasp_posture.position.push_back(.25);
//   grasps[0].grasp_posture.position.push_back(.25);
//   grasps[0].grasp_posture.position.push_back(.25);
//   grasps[0].grasp_posture.position.push_back(.25);

//   grasp_evaluation_visualization_dialog_->evaluateGrasps(pv_->getCurrentGroup(),
//                                                          goal,
//                                                          &current_diff_->getCurrentState(),
//                                                          grasps);
// }

}
