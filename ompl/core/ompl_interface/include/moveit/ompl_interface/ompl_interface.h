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

#ifndef MOVEIT_OMPL_INTERFACE_OMPL_INTERFACE_
#define MOVEIT_OMPL_INTERFACE_OMPL_INTERFACE_

#include <moveit/ompl_interface/planning_context_manager.h>
#include <moveit/ompl_interface/constraints_library.h>
#include <moveit/constraint_samplers/constraint_sampler_manager.h>
#include <moveit_msgs/ComputePlanningPluginsBenchmark.h>
#include <moveit_msgs/GetMotionPlan.h>
#include <string>
#include <map>

/** \brief The MoveIt interface to OMPL */
namespace ompl_interface
{

/** @class OMPLInterface
 *  This class defines the interface to the motion planners in OMPL*/
class OMPLInterface
{
public: 
  
  OMPLInterface(const kinematic_model::KinematicModelConstPtr &kmodel);
  virtual ~OMPLInterface(void);
  
  /** @brief Specify configurations for the planners.
      @param pconfig Configurations for the different planners */
  void setPlanningConfigurations(const std::vector<PlanningConfigurationSettings> &pconfig)
  {
    context_manager_.setPlanningConfigurations(pconfig);
  }
  
  /** @brief Solve the planning problem */
  bool solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
             const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res) const;

  /** @brief Solve the planning problem but give a more detailed response */
  bool solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
             const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::MotionPlanDetailedResponse &res) const;
  
  /** @brief Benchmark the planning problem*/
  bool benchmark(const planning_scene::PlanningSceneConstPtr& planning_scene,
                 const moveit_msgs::ComputePlanningPluginsBenchmark::Request &req,                  
                 moveit_msgs::ComputePlanningPluginsBenchmark::Response &res) const;
  
  /** @brief Solve the planning problem
   *  @param config
   *  @param start_state The start state specified for the planning problem
   *  @param goal_constraints The goal constraints
   *  @param timeout The amount of time to spend on planning
   */
  ob::PathPtr solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                    const std::string &config, const kinematic_state::KinematicState &start_state,
                    const moveit_msgs::Constraints &goal_constraints, double timeout,
                    const std::string &factory_type = "") const;
  
  /** @brief Solve the planning problem
   *  @param config
   *  @param start_state The start state specified for the planning problem
   *  @param goal_constraints The goal constraints
   *  @param path_constraints The path constraints
   *  @param timeout The amount of time to spend on planning
   */
  ob::PathPtr solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                    const std::string &config, const kinematic_state::KinematicState &start_state,
                    const moveit_msgs::Constraints &goal_constraints,
                    const moveit_msgs::Constraints &path_constraints, double timeout,
                    const std::string &factory_type = "") const;
  
  void terminateSolve(void);

  ModelBasedPlanningContextPtr getLastPlanningContext(void) const
  {
    return context_manager_.getLastPlanningContext();    
  }
  
  ModelBasedPlanningContextPtr getPlanningContext(const moveit_msgs::MotionPlanRequest &req) const
  {
    ModelBasedPlanningContextPtr ctx = context_manager_.getPlanningContext(req);
    if (ctx)
      configureConstraints(ctx);
    return ctx;
  }
  
  ModelBasedPlanningContextPtr getPlanningContext(const std::string &config, const std::string &factory_type = "") const
  {
    ModelBasedPlanningContextPtr ctx = context_manager_.getPlanningContext(config, factory_type);
    if (ctx)
      configureConstraints(ctx);
    return ctx;
  }
    
  const PlanningContextManager& getPlanningContextManager(void) const
  {
    return context_manager_;
  }
  
  PlanningContextManager& getPlanningContextManager(void)
  {
    return context_manager_;
  }

  ConstraintsLibrary& getConstraintsLibrary(void)
  {
    return *constraints_library_;
  }

  const ConstraintsLibrary& getConstraintsLibrary(void) const
  {
    return *constraints_library_;
  }

  constraint_samplers::ConstraintSamplerManager& getConstraintSamplerManager(void)
  {
    return *constraint_sampler_manager_;
  }
  
  void useConstraintsApproximations(bool flag)
  {
    use_constraints_approximations_ = flag;
  }
  
  bool isUsingConstraintsApproximations(void) const
  {
    return use_constraints_approximations_;
  }

  void loadConstraintApproximations(const std::string &path)
  {
    constraints_library_->loadConstraintApproximations(path);
  }

  void saveConstraintApproximations(const std::string &path)
  {
    constraints_library_->saveConstraintApproximations(path);
  }
  
protected:

  void configureConstraints(const ModelBasedPlanningContextPtr &context) const
  {
    if (use_constraints_approximations_)
      context->setConstraintsApproximations(constraints_library_);
    else
      context->setConstraintsApproximations(ConstraintsLibraryPtr());
  }
  
  /** \brief Configure the OMPL planning context for a new planning request */
  ModelBasedPlanningContextPtr prepareForSolve(const moveit_msgs::MotionPlanRequest &req,
                                               const planning_scene::PlanningSceneConstPtr& planning_scene, 
                                               moveit_msgs::MoveItErrorCodes *error_code,
                                               unsigned int *attempts, double *timeout) const;
  
  /** \brief The kinematic model for which motion plans are computed */
  kinematic_model::KinematicModelConstPtr kmodel_;
  
  constraint_samplers::ConstraintSamplerManagerPtr constraint_sampler_manager_;
  
  PlanningContextManager context_manager_;

  ConstraintsLibraryPtr constraints_library_;
  
  bool use_constraints_approximations_;
  
};

}


#endif
