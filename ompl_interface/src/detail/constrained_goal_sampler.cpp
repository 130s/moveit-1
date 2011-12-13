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

#include "ompl_interface/detail/constrained_goal_sampler.h"
#include <ompl/geometric/ik/GAIK.h>

ompl_interface::ConstrainedGoalSampler::ConstrainedGoalSampler(const PlanningGroup *pg, const kinematic_constraints::KinematicConstraintSetPtr &ks,
                                                               const kinematic_constraints::ConstraintSamplerPtr &cs) :
    ompl::base::GoalLazySamples(pg->getOMPLContext().getSpaceInformation(),
                                cs ? boost::bind(&ConstrainedGoalSampler::sampleUsingConstraintSampler, this, _1, _2) :
                                boost::bind(&ConstrainedGoalSampler::sampleUsingGAIK, this, _1, _2), false),
    pg_(pg), ks_(ks), cs_(cs), state_(pg->getStartState())
{
    startSampling();
}

bool ompl_interface::ConstrainedGoalSampler::sampleUsingGAIK(const ompl::base::GoalLazySamples *gls, ompl::base::State *newGoal)
{
    unsigned int ma = pg_->getMaximumSamplingAttempts();

    // terminate after too many attempts
    if (gls->samplingAttemptsCount() >= ma)
        return false;
    // terminate after a maximum number of samples
    if (gls->getStateCount() >= pg_->getMaximumGoalSamples())
        return false;
    // terminate the sampling thread when a solution has been found
    if (gls->isAchieved())
        return false;

    // this class is NOT a valid goal region from a thread safety point of view;
    // HOWEVER, it is only used with GAIK, which is single-threaded, so this is safe
    class ConstrainedGoalRegion : public ompl::base::GoalRegion
    {
    public:
        ConstrainedGoalRegion(const PlanningGroup *pg, const kinematic_constraints::KinematicConstraintSet *ks,
                              planning_models::KinematicState *state) :
            ompl::base::GoalRegion(pg->getOMPLContext().getSpaceInformation()), pg_(pg), ks_(ks), state_(state)
        {
        }

        virtual double distanceGoal(const ompl::base::State *st) const
        {
            pg_->getKMStateSpace().copyToKinematicState(*state_, st);
            return ks_->decide(*state_).second;
        }

        virtual bool isSatisfied(const ompl::base::State *st, double *distance) const
        {
            pg_->getKMStateSpace().copyToKinematicState(*state_, st);
            const std::pair<bool, double> &r = ks_->decide(*state_);
            if (distance)
                *distance = r.second;
            return r.first;
        }

    protected:

        const PlanningGroup                                 *pg_;
        const kinematic_constraints::KinematicConstraintSet *ks_;
        planning_models::KinematicState                     *state_;
    };

    ConstrainedGoalRegion reg(pg_, ks_.get(), &state_);
    ompl::geometric::GAIK g(si_);
    while (gls->isSampling())
        if (g.solve(0.1, reg, newGoal))
            return true;
    return false;
}

bool ompl_interface::ConstrainedGoalSampler::sampleUsingConstraintSampler(const ompl::base::GoalLazySamples *gls, ompl::base::State *newGoal)
{
    unsigned int ma = pg_->getMaximumSamplingAttempts();

    // terminate after too many attempts
    if (gls->samplingAttemptsCount() >= ma)
        return false;
    // terminate after a maximum number of samples
    if (gls->getStateCount() >= pg_->getMaximumGoalSamples())
        return false;
    // terminate the sampling thread when a solution has been found
    if (gls->isAchieved())
        return false;

    std::vector<double> values;
    for (unsigned int a = 0 ; a < ma && gls->isSampling() ; ++a)
        if (cs_->sample(values, pg_->getStartState(), ma))
        {
            state_.getJointStateGroup(pg_->getJointModelGroup()->getName())->setStateValues(values);
            if (ks_->decide(state_).first)
            {
                pg_->getKMStateSpace().copyToOMPLState(newGoal, values);
                return true;
            }
        }
    return false;
}
