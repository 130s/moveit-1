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

/* Author: Ioan Sucan */

#ifndef MOVEIT_PLANNING_SCENE_MONITOR_TRAJECTORY_MONITOR_
#define MOVEIT_PLANNING_SCENE_MONITOR_TRAJECTORY_MONITOR_

#include <moveit/planning_scene_monitor/current_state_monitor.h>
#include <moveit_msgs/RobotTrajectory.h>
#include <boost/thread.hpp>

namespace planning_scene_monitor
{

typedef boost::function<void(const planning_models::KinematicStateConstPtr &state, const ros::Time &stamp)> TrajectoryStateAddedCallback;

/** @class TrajectoryMonitor
    @brief Monitors the joint_states topic and tf to record the trajectory of the robot. */
class TrajectoryMonitor
{
public:
  
  /** @brief Constructor.
   */
  TrajectoryMonitor(const CurrentStateMonitorConstPtr &state_monitor, double sampling_frequency = 5.0);
  
  ~TrajectoryMonitor(void);
  
  void startTrajectoryMonitor(void);
  
  void stopTrajectoryMonitor(void);
  
  void clearTrajectory(void);
  
  bool isActive(void) const;

  double getSamplingFrequency(void) const
  {
    return sampling_frequency_;
  }
  
  void setSamplingFrequency(double sampling_frequency);
  
  /// Return the current maintained trajectory. This function is not thread safe (hence NOT const), because the trajectory could be modified.
  const std::vector<planning_models::KinematicStateConstPtr>& getTrajectoryStates(void)
  {
    return trajectory_states_;
  }

  /// Return the time stamps for the current maintained trajectory. This function is not thread safe (hence NOT const), because the trajectory could be modified.
  const std::vector<ros::Time>& getTrajectoryStamps(void)
  {
    return trajectory_stamps_;
  }

  void getTrajectory(moveit_msgs::RobotTrajectory &trajectory);
  
  void setOnStateAddCallback(const TrajectoryStateAddedCallback &callback)
  {
    state_add_callback_ = callback;
  }
  
private:
  
  void recordStates(void);
  
  CurrentStateMonitorConstPtr current_state_monitor_;
  double sampling_frequency_;

  std::vector<planning_models::KinematicStateConstPtr> trajectory_states_;
  std::vector<ros::Time> trajectory_stamps_;

  boost::scoped_ptr<boost::thread> record_states_thread_;
  TrajectoryStateAddedCallback state_add_callback_;
};

typedef boost::shared_ptr<TrajectoryMonitor> TrajectoryMonitorPtr;
typedef boost::shared_ptr<const TrajectoryMonitor> TrajectoryMonitorConstPtr;
}

#endif
