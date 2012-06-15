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

/** \author E. Gil Jones */

#include <collision_distance_field/collision_world_distance_field.h>
#include <collision_distance_field/collision_common_distance_field.h>
#include <distance_field/propagation_distance_field.h>

namespace collision_distance_field
{

CollisionWorldDistanceField::CollisionWorldDistanceField(double size_x, 
                                                         double size_y,
                                                         double size_z,
                                                         bool use_signed_distance_field,
                                                         double resolution,
                                                         double collision_tolerance,
                                                         double max_propogation_distance)
  : collision_detection::CollisionWorld(),
    size_x_(size_x),
    size_y_(size_y),
    size_z_(size_z),
    use_signed_distance_field_(use_signed_distance_field),
    resolution_(resolution),
    collision_tolerance_(collision_tolerance),
    max_propogation_distance_(max_propogation_distance)
{  
  distance_field_cache_entry_ = generateDistanceFieldCacheEntry();
}

void CollisionWorldDistanceField::checkRobotCollision(const collision_detection::CollisionRequest &req, 
                                                      collision_detection::CollisionResult &res, 
                                                      const collision_detection::CollisionRobot &robot, 
                                                      const planning_models::KinematicState &state, 
                                                      const collision_detection::AllowedCollisionMatrix &acm) const
{
  boost::shared_ptr<const distance_field::DistanceField> env_distance_field = distance_field_cache_entry_->distance_field_;
  const collision_distance_field::CollisionRobotDistanceField& cdr = dynamic_cast<const CollisionRobotDistanceField&>(robot);
  boost::shared_ptr<const CollisionRobotDistanceField::DistanceFieldCacheEntry> dfce;

  boost::shared_ptr<CollisionRobotDistanceField::GroupStateRepresentation> gsr = cdr.generateCollisionCheckingStructures(req.group_name,
                                                                                                                         state,
                                                                                                                         &acm,
                                                                                                                         dfce,
                                                                                                                         false);
  getEnvironmentProximityGradients(dfce, gsr, env_distance_field);
  (const_cast<CollisionWorldDistanceField*>(this))->last_gsr_ = gsr;
  //checkRobotCollisionHelper(req, res, robot, state, &acm);
}

boost::shared_ptr<CollisionRobotDistanceField::GroupStateRepresentation>  
CollisionWorldDistanceField::getCollisionGradients(const collision_detection::CollisionRequest &req, 
                                                   collision_detection::CollisionResult &res, 
                                                   const collision_detection::CollisionRobot &robot, 
                                                   const planning_models::KinematicState &state, 
                                                   const collision_detection::AllowedCollisionMatrix &acm) const {
  boost::shared_ptr<const distance_field::DistanceField> env_distance_field = distance_field_cache_entry_->distance_field_;
  const collision_distance_field::CollisionRobotDistanceField& cdr = dynamic_cast<const CollisionRobotDistanceField&>(robot);
  boost::shared_ptr<const CollisionRobotDistanceField::DistanceFieldCacheEntry> dfce;
  
  boost::shared_ptr<CollisionRobotDistanceField::GroupStateRepresentation> gsr = cdr.generateCollisionCheckingStructures(req.group_name,
                                                                                                                         state,
                                                                                                                         &acm,
                                                                                                                         dfce,
                                                                                                                         true);
  cdr.getSelfProximityGradients(dfce, gsr);
  cdr.getIntraGroupProximityGradients(dfce, gsr);
  getEnvironmentProximityGradients(dfce, gsr, env_distance_field);
  return gsr;
}

bool CollisionWorldDistanceField::getEnvironmentProximityGradients(const boost::shared_ptr<const CollisionRobotDistanceField::DistanceFieldCacheEntry>& dfce,
                                                                   boost::shared_ptr<CollisionRobotDistanceField::GroupStateRepresentation>& gsr,
                                                                   const boost::shared_ptr<const distance_field::DistanceField>& env_distance_field) const {
  bool in_collision = false;
  for(unsigned int i = 0; i < dfce->link_names_.size(); i++) {
    if(!dfce->link_has_geometry_[i]) continue;
    bool coll = getCollisionSphereGradients(env_distance_field.get(),
                                            gsr->link_body_decompositions_[i]->getCollisionSpheres(),
                                            gsr->link_body_decompositions_[i]->getSphereCenters(),
                                            gsr->gradients_[i],
                                            collision_distance_field::ENVIRONMENT,
                                            0.0,
                                            false,
                                            max_propogation_distance_,
                                            false);
    if(coll) {
      in_collision = true;
    }
  }
  return in_collision;
} 

void CollisionWorldDistanceField::addToObject(const std::string& id,
                                              const shapes::ShapeConstPtr &shape, 
                                              const Eigen::Affine3d &pose)
{
  ros::WallTime n = ros::WallTime::now();
  CollisionWorld::addToObject(id, shape, pose);
  std::vector<Eigen::Vector3d> add_points;
  std::vector<Eigen::Vector3d> subtract_points;
  updateDistanceObject(id, distance_field_cache_entry_, add_points, subtract_points);
  distance_field_cache_entry_->distance_field_->removePointsFromField(subtract_points);
  distance_field_cache_entry_->distance_field_->addPointsToField(add_points);
  ROS_DEBUG_STREAM("Adding took " << (ros::WallTime::now()-n));
}

bool CollisionWorldDistanceField::moveShapeInObject(const std::string &id, 
                                                    const shapes::ShapeConstPtr &shape, 
                                                    const Eigen::Affine3d &pose)
{
  if (CollisionWorld::moveShapeInObject(id, shape, pose))
  {
    ros::WallTime n = ros::WallTime::now();
    std::vector<Eigen::Vector3d> add_points;
    std::vector<Eigen::Vector3d> subtract_points;
    updateDistanceObject(id, distance_field_cache_entry_, add_points, subtract_points);
    distance_field_cache_entry_->distance_field_->removePointsFromField(subtract_points);
    distance_field_cache_entry_->distance_field_->addPointsToField(add_points);
    ROS_DEBUG_STREAM("Moving took " << (ros::WallTime::now()-n));
    return true;
  }
  return false;
}

void CollisionWorldDistanceField::updateDistanceObject(const std::string& id,
                                                       boost::shared_ptr<CollisionWorldDistanceField::DistanceFieldCacheEntry>& dfce,
                                                       std::vector<Eigen::Vector3d>& add_points,
                                                       std::vector<Eigen::Vector3d>& subtract_points)
{
  std::map<std::string, std::vector<PosedBodyPointDecompositionPtr> >::iterator cur_it = dfce->posed_body_point_decompositions_.find(id);
  if(cur_it != dfce->posed_body_point_decompositions_.end()) {
    for(unsigned int i = 0; i < cur_it->second.size(); i++) {
      subtract_points.insert(subtract_points.end(),
                             cur_it->second[i]->getCollisionPoints().begin(),
                             cur_it->second[i]->getCollisionPoints().end());
    }
  }
  ObjectConstPtr object = getObject(id);
  std::vector<PosedBodyPointDecompositionPtr> shape_points;
  for(unsigned int i = 0; i < object->shapes_.size(); i++) {
    
    BodyDecompositionConstPtr bd = getBodyDecompositionCacheEntry(object->shapes_[i],
                                                                  resolution_);
    
    shape_points.push_back(boost::make_shared<PosedBodyPointDecomposition>(bd, object->shape_poses_[i]));
    add_points.insert(add_points.end(),
                      shape_points.back()->getCollisionPoints().begin(),
                      shape_points.back()->getCollisionPoints().end());
  }
  dfce->posed_body_point_decompositions_[id] = shape_points;
}

boost::shared_ptr<CollisionWorldDistanceField::DistanceFieldCacheEntry>
CollisionWorldDistanceField::generateDistanceFieldCacheEntry()
{
  boost::shared_ptr<DistanceFieldCacheEntry> dfce(new DistanceFieldCacheEntry());
  if(use_signed_distance_field_)
  {
    dfce->distance_field_.reset(new distance_field::SignedPropagationDistanceField(size_x_,
                                                                                   size_y_, 
                                                                                   size_z_, 
                                                                                   resolution_, 
                                                                                   -(size_x_/2.0), 
                                                                                   -(size_y_/2.0), 
                                                                                   -(size_z_/2.0), 
                                                                                   max_propogation_distance_));
  }
  else
  {
    dfce->distance_field_.reset(new distance_field::PropagationDistanceField(size_x_,
                                                                             size_y_, 
                                                                             size_z_, 
                                                                             resolution_, 
                                                                             -(size_x_/2.0), 
                                                                             -(size_y_/2.0), 
                                                                             -(size_z_/2.0), 
                                                                             max_propogation_distance_));
  }
  std::vector<Eigen::Vector3d> add_points;
  std::vector<Eigen::Vector3d> subtract_points;
  for(std::map<std::string, ObjectPtr>::const_iterator it = objects_.begin();
      it != objects_.end();
      it++) 
  {
    updateDistanceObject(it->first, dfce, add_points, subtract_points);
  }
  dfce->distance_field_->addPointsToField(add_points);
  return dfce;
}

}
