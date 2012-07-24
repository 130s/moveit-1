/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2012, Willow Garage, Inc.
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

#include <sbpl_interface/sbpl_interface.h>
#include <sbpl_interface/environment_chain3d.h>
#include <planning_models/conversions.h>

namespace sbpl_interface {

bool SBPLInterface::solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                          const moveit_msgs::GetMotionPlan::Request &req, 
                          moveit_msgs::GetMotionPlan::Response &res) const
{
  planning_models::KinematicState start_state(planning_scene->getCurrentState());
  planning_models::robotStateToKinematicState(*planning_scene->getTransforms(), req.motion_plan_request.start_state, start_state);

  ros::WallTime wt = ros::WallTime::now();  
  EnvironmentChain3D* env_chain = new EnvironmentChain3D(planning_scene);
  std::cerr << "Created" << std::endl;
  if(!env_chain->setupForMotionPlan(planning_scene, 
                                    req)){
    std::cerr << "Env chain setup failing" << std::endl;
    return false;
  }
  
  //DummyEnvironment* dummy_env = new DummyEnvironment();
  SBPLPlanner *planner = new ARAPlanner(env_chain, true);
  planner->set_initialsolution_eps(100.0);
  planner->set_search_mode(true);
  planner->force_planning_from_scratch();
  planner->set_start(env_chain->getPlanningData().start_hash_entry_->stateID);
  planner->set_goal(env_chain->getPlanningData().goal_hash_entry_->stateID);
  std::cerr << "Creation took " << (ros::WallTime::now()-wt) << std::endl;
  std::vector<int> solution_state_ids;
  int solution_cost;
  wt = ros::WallTime::now();
  bool b_ret = planner->replan(10.0, &solution_state_ids, &solution_cost);
  std::cerr << "B ret is " << b_ret << " planning time " << (ros::WallTime::now()-wt) << std::endl;
  if(!b_ret) {
    return false;
  }
  if(solution_state_ids.size() == 0) {
    std::cerr << "Success but no path" << std::endl;
    return false;
  }
  if(!env_chain->populateTrajectoryFromStateIDSequence(solution_state_ids,
                                                       res.trajectory.joint_trajectory)) {
    std::cerr << "Success but path bad" << std::endl;
    return false;
  }
  res.error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
  return true;
}

}
