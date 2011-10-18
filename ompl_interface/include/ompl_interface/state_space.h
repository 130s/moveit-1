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

#ifndef OMPL_INTERFACE_STATE_SPACE_
#define OMPL_INTERFACE_STATE_SPACE_

#include <ompl/base/StateSpace.h>
#include <ompl/tools/spaces/StateSpaceCollection.h>
#include <planning_models/kinematic_model.h>
#include <planning_models/kinematic_state.h>

namespace ompl_interface
{
    
    /// Construction of OMPL state space from kinematic joints
    class KMStateSpace
    {
    public:
	
	/// Construct the OMPL state space that corresponds to a set of joints. Record all the constructed spaces in the collection \e ssc
	KMStateSpace(ompl::StateSpaceCollection &ssc, const std::vector<const planning_models::KinematicModel::JointModel*> &joints);
	
	/// Get the constructed OMPL state space
	const ompl::base::StateSpacePtr& getOMPLSpace(void) const;
	
	/// Copy the data from a set of joint states to an OMPL state. The join states \b must be specified in the same order as the joint models in the constructor
	void copyToKinematicState(const std::vector<planning_models::KinematicState::JointState*> &js, const ompl::base::State *state) const;
	
	/// Copy the data from an OMPL state to a set of joint states. The join states \b must be specified in the same order as the joint models in the constructor
	void copyToOMPLState(ompl::base::State *state, const std::vector<const planning_models::KinematicState::JointState*> &js) const;
	
	/// Get the double value that corresponds to a joint name, directly from an OMPL state
	double* getOMPLStateValueAddress(const std::string &joint_name, ompl::base::State *state) const;

	/// Get the double value that corresponds to a joint name, directly from an OMPL state
	const double* getOMPLStateValueAddress(const std::string &joint_name, const ompl::base::State *state) const;
	
	const std::vector<const planning_models::KinematicModel::JointModel*> getJointModels(void) const
	{
	    return joints_;
	}
	
	/// Set the planning volume for the SE2 and/or SE3 components of the state space
	void setPlanningVolume(double minX, double maxX, double minY, double maxY, double minZ, double maxZ);
    
    private:
	
	void constructSpace(ompl::StateSpaceCollection &ssc, const std::vector<const planning_models::KinematicModel::JointModel*> &joints);
	
	/// The sequence of joint models that make up the state space
	std::vector<const planning_models::KinematicModel::JointModel*> joints_;

	/// The order in which the joints were used to construct the OMPL state space
	std::vector<std::size_t>                                        joint_mapping_;	   
	
	/// The state space that corresponds to the specified joints
	ompl::base::StateSpacePtr                                       space_;

	/// Keep the subspace components of the state space around for convenience
	std::vector<ompl::base::StateSpacePtr>                          all_components_;

	/// A simple tool that allows fast addressing of state values by name
	ompl::StateAddress                                              state_address_;
    };
    
    /*
    class KinematicStateConvertor
    {
    public:
	KinematicStateConvertor(const KMStateSpace &kss, const planning_models::KinematicState &full_state) : kss_(kss), full_state_(full_state)
	{
	}
	
	const planning_models::KinematicState& convertToKinematicState(const ompl::base::State *state) const;
	
    private:

	const KMStateSpace                                                     &kss_;	
	mutable std::map<boost::thread::id, planning_models::KinematicStatePtr> states_;
	mutable boost::mutex                                                    lock_;
    };
    */
	
}

#endif
    
