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

/* Author: Ioan Sucan, Sachin Chitta */

#include <moveit/ompl_interface/ompl_interface.h>
#include <moveit/kinematic_state/conversions.h>
#include <ompl/tools/debug/Profiler.h>
#include <fstream>

ompl_interface::OMPLInterface::OMPLInterface(const kinematic_model::KinematicModelConstPtr &kmodel) :
  kmodel_(kmodel),
  constraint_sampler_manager_(new  constraint_samplers::ConstraintSamplerManager()),
  context_manager_(kmodel, constraint_sampler_manager_),
  constraints_library_(new ConstraintsLibrary(context_manager_)),
  use_constraints_approximations_(true)
{
}

ompl_interface::OMPLInterface::~OMPLInterface(void)
{
}

ompl_interface::ModelBasedPlanningContextPtr ompl_interface::OMPLInterface::getPlanningContext(const moveit_msgs::MotionPlanRequest &req) const
{
  ModelBasedPlanningContextPtr ctx = context_manager_.getPlanningContext(req);
  if (ctx)
    configureConstraints(ctx);
  return ctx;
}

ompl_interface::ModelBasedPlanningContextPtr ompl_interface::OMPLInterface::getPlanningContext(const std::string &config, const std::string &factory_type) const
{
  ModelBasedPlanningContextPtr ctx = context_manager_.getPlanningContext(config, factory_type);
  if (ctx)
    configureConstraints(ctx);
  return ctx;
}

void ompl_interface::OMPLInterface::configureConstraints(const ModelBasedPlanningContextPtr &context) const
{
  if (use_constraints_approximations_)
    context->setConstraintsApproximations(constraints_library_);
  else
    context->setConstraintsApproximations(ConstraintsLibraryPtr());
}

ompl_interface::ModelBasedPlanningContextPtr ompl_interface::OMPLInterface::prepareForSolve(const moveit_msgs::MotionPlanRequest &req, 
                                                                                            const planning_scene::PlanningSceneConstPtr& planning_scene,
                                                                                            moveit_msgs::MoveItErrorCodes *error_code,
                                                                                            unsigned int *attempts, double *timeout) const
{
  ot::Profiler::ScopedBlock sblock("OMPLInterface:PrepareForSolve");

  kinematic_state::KinematicState start_state = planning_scene->getCurrentState();
  kinematic_state::robotStateToKinematicState(*planning_scene->getTransforms(), req.start_state, start_state);

  ModelBasedPlanningContextPtr context = getPlanningContext(req);
  if (!context)
  {
    error_code->val = moveit_msgs::MoveItErrorCodes::INVALID_GROUP_NAME;
    return context;
  }
  
  *timeout = req.allowed_planning_time.toSec();
  if (*timeout <= 0.0)
  {
    logInform("The timeout for planning must be positive (%lf specified). Assuming one second instead.", *timeout);
    *timeout = 1.0;
  }
  
  *attempts = 1;
  if (req.num_planning_attempts > 0)
    *attempts = req.num_planning_attempts;
  else
    if (req.num_planning_attempts < 0)
      logError("The number of desired planning attempts should be positive. Assuming one attempt.");
  
  context->clear();
  
  // set the planning scene
  context->setPlanningScene(planning_scene);
  context->setCompleteInitialState(start_state);
  
  context->setPlanningVolume(req.workspace_parameters);
  if (!context->setPathConstraints(req.path_constraints, error_code))
    return ModelBasedPlanningContextPtr();
  if (!context->setGoalConstraints(req.goal_constraints, req.path_constraints, error_code))
    return ModelBasedPlanningContextPtr();
  context->configure();
  logDebug("%s: New planning context is set.", context->getName().c_str());
  error_code->val = moveit_msgs::MoveItErrorCodes::SUCCESS;

  return context;
}

bool ompl_interface::OMPLInterface::solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                          const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res) const
{
  ompl::tools::Profiler::ScopedStart pslv;
  ot::Profiler::ScopedBlock sblock("OMPLInterface:Solve");

  unsigned int attempts = 1;
  double timeout = 0.0;
  ModelBasedPlanningContextPtr context = prepareForSolve(req.motion_plan_request, planning_scene, &res.error_code, &attempts, &timeout);
  if (!context)
    return false;

  if (context->solve(timeout, attempts))
  {
    double ptime = context->getLastPlanTime();
    if (ptime < timeout)
    {
      context->simplifySolution(timeout - ptime);
      ptime += context->getLastSimplifyTime();
    }
    context->interpolateSolution();
      
    // fill the response
    logDebug("%s: Returning successful solution with %lu states", context->getName().c_str(),
             context->getOMPLSimpleSetup().getSolutionPath().getStateCount());
    kinematic_state::kinematicStateToRobotState(context->getCompleteInitialRobotState(), res.trajectory_start);
    context->getSolutionPath(res.trajectory);
    res.planning_time = ros::Duration(ptime);
    return true;
  }
  else
  {
    logInform("Unable to solve the planning problem");
    res.error_code.val = moveit_msgs::MoveItErrorCodes::PLANNING_FAILED;
    return false;
  }
}

bool ompl_interface::OMPLInterface::solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
					  const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::MotionPlanDetailedResponse &res) const
{
  ompl::tools::Profiler::ScopedStart pslv;
  ot::Profiler::ScopedBlock sblock("OMPLInterface:Solve");
  
  unsigned int attempts = 1;
  double timeout = 0.0;
  moveit_msgs::MoveItErrorCodes error_code; // not used
  ModelBasedPlanningContextPtr context = prepareForSolve(req.motion_plan_request, planning_scene, &error_code, &attempts, &timeout);
  if (!context)
    return false;
  
  if (context->solve(timeout, attempts))
  {
    res.trajectory.reserve(3);
    kinematic_state::kinematicStateToRobotState(context->getCompleteInitialRobotState(), res.trajectory_start);
    
    // add info about planned solution
    double ptime = context->getLastPlanTime();
    res.processing_time.push_back(ros::Duration(ptime));
    res.description.push_back("plan");
    res.trajectory.resize(res.trajectory.size() + 1);
    context->getSolutionPath(res.trajectory.back());
    
    // simplify solution if time remains
    if (ptime < timeout)
    {
      context->simplifySolution(timeout - ptime);
      res.processing_time.push_back(ros::Duration(context->getLastSimplifyTime()));
      res.description.push_back("simplify");
      res.trajectory.resize(res.trajectory.size() + 1);
      context->getSolutionPath(res.trajectory.back());
    }
    
    ros::WallTime start_interpolate = ros::WallTime::now();
    context->interpolateSolution();
    res.processing_time.push_back(ros::Duration((ros::WallTime::now() - start_interpolate).toSec()));
    res.description.push_back("interpolate");
    res.trajectory.resize(res.trajectory.size() + 1);
    context->getSolutionPath(res.trajectory.back());
    
    // fill the response
    logDebug("%s: Returning successful solution with %lu states", context->getName().c_str(),
             context->getOMPLSimpleSetup().getSolutionPath().getStateCount());
    return true;
  }
  else
  {
    logInform("Unable to solve the planning problem");
    error_code.val = moveit_msgs::MoveItErrorCodes::PLANNING_FAILED;
    return false;
  }
}

bool ompl_interface::OMPLInterface::benchmark(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                              const moveit_msgs::ComputePlanningPluginsBenchmark::Request &req,
                                              moveit_msgs::ComputePlanningPluginsBenchmark::Response &res) const
{  
  unsigned int attempts = 1;
  double timeout = 0.0;

  ModelBasedPlanningContextPtr context = prepareForSolve(req.motion_plan_request, planning_scene, &res.error_code, &attempts, &timeout);
  if (!context)
    return false;
  return context->benchmark(timeout, attempts, req.filename);
}

ompl::base::PathPtr ompl_interface::OMPLInterface::solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                                         const std::string &config, const kinematic_state::KinematicState &start_state,
                                                         const moveit_msgs::Constraints &goal_constraints, double timeout,
                                                         const std::string &factory_type) const
{
  moveit_msgs::Constraints empty;
  return solve(planning_scene, config, start_state, goal_constraints, empty, timeout, factory_type);
}

ompl::base::PathPtr ompl_interface::OMPLInterface::solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                                         const std::string &config, const kinematic_state::KinematicState &start_state,
                                                         const moveit_msgs::Constraints &goal_constraints,
                                                         const moveit_msgs::Constraints &path_constraints, double timeout,
                                                         const std::string &factory_type) const
{ 
  ompl::tools::Profiler::ScopedStart pslv;
  ot::Profiler::ScopedBlock sblock("OMPLInterface:Solve");

  ModelBasedPlanningContextPtr context = getPlanningContext(config, factory_type);
  if (!context)
    return ob::PathPtr();
  
  std::vector<moveit_msgs::Constraints> goal_constraints_v(1, goal_constraints);  
  context->setPlanningScene(planning_scene);
  context->setCompleteInitialState(start_state);
  context->setPathConstraints(path_constraints, NULL);
  context->setGoalConstraints(goal_constraints_v, path_constraints, NULL);
  context->configure();
  
  // solve the planning problem
  if (context->solve(timeout, 1))
  {
    double ptime = context->getLastPlanTime();
    if (ptime < timeout)
      context->simplifySolution(timeout - ptime);
    context->interpolateSolution();
    return ob::PathPtr(new og::PathGeometric(context->getOMPLSimpleSetup().getSolutionPath()));
  }
  
  return ob::PathPtr();  
}

void ompl_interface::OMPLInterface::terminateSolve(void)
{
  const ModelBasedPlanningContextPtr &context = getLastPlanningContext();
  if (context)
    context->terminateSolve();
}
