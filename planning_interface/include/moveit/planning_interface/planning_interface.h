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
*   * Neither the name of Willow Garage, Inc. nor the names of its
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

#ifndef MOVEIT_PLANNING_INTERFACE_PLANNING_INTERFACE_
#define MOVEIT_PLANNING_INTERFACE_PLANNING_INTERFACE_

#include <moveit/planning_scene/planning_scene.h>
#include <moveit_msgs/GetMotionPlan.h>
#include <moveit_msgs/MotionPlanDetailedResponse.h>

namespace planning_interface
{

/** \brief Base class for a MoveIt planner */
class Planner
{
public:
  
  Planner()
  {
  }

  virtual ~Planner() 
  {
  };
  
  /// Subclass may implement methods below
  virtual void init(const kinematic_model::KinematicModelConstPtr& model) { }
  
  /// Get a short string that identifies the planning interface
  virtual std::string getDescription(void) const { return ""; }
  
  /// Get the names of the known planning algorithms (values that can be filled as planner_id in the planning request)
  virtual void getPlanningAlgorithms(std::vector<std::string> &algs) const { }

  /// Subclass must implement methods below
  virtual bool solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                     const moveit_msgs::GetMotionPlan::Request &req, 
                     moveit_msgs::GetMotionPlan::Response &res) const = 0;
  
  virtual bool solve(const planning_scene::PlanningSceneConstPtr& planning_scene,
                     const moveit_msgs::GetMotionPlan::Request &req, 
                     moveit_msgs::MotionPlanDetailedResponse &res) const = 0;
  
  /// Determine whether this plugin instance is able to represent this planning request
  virtual bool canServiceRequest(const moveit_msgs::GetMotionPlan::Request &req)  const = 0;
  
  /// Request termination, if a solve() function is currently computing plans
  virtual void terminate(void) const = 0;
  
};

typedef boost::shared_ptr<Planner> PlannerPtr;
typedef boost::shared_ptr<const Planner> PlannerConstPtr;

} // planning_interface

#endif
