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

#ifndef MOVEIT_PLANNING_MODELS_CONVERSIONS_
#define MOVEIT_PLANNING_MODELS_CONVERSIONS_

#include <moveit/planning_models/transforms.h>
#include <moveit_msgs/RobotState.h>
#include <moveit_msgs/RobotTrajectory.h>

namespace planning_models
{
/**
 * @brief Convert a joint state to a kinematic state
 * @param joint_state The input joint state to be converted
 * @param state The resultant kinematic state
 * @return True if successful, false if failed for any reason
 */
bool jointStateToKinematicState(const sensor_msgs::JointState &joint_state, planning_models::KinematicState& state);

/**
 * @brief Convert a robot state (with accompanying extra transforms) to a kinematic state
 * @param tf An instance of a transforms object
 * @param robot_state The input robot state
 * @param state The resultant kinematic state
 * @return True if successful, false if failed for any reason
 */
bool robotStateToKinematicState(const Transforms &tf, const moveit_msgs::RobotState &robot_state, KinematicState& state, bool copy_attached_bodies = true);
/**
 * @brief Convert a robot state (with accompanying extra transforms) to a kinematic state
 * @param robot_state The input robot state
 * @param state The resultant kinematic state
 * @return True if successful, false if failed for any reason
 */
bool robotStateToKinematicState(const moveit_msgs::RobotState &robot_state, KinematicState& state, bool copy_attached_bodies = true);

/**
 * @brief Convert a kinematic state to a robot state message
 * @param state The input kinematic state object
 * @param robot_state The resultant RobotState message
 */
void kinematicStateToRobotState(const KinematicState& state, moveit_msgs::RobotState &robot_state, bool copy_attached_bodies = true);

/**
 * @brief Convert a kinematic state to a joint state message
 * @param state The input kinematic state object
 * @param robot_state The resultant JointState message
 */
void kinematicStateToJointState(const KinematicState& state, sensor_msgs::JointState &joint_state);

/**
 * @brief Convert a RobotTrajectoryPoint (in a RobotTrajectory) to a RobotState message
 * @param rt The input RobotTrajectory
 * @param index The index of the point that we want to convert
 * @param rs The resultant robot state
 * @return True if any data was copied to \e rs. If \e index is out of range, no data is copied and false is returned.
 */
bool robotTrajectoryPointToRobotState(const moveit_msgs::RobotTrajectory &rt, std::size_t index, moveit_msgs::RobotState &rs);
}

#endif
