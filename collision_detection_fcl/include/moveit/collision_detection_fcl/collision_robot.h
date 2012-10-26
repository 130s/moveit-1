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

#ifndef MOVEIT_COLLISION_DETECTION_FCL_COLLISION_ROBOT_
#define MOVEIT_COLLISION_DETECTION_FCL_COLLISION_ROBOT_

#include <moveit/collision_detection_fcl/collision_common.h>

namespace collision_detection
{
  
  class CollisionRobotFCL : public CollisionRobot
  {
    friend class CollisionWorldFCL;
    
  public:
    
    CollisionRobotFCL(const kinematic_model::KinematicModelConstPtr &kmodel, double padding = 0.0, double scale = 1.0);
    
    CollisionRobotFCL(const CollisionRobotFCL &other);
    
    virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state) const;    
    virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state, const AllowedCollisionMatrix &acm) const;
    virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state1, const kinematic_state::KinematicState &state2) const;
    virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state1, const kinematic_state::KinematicState &state2, const AllowedCollisionMatrix &acm) const;

    virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state,
				     const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state) const;
    virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state,
				     const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state,
				     const AllowedCollisionMatrix &acm) const;
    virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state1, const kinematic_state::KinematicState &state2,
                                     const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state1, const kinematic_state::KinematicState &other_state2) const;
    virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state1, const kinematic_state::KinematicState &state2,
                                     const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state1, const kinematic_state::KinematicState &other_state2,
                                     const AllowedCollisionMatrix &acm) const;

    virtual double distanceSelf(const kinematic_state::KinematicState &state) const;
    virtual double distanceSelf(const kinematic_state::KinematicState &state, const AllowedCollisionMatrix &acm) const;
    virtual double distanceOther(const kinematic_state::KinematicState &state,
                                 const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state) const;
    virtual double distanceOther(const kinematic_state::KinematicState &state, const CollisionRobot &other_robot,
                                 const kinematic_state::KinematicState &other_state, const AllowedCollisionMatrix &acm) const;
 
  protected:
    
    virtual void updatedPaddingOrScaling(const std::vector<std::string> &links);
    void constructFCLObject(const kinematic_state::KinematicState &state, FCLObject &fcl_obj) const;
    void allocSelfCollisionBroadPhase(const kinematic_state::KinematicState &state, FCLManager &manager) const;
    void getAttachedBodyObjects(const kinematic_state::AttachedBody *ab, std::vector<FCLGeometryConstPtr> &geoms) const;
    
    void checkSelfCollisionHelper(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state,
				  const AllowedCollisionMatrix *acm) const;
    void checkOtherCollisionHelper(const CollisionRequest &req, CollisionResult &res, const kinematic_state::KinematicState &state,
				   const CollisionRobot &other_robot, const kinematic_state::KinematicState &other_state,
				   const AllowedCollisionMatrix *acm) const;
    double distanceSelfHelper(const kinematic_state::KinematicState &state, const AllowedCollisionMatrix *acm) const;
    double distanceOtherHelper(const kinematic_state::KinematicState &state, const CollisionRobot &other_robot,
                               const kinematic_state::KinematicState &other_state, const AllowedCollisionMatrix *acm) const;
    
    std::vector<const kinematic_model::LinkModel*> links_;
    std::vector<FCLGeometryConstPtr>               geoms_;
    std::map<std::string, std::size_t>             index_map_;
  };

}

#endif
