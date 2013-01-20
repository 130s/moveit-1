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

/* Author: Ioan Sucan */

#ifndef MOVEIT_PLAN_EXECUTION_PLAN_EXECUTION_
#define MOVEIT_PLAN_EXECUTION_PLAN_EXECUTION_

#include <moveit/plan_execution/plan_representation.h>
#include <moveit/trajectory_execution_manager/trajectory_execution_manager.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <moveit/planning_scene_monitor/trajectory_monitor.h>
#include <moveit/sensor_manager/sensor_manager.h>
#include <pluginlib/class_loader.h>
#include <boost/scoped_ptr.hpp>

/** \brief This namespace includes functionality specific to the execution and monitoring of motion plans */
namespace plan_execution
{

class PlanExecution
{
public:
  
  struct Options
  {
    Options(void) : replan_(false),
                    replan_attempts_(0)
    {
    }
    
    bool replan_;
    unsigned int replan_attempts_;
    
    ExecutableMotionPlanComputationFn plan_callback_;
    boost::function<bool(ExecutableMotionPlan &plan_to_update,
                         kinematic_state::KinematicStatePtr &current_monitored_state,
                         std::size_t currently_executed_trajectory,
                         std::size_t currently_trajectory_waypoint)> repair_plan_callback_;

    boost::function<void(void)> before_plan_callback_;
    boost::function<void(void)> before_execution_callback_;
    boost::function<void(void)> done_callback_;
  };
  
  PlanExecution(const planning_scene_monitor::PlanningSceneMonitorPtr &planning_scene_monitor, 
                const trajectory_execution_manager::TrajectoryExecutionManagerPtr& trajectory_execution);
  ~PlanExecution(void);

  const planning_scene_monitor::PlanningSceneMonitorPtr& getPlanningSceneMonitor(void) const
  {
    return planning_scene_monitor_;
  }
  
  const trajectory_execution_manager::TrajectoryExecutionManagerPtr& getTrajectoryExecutionManager(void) const
  {
    return trajectory_execution_manager_;
  }

  double getTrajectoryStateRecordingFrequency(void) const
  {
    if (trajectory_monitor_)
      return trajectory_monitor_->getSamplingFrequency();
    else
      return 0.0;
  }
  
  void setTrajectoryStateRecordingFrequency(double freq)
  {
    if (trajectory_monitor_)
      trajectory_monitor_->setSamplingFrequency(freq);
  }

  void setMaxReplanAttempts(unsigned int attempts)
  {
    default_max_replan_attempts_ = attempts;
  }
  
  unsigned int getMaxReplanAttempts(void) const
  {
    return default_max_replan_attempts_;
  }

  void planAndExecute(ExecutableMotionPlan &plan, const Options &opt);
  void planAndExecute(ExecutableMotionPlan &plan, const moveit_msgs::PlanningScene &scene_diff, const Options &opt);

  void stop(void);

private:

  void planAndExecuteHelper(ExecutableMotionPlan &plan, const Options &opt);  
  moveit_msgs::MoveItErrorCodes executeAndMonitor(const ExecutableMotionPlan &plan);
  bool isRemainingPathValid(const ExecutableMotionPlan &plan);
  
  void planningSceneUpdatedCallback(const planning_scene_monitor::PlanningSceneMonitor::SceneUpdateType update_type);
  void doneWithTrajectoryExecution(const moveit_controller_manager::ExecutionStatus &status);
  
  ros::NodeHandle node_handle_;
  planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor_;
  trajectory_execution_manager::TrajectoryExecutionManagerPtr trajectory_execution_manager_;
  planning_scene_monitor::TrajectoryMonitorPtr trajectory_monitor_;

  unsigned int default_max_replan_attempts_;
  
  bool preempt_requested_;
  bool new_scene_update_;
  
  bool execution_complete_;
  
  class DynamicReconfigureImpl;
  DynamicReconfigureImpl *reconfigure_impl_;
};

typedef boost::shared_ptr<PlanExecution> PlanExecutionPtr;
typedef boost::shared_ptr<const PlanExecution> PlanExecutionConstPtr;

}
#endif
