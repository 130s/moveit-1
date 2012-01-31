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

#include "ompl_interface/ompl_interface.h"
#include <planning_models/conversions.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <ompl/util/Profiler.h>

ompl_interface::OMPLInterface::OMPLInterface(void) : configured_(false)
{   
  constraints_.reset(new std::vector<ConstraintApproximation>());
}

ompl_interface::OMPLInterface::~OMPLInterface(void)
{
}

bool ompl_interface::OMPLInterface::configure(const planning_scene::PlanningSceneConstPtr &scene, const std::vector<PlanningConfigurationSettings> &pconfig)
{
  scene_ = scene;
  if (!scene_ || !scene_->isConfigured())
  {
    ROS_ERROR("Cannot configure OMPL interface without configured planning scene");
    return false;
  }
  
  // construct specified configurations
  for (std::size_t i = 0 ; i < pconfig.size() ; ++i)
  {
    const planning_models::KinematicModel::JointModelGroup *jmg = scene_->getKinematicModel()->getJointModelGroup(pconfig[i].group);
    if (jmg)
    {
      planning_groups_[pconfig[i].name].reset(new PlanningConfiguration(pconfig[i].name, jmg, constraints_, pconfig[i].config, scene_));
      ROS_INFO_STREAM("Added planning configuration '" << pconfig[i].name << "'");
    }
  }
  // construct default configurations
  const std::map<std::string, planning_models::KinematicModel::JointModelGroup*>& groups = scene_->getKinematicModel()->getJointModelGroupMap();
  for (std::map<std::string, planning_models::KinematicModel::JointModelGroup*>::const_iterator it = groups.begin() ; it != groups.end() ; ++it)
    if (planning_groups_.find(it->first) == planning_groups_.end())
    {
      static const std::map<std::string, std::string> empty;
      planning_groups_[it->first].reset(new PlanningConfiguration(it->first, it->second, constraints_, empty, scene_));
      ROS_INFO_STREAM("Added planning configuration '" << it->first << "'");
    }
  
  configured_ = true;
  return true;
}

void ompl_interface::OMPLInterface::configureIKSolvers(const std::map<std::string, kinematic_constraints::IKAllocator> &ik_allocators)
{
  for (std::map<std::string, PlanningConfigurationPtr>::iterator it = planning_groups_.begin() ; it != planning_groups_.end() ; ++it)
  {
    std::map<std::string, kinematic_constraints::IKAllocator>::const_iterator jt = ik_allocators.find(it->second->getJointModelGroup()->getName());
    if (jt == ik_allocators.end())
    {
      // if an IK allocator is NOT available for this group, we try to see if we can use subgroups for IK
      const planning_models::KinematicModel::JointModelGroup *jmg = it->second->getJointModelGroup();
      const planning_models::KinematicModel *km = jmg->getParentModel();
      std::set<const planning_models::KinematicModel::JointModel*> joints;
      joints.insert(jmg->getJointModels().begin(), jmg->getJointModels().end());
      
      std::vector<const planning_models::KinematicModel::JointModelGroup*> subs;
      
      // go through the groups that we know have IK allocators and see if they are included in the group that does not; fi so, put that group in sub
      for (std::map<std::string, kinematic_constraints::IKAllocator>::const_iterator kt = ik_allocators.begin() ; kt != ik_allocators.end() ; ++kt)
      {
	const planning_models::KinematicModel::JointModelGroup *sub = km->getJointModelGroup(kt->first);
	std::set<const planning_models::KinematicModel::JointModel*> sub_joints;
	sub_joints.insert(sub->getJointModels().begin(), sub->getJointModels().end());
	
	if (std::includes(joints.begin(), joints.end(), sub_joints.begin(), sub_joints.end()))
	{
	  std::set<const planning_models::KinematicModel::JointModel*> result;
	  std::set_difference(joints.begin(), joints.end(), sub_joints.begin(), sub_joints.end(),
			      std::inserter(result, result.end()));
	  subs.push_back(sub);
	  joints = result;
	}
      }
      
      // if we found subgroups, pass that information to the planning group
      if (!subs.empty())
      {
	kinematic_constraints::IKSubgroupAllocator &sa = it->second->ik_subgroup_allocators_;
	std::stringstream ss;
	for (std::size_t i = 0 ; i < subs.size() ; ++i)
	{
	  ss << subs[i]->getName() << " ";
	  sa.ik_allocators_[subs[i]] = ik_allocators.find(subs[i]->getName())->second;
	}
	ROS_INFO("Added sub-group IK allocators for group '%s': [ %s]", jmg->getName().c_str(), ss.str().c_str());
      }
    }
    else
      // if the IK allocator is for this group, we use it
      it->second->ik_allocator_ = jt->second;
  }
}

void ompl_interface::OMPLInterface::setMaximumSamplingAttempts(unsigned int max_sampling_attempts)
{
  for (std::map<std::string, PlanningConfigurationPtr>::iterator it = planning_groups_.begin() ; it != planning_groups_.end() ; ++it)
    it->second->setMaximumSamplingAttempts(max_sampling_attempts);
}

void ompl_interface::OMPLInterface::setMaximumGoalSamples(unsigned int max_goal_samples)
{
  for (std::map<std::string, PlanningConfigurationPtr>::iterator it = planning_groups_.begin() ; it != planning_groups_.end() ; ++it)
    it->second->setMaximumGoalSamples(max_goal_samples);
}

void ompl_interface::OMPLInterface::setMaximumPlanningThreads(unsigned int max_planning_threads)
{
  for (std::map<std::string, PlanningConfigurationPtr>::iterator it = planning_groups_.begin() ; it != planning_groups_.end() ; ++it)
    it->second->setMaximumPlanningThreads(max_planning_threads);
}

bool ompl_interface::OMPLInterface::prepareForSolve(const moveit_msgs::MotionPlanRequest &req, moveit_msgs::MoveItErrorCodes &error_code,
                                                    PlanningConfigurationPtr &pg_to_use, unsigned int &attempts, double &timeout) const
{
  if (req.group_name.empty())
  {
    error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_GROUP_NAME;
    ROS_ERROR("No group specified to plan for");
    return false;
  }
  
  // identify the correct planning group
  std::map<std::string, PlanningConfigurationPtr>::const_iterator pg = planning_groups_.end();
  if (!req.planner_id.empty())
  {
    pg = planning_groups_.find(req.group_name + "[" + req.planner_id + "]");
    if (pg == planning_groups_.end())
      ROS_WARN_STREAM("Cannot find planning configuration for group '" << req.group_name
		      << "' using planner '" << req.planner_id << "'. Will use defaults instead.");
  }
  if (pg == planning_groups_.end())
  {
    pg = planning_groups_.find(req.group_name);
    if (pg == planning_groups_.end())
    {
      error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_GROUP_NAME;
      ROS_ERROR_STREAM("Cannot find planning configuration for group '" << req.group_name << "'");
      return false;
    }
  }
  
  // configure the planning group
  
  // get the starting state
  planning_models::KinematicState ks = scene_->getCurrentState();
  planning_models::robotStateToKinematicState(*scene_->getTransforms(), req.start_state, ks);
  
  if (!pg->second->setupPlanningContext(ks, req.goal_constraints, req.path_constraints, &error_code))
    return false;
  pg->second->setPlanningVolume(req.workspace_parameters);
  
  // solve the planning problem
  timeout = req.allowed_planning_time.toSec();
  if (timeout <= 0.0)
  {
    error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_ALLOWED_PLANNING_TIME;
    ROS_ERROR("The timeout for planning must be positive (%lf specified). Assuming one second instead.", timeout);
    timeout = 1.0;
  }
  
  pg_to_use = pg->second;
  
  attempts = 1;
  if (req.num_planning_attempts > 0)
    attempts = req.num_planning_attempts;
  else
    if (req.num_planning_attempts < 0)
      ROS_ERROR("The number of desired planning attempts should be positive. Assuming one attempt.");
  
  return true;
}

bool ompl_interface::OMPLInterface::solve(const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res) const
{
  ompl::Profiler::ScopedStart pslv;
  
  PlanningConfigurationPtr pg;
  unsigned int attempts = 0;
  double timeout = 0.0;
  if (!prepareForSolve(req.motion_plan_request, res.error_code, pg, attempts, timeout))
    return false;
  last_planning_configuration_solve_ = pg;
  
  if (pg->solve(timeout, attempts))
  {
    double ptime = pg->getLastPlanTime();
    if (ptime < timeout)
      pg->simplifySolution(timeout - ptime);
    pg->interpolateSolution();
    
    // fill the response
    ROS_DEBUG("%s: Returning successful solution with %lu states", pg->getName().c_str(),
	      pg->getOMPLSimpleSetup().getSolutionPath().states.size());
    planning_models::kinematicStateToRobotState(pg->getStartState(), res.robot_state);
    res.planning_time = ros::Duration(pg->getLastPlanTime());
    pg->getSolutionPath(res.trajectory);
    res.error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
    return true;
  }
  else
  {
    ROS_INFO("Unable to solve the planning problem");
    return false;
  }
}

bool ompl_interface::OMPLInterface::benchmark(const moveit_msgs::ComputePlanningBenchmark::Request &req, moveit_msgs::ComputePlanningBenchmark::Response &res) const
{
  PlanningConfigurationPtr pg;
  unsigned int attempts = 0;
  double timeout = 0.0;
  if (!prepareForSolve(req.motion_plan_request, res.error_code, pg, attempts, timeout))
    return false;
  res.error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
  return pg->benchmark(timeout, attempts, req.filename);
}

bool ompl_interface::OMPLInterface::solve(const std::string &config, const planning_models::KinematicState &start_state, const moveit_msgs::Constraints &goal_constraints, double timeout) const
{
  moveit_msgs::Constraints empty;
  return solve(config, start_state, goal_constraints, empty, timeout);
}

bool ompl_interface::OMPLInterface::solve(const std::string &config, const planning_models::KinematicState &start_state, const moveit_msgs::Constraints &goal_constraints,
                                          const moveit_msgs::Constraints &path_constraints, double timeout) const
{ 
  ompl::Profiler::ScopedStart pslv;

  std::map<std::string, PlanningConfigurationPtr>::const_iterator pg = planning_groups_.find(config);
  if (pg == planning_groups_.end())
  {
    ROS_ERROR("Planner configuration '%s' not found", config.c_str());
    return false;
  }
  
  // configure the planning group
  std::vector<moveit_msgs::Constraints> goal_constraints_v(1, goal_constraints);
  if (!pg->second->setupPlanningContext(start_state, goal_constraints_v, path_constraints))
    return false;
  
  last_planning_configuration_solve_ = pg->second;
  
  // solve the planning problem
  if (pg->second->solve(timeout, 1))
  {
    double ptime = pg->second->getLastPlanTime();
    if (ptime < timeout)
      pg->second->simplifySolution(timeout - ptime);
    pg->second->interpolateSolution();
    return true;
  }
  
  return false;
}

const ompl_interface::PlanningConfigurationPtr& ompl_interface::OMPLInterface::getPlanningConfiguration(const std::string &config) const
{
  std::map<std::string, PlanningConfigurationPtr>::const_iterator pg = planning_groups_.find(config);
  if (pg == planning_groups_.end())
  {
    ROS_ERROR("Planner configuration '%s' not found", config.c_str());
    static PlanningConfigurationPtr empty;
    return empty;
  }
  else
    return pg->second;
}

void ompl_interface::OMPLInterface::addConstraintApproximation(const moveit_msgs::Constraints &constr, const std::string &group, unsigned int samples)
{
  addConstraintApproximation(constr, constr, group, samples);
}

void ompl_interface::OMPLInterface::addConstraintApproximation(const moveit_msgs::Constraints &constr_sampling, const moveit_msgs::Constraints &constr_hard, const std::string &group, unsigned int samples)
{  
  const PlanningConfigurationPtr &pg = getPlanningConfiguration(group);
  if (pg)
  {
    ompl::base::StateStoragePtr ss = pg->constructConstraintApproximation(constr_sampling, constr_hard, samples);
    if (ss)
      constraints_->push_back(ConstraintApproximation(scene_->getKinematicModel(), group, constr_hard, group + "_" + 
						      boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + ".ompldb", ss));
    else
      ROS_ERROR("Unable to construct constraint approximation for group '%s'", group.c_str());
  }
}

void ompl_interface::OMPLInterface::loadConstraintApproximations(const std::string &path)
{
  ROS_INFO("Loading constrained space approximations from '%s'", path.c_str());
  
  std::ifstream fin((path + "/list").c_str());
  if (!fin.good())
    ROS_ERROR("Unable to find 'list' in folder '%s'", path.c_str());
  
  while (fin.good() && !fin.eof())
  {
    std::string group, serialization, filename;
    fin >> group;
    if (fin.eof())
      break;
    fin >> serialization;    
    if (fin.eof())
      break;
    fin >> filename;
    const PlanningConfigurationPtr &pg = getPlanningConfiguration(group);
    if (pg)
    {
      ConstraintApproximationStateStorage *cass = new ConstraintApproximationStateStorage(pg->getOMPLSimpleSetup().getStateSpace());
      cass->load((path + "/" + filename).c_str());
      for (std::size_t i = 0 ; i < cass->size() ; ++i)
	cass->getState(i)->as<KMStateSpace::StateType>()->tag = i;
      constraints_->push_back(ConstraintApproximation(scene_->getKinematicModel(), group, serialization, filename, ompl::base::StateStoragePtr(cass)));
    }
  }
}

void ompl_interface::OMPLInterface::saveConstraintApproximations(const std::string &path)
{
  ROS_INFO("Saving %u constrained space approximations to '%s'", (unsigned int)constraints_->size(), path.c_str());
  
  std::ofstream fout((path + "/list").c_str());
  for (std::size_t i = 0 ; i < constraints_->size() ; ++i)
  {
    fout << constraints_->at(i).group_ << std::endl;
    fout << constraints_->at(i).serialization_ << std::endl;
    fout << constraints_->at(i).ompldb_filename_ << std::endl;
    if (constraints_->at(i).state_storage_)
      constraints_->at(i).state_storage_->store((path + "/" + constraints_->at(i).ompldb_filename_).c_str());
  }
  fout.close();
}

void ompl_interface::OMPLInterface::clearConstraintApproximations(void)
{
  constraints_->clear();
}

void ompl_interface::OMPLInterface::printConstraintApproximations(std::ostream &out) const
{
  for (std::size_t i = 0 ; i < constraints_->size() ; ++i)
  {
    out << constraints_->at(i).group_ << std::endl;
    out << constraints_->at(i).ompldb_filename_ << std::endl;
    out << constraints_->at(i).constraint_msg_ << std::endl;
  }
}

void ompl_interface::OMPLInterface::updatePlanningScene(const planning_scene::PlanningSceneConstPtr& planning_scene) 
{
  scene_ = planning_scene;
  for(std::map<std::string, PlanningConfigurationPtr>::iterator it = planning_groups_.begin();
      it != planning_groups_.end();
      it++) {
    it->second->updatePlanningScene(scene_);
  }
}
