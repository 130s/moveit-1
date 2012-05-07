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

/* Author: Ioan Sucan, Sachin Chitta */

#ifndef MOVEIT_OMPL_INTERFACE_PARAMETERIZATION_JOINT_SPACE_JOINT_MODEL_STATE_SPACE_HELPER_
#define MOVEIT_OMPL_INTERFACE_PARAMETERIZATION_JOINT_SPACE_JOINT_MODEL_STATE_SPACE_HELPER_

#include "ompl_interface/parameterization/model_based_state_space.h"

namespace ompl_interface
{

// Construct an OMPL state space that corresponds to an array of joints. It is assumed that the copy* functions will be called
// with joints passed in the same order as for the constructor. This ensures efficient copy operations.
class JointModelStateSpaceHelper
{     
public:
  
  JointModelStateSpaceHelper(const std::vector<const pm::KinematicModel::JointModel*> &joints)
  {
    constructSpace(joints);
  }
  
  void copyToKinematicState(const std::vector<pm::KinematicState::JointState*> &js, const ob::State *state) const;
  void copyToOMPLState(ob::State *state, const std::vector<pm::KinematicState::JointState*> &js) const;
  //  void copyToOMPLState(ob::State *state, const std::vector<double> &values) const;
  
  const ob::StateSpacePtr& getStateSpace(void) const
  {
    return state_space_;
  }
  
private:
  
  /// The order in which the joints were used to construct the OMPL state space
  std::vector<std::size_t> joint_mapping_;
  
  /// The order in which the joint variables were used to construct the OMPL state space
  std::vector<std::size_t> variable_mapping_;
  
  std::vector<ob::StateSpacePtr> components_;
  
  ob::StateSpacePtr state_space_;
  
  void constructSpace(const std::vector<const pm::KinematicModel::JointModel*> &joints);
  
};



}

#endif
