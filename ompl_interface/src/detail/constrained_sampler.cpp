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

#include "ompl_interface/detail/constrained_sampler.h"
#include "ompl_interface/model_based_planning_context.h"
#include <ompl/tools/debug/Profiler.h>

ompl_interface::ConstrainedSampler::ConstrainedSampler(const ModelBasedPlanningContext *pc, const kc::ConstraintSamplerPtr &cs) :
  ob::StateSampler(pc->getOMPLStateSpace().get()), planning_context_(pc), default_(space_->allocDefaultStateSampler()),
  constraint_sampler_(cs), constrained_success_(0), constrained_failure_(0)
{
}

bool ompl_interface::ConstrainedSampler::sampleC(ob::State *state)
{
  std::vector<double> values;
  if (constraint_sampler_->sample(values, planning_context_->getCompleteInitialRobotState(), planning_context_->getMaximumStateSamplingAttempts()))
  {
    planning_context_->getOMPLStateSpace()->copyToOMPLState(state, values);
    if (space_->satisfiesBounds(state))
    {
	++constrained_success_;
	return true;
    }
  }
  ++constrained_failure_;
  return false;
}

void ompl_interface::ConstrainedSampler::sampleUniform(ob::State *state)
{
  if (!sampleC(state))
    default_->sampleUniform(state);
}

void ompl_interface::ConstrainedSampler::sampleUniformNear(ob::State *state, const ob::State *near, const double distance)
{
  if (sampleC(state))
  {    
    double total_d = space_->distance(state, near);
    if (total_d > distance)
    {
      double dist = rng_.uniformReal(0.0, distance);
      space_->interpolate(near, state, dist / total_d, state);
    }
  }
  else
    default_->sampleUniformNear(state, near, distance);
}

void ompl_interface::ConstrainedSampler::sampleGaussian(ob::State *state, const ob::State *mean, const double stdDev)
{
  if (sampleC(state))
  {
    double dist = rng_.gaussian(0.0, stdDev);
    double total_d = space_->distance(state, mean);
    if (total_d > dist)
      space_->interpolate(mean, state, dist / total_d, state);
  }
  else
    default_->sampleGaussian(state, mean, stdDev);
}
