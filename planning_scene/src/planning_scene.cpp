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

#include "planning_scene/planning_scene.h"
#include <collision_detection_fcl/collision_world.h>
#include <collision_detection_fcl/collision_robot.h>
#include <geometric_shapes/shape_operations.h>
#include <planning_models/conversions.h>
#include <octomap_msgs/conversions.h>
#include <ros/console.h>
#include <set>

namespace planning_scene
{

static const std::string COLLISION_MAP_NS = "_1_collision_map";
static const std::string OCTOMAP_NS = "_2_octomap";
static const std::string DEFAULT_SCENE_NAME = "(noname)";
}

bool planning_scene::PlanningScene::isEmpty(const moveit_msgs::PlanningScene &msg)
{
  return msg.name.empty() && msg.fixed_frame_transforms.empty() && msg.robot_state.multi_dof_joint_state.joint_names.empty() &&
      msg.robot_state.joint_state.name.empty() && msg.attached_collision_objects.empty() && msg.allowed_collision_matrix.entry_names.empty() &&
      msg.link_padding.empty() && msg.link_scale.empty() && msg.world.collision_objects.empty() && msg.world.octomap.data.empty() &&
      msg.world.collision_map.boxes.empty();
}

planning_scene::PlanningScenePtr planning_scene::PlanningScene::clone(const planning_scene::PlanningSceneConstPtr &scene)
{
  PlanningScenePtr result = scene->diff();
  result->decoupleParent();
  return result;
}

planning_scene::PlanningScenePtr planning_scene::PlanningScene::diff(void) const
{
  return PlanningScenePtr(new PlanningScene(shared_from_this()));
}

planning_scene::PlanningScenePtr planning_scene::PlanningScene::diff(const moveit_msgs::PlanningScene &msg) const
{
  planning_scene::PlanningScenePtr result = diff();
  result->setPlanningSceneDiffMsg(msg);
  return result;
}

planning_scene::PlanningScene::PlanningScene(void) : configured_(false)
{
  name_ = DEFAULT_SCENE_NAME;
  setCollisionDetectionTypes<collision_detection::CollisionWorldFCL, collision_detection::CollisionRobotFCL>();
}

planning_scene::PlanningScene::PlanningScene(const PlanningSceneConstPtr &parent) : parent_(parent), configured_(false)
{
  if (parent_)
  {
    collision_detection_allocator_.reset(parent_->collision_detection_allocator_->clone());
    if (parent_->isConfigured())
      configure(parent_->getUrdfModel(), parent_->getSrdfModel());
    if (!parent_->getName().empty())
      name_ = parent_->getName() + "+";
  }
  else
  {
    ROS_ERROR("NULL parent scene specified. Ignoring.");
    name_ = DEFAULT_SCENE_NAME; 
    setCollisionDetectionTypes<collision_detection::CollisionWorldFCL, collision_detection::CollisionRobotFCL>();
  }
}

bool planning_scene::PlanningScene::configure(const boost::shared_ptr<const urdf::ModelInterface> &urdf_model,
                                              const boost::shared_ptr<const srdf::Model> &srdf_model,
                                              const std::string &root_link)
{ 
  if (!parent_)
  {
    bool same = configured_ && urdf_model_ == urdf_model && srdf_model_ == srdf_model;
    if (!same || !kmodel_ || kmodel_->getRootLinkName() != root_link)
    {
      planning_models::KinematicModelPtr newModel;
      if (root_link.empty())
        newModel.reset(new planning_models::KinematicModel(urdf_model, srdf_model));
      else
        newModel.reset(new planning_models::KinematicModel(urdf_model, srdf_model, root_link));
      return configure(urdf_model, srdf_model, newModel);
    }
  }
  else
    return configure(urdf_model, srdf_model, planning_models::KinematicModelPtr());
  return isConfigured();
}

bool planning_scene::PlanningScene::configure(const boost::shared_ptr<const urdf::ModelInterface> &urdf_model,
                                              const boost::shared_ptr<const srdf::Model> &srdf_model,
                                              const planning_models::KinematicModelPtr &kmodel)
{
  if (!parent_)
  {
    // nothing other than perhaps the root link has changed since the last call to configure()
    bool same = configured_ && kmodel_ == kmodel;
    if (!same)
    {
      urdf_model_ = urdf_model;
      srdf_model_ = srdf_model;
      kmodel_ = kmodel;
      kmodel_const_ = kmodel_;
      ftf_.reset(new planning_models::Transforms(kmodel_->getModelFrame()));
      ftf_const_ = ftf_;
      
      if (kstate_)
      {
        // keep the same joint values, update the transforms if needed
        std::map<std::string, double> jsv;
        kstate_->getStateValues(jsv);
        kstate_.reset(new planning_models::KinematicState(kmodel_));
        kstate_->setStateValues(jsv);
      }
      else
      {
        kstate_.reset(new planning_models::KinematicState(kmodel_));
        kstate_->setToDefaultValues();
      }
      
      // no need to reset this if the scene was previously configured
      if (!acm_)
        acm_.reset(new collision_detection::AllowedCollisionMatrix());
      
      crobot_ = collision_detection_allocator_->allocateRobot(kmodel_);
      crobot_unpadded_ = collision_detection_allocator_->allocateRobot(kmodel_);
      crobot_const_ = crobot_;
      crobot_unpadded_const_ = crobot_unpadded_;
      
      // no need to change the world if it was previously configured;
      // there is a catch though: the frame for planning may have changed, if a different root link was specified;
      // however, this is direcly requested by the user
      if (!cworld_)
      {
        cworld_ = collision_detection_allocator_->allocateWorld();
        cworld_const_ = cworld_;
        
        colors_.reset(new std::map<std::string, std_msgs::ColorRGBA>());
      }
      
      configured_ = true;
    }
  }
  else
  {
    if (parent_->isConfigured())
    {
      if (srdf_model != parent_->getSrdfModel() || urdf_model != parent_->getUrdfModel())
        ROS_ERROR("Parent of planning scene is not constructed from the same robot model");

      // even if we have a parent, we do maintain a separate world representation, one that records changes
      // this is cheap however, because the worlds share the world representation
      cworld_ = collision_detection_allocator_->allocateWorld(parent_->getCollisionWorld());
      cworld_->recordChanges(true);
      cworld_const_ = cworld_;
      configured_ = true;
    }
    else
      ROS_ERROR("Parent is not configured yet");
  }

  return isConfigured();
}

void planning_scene::PlanningScene::clearDiffs(void)
{
  if (!parent_)
    return;

  // clear everything, reset the world
  cworld_ = collision_detection_allocator_->allocateWorld(parent_->getCollisionWorld());
  cworld_->recordChanges(true);
  cworld_const_ = cworld_;

  kmodel_.reset();
  kmodel_const_.reset();
  ftf_.reset();
  ftf_const_.reset();
  kstate_.reset();
  acm_.reset();
  crobot_.reset();
  crobot_const_.reset();
  crobot_unpadded_.reset();
  crobot_unpadded_const_.reset();
  colors_.reset();
}

void planning_scene::PlanningScene::pushDiffs(const PlanningScenePtr &scene)
{
  if (!parent_)
    return;

  if (ftf_)
    *scene->getTransforms() = *ftf_;

  if (kstate_)
    scene->getCurrentState() = *kstate_;

  if (acm_)
    scene->getAllowedCollisionMatrix() = *acm_;

  if (crobot_)
  {
    scene->getCollisionRobot()->setLinkPadding(crobot_->getLinkPadding());
    scene->getCollisionRobot()->setLinkScale(crobot_->getLinkScale());
  }

  if (cworld_->isRecordingChanges())
  {
    const std::vector<collision_detection::CollisionWorld::Change> &changes = cworld_->getChanges();
    if (!changes.empty())
    {
      collision_detection::CollisionWorldPtr w = scene->getCollisionWorld();
      for (std::size_t i = 0 ; i < changes.size() ; ++i)
        if (changes[i].type_ == collision_detection::CollisionWorld::Change::ADD)
        {
          collision_detection::CollisionWorld::ObjectConstPtr obj = cworld_->getObject(changes[i].id_);
          if (hasColor(changes[i].id_))
            scene->setColor(changes[i].id_, getColor(changes[i].id_));
          w->addToObject(obj->id_, obj->shapes_, obj->shape_poses_);
        }
        else
          if (changes[i].type_ == collision_detection::CollisionWorld::Change::REMOVE)
          {
            w->removeObject(changes[i].id_);
            scene->removeColor(changes[i].id_);
          }
          else
            ROS_ERROR("Unknown change on collision world");
    }
  }
}

double planning_scene::PlanningScene::distanceToCollisionUnpadded(const planning_models::KinematicState &kstate) const
{
  return getCollisionWorld()->distanceRobot(*getCollisionRobotUnpadded(), kstate);
}

double planning_scene::PlanningScene::distanceToCollisionUnpadded(const planning_models::KinematicState &kstate, const collision_detection::AllowedCollisionMatrix& acm) const
{
  return getCollisionWorld()->distanceRobot(*getCollisionRobotUnpadded(), kstate, acm);
}

double planning_scene::PlanningScene::distanceToCollision(const planning_models::KinematicState &kstate) const
{
  return getCollisionWorld()->distanceRobot(*getCollisionRobot(), kstate);
}

double planning_scene::PlanningScene::distanceToCollision(const planning_models::KinematicState &kstate, const collision_detection::AllowedCollisionMatrix& acm) const
{
  return getCollisionWorld()->distanceRobot(*getCollisionRobot(), kstate, acm);
}

void planning_scene::PlanningScene::checkCollision(const collision_detection::CollisionRequest& req, collision_detection::CollisionResult &res) const
{
  checkCollision(req, res, getCurrentState());
}

void planning_scene::PlanningScene::checkSelfCollision(const collision_detection::CollisionRequest& req, collision_detection::CollisionResult &res) const
{
  checkSelfCollision(req, res, getCurrentState());
}

void planning_scene::PlanningScene::checkCollision(const collision_detection::CollisionRequest& req, collision_detection::CollisionResult &res,
                                                   const planning_models::KinematicState &kstate) const
{
  // check collision with the world using the padded version
  if (parent_)
    getCollisionWorld()->checkRobotCollision(req, res, *getCollisionRobot(), kstate, getAllowedCollisionMatrix());
  else
    getCollisionWorld()->checkRobotCollision(req, res, *crobot_, kstate, *acm_);
  
  if (!res.collision || (req.contacts && res.contacts.size() < req.max_contacts))
  {
    // do self-collision checking with the unpadded version of the robot
    if (parent_)
      getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, getAllowedCollisionMatrix());
    else
      getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, *acm_);
  }
}

void planning_scene::PlanningScene::checkSelfCollision(const collision_detection::CollisionRequest& req, collision_detection::CollisionResult &res,
                                                       const planning_models::KinematicState &kstate) const
{
  // do self-collision checking with the unpadded version of the robot
  getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, getAllowedCollisionMatrix());
}

void planning_scene::PlanningScene::checkCollision(const collision_detection::CollisionRequest& req,
                                                   collision_detection::CollisionResult &res,
                                                   const planning_models::KinematicState &kstate,
                                                   const collision_detection::AllowedCollisionMatrix& acm) const
{
  // check collision with the world using the padded version
  getCollisionWorld()->checkRobotCollision(req, res, *getCollisionRobot(), kstate, acm);
  
  // do self-collision checking with the unpadded version of the robot
  if (!res.collision || (req.contacts && res.contacts.size() < req.max_contacts))
    getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, acm);
}

void planning_scene::PlanningScene::checkCollisionUnpadded(const collision_detection::CollisionRequest& req,
							   collision_detection::CollisionResult &res) const
{
  return checkCollisionUnpadded(req, res, getCurrentState(), getAllowedCollisionMatrix());
}

void planning_scene::PlanningScene::checkCollisionUnpadded(const collision_detection::CollisionRequest& req,
							   collision_detection::CollisionResult &res,
							   const planning_models::KinematicState &kstate) const
{
  return checkCollisionUnpadded(req, res, kstate, getAllowedCollisionMatrix());
}

void planning_scene::PlanningScene::checkCollisionUnpadded(const collision_detection::CollisionRequest& req,
                                                           collision_detection::CollisionResult &res,
                                                           const planning_models::KinematicState &kstate,
                                                           const collision_detection::AllowedCollisionMatrix& acm) const
{
  // check collision with the world using the padded version
  getCollisionWorld()->checkRobotCollision(req, res, *getCollisionRobotUnpadded(), kstate, acm);

  // do self-collision checking with the unpadded version of the robot
  if (!res.collision || (req.contacts && res.contacts.size() < req.max_contacts))
  {
    getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, acm);
  }
}

void planning_scene::PlanningScene::checkSelfCollision(const collision_detection::CollisionRequest& req,
                                                       collision_detection::CollisionResult &res,
                                                       const planning_models::KinematicState &kstate,
                                                       const collision_detection::AllowedCollisionMatrix& acm) const
{
  // do self-collision checking with the unpadded version of the robot
  getCollisionRobotUnpadded()->checkSelfCollision(req, res, kstate, acm);
}

const collision_detection::CollisionRobotPtr& planning_scene::PlanningScene::getCollisionRobot(void)
{
  if (!crobot_)
  {
    crobot_ = collision_detection_allocator_->allocateRobot(parent_->getCollisionRobot());
    crobot_const_ = crobot_;
  }
  return crobot_;
}

planning_models::KinematicState& planning_scene::PlanningScene::getCurrentState(void)
{
  if (!kstate_)
    kstate_.reset(new planning_models::KinematicState(parent_->getCurrentState()));
  return *kstate_;
}

collision_detection::AllowedCollisionMatrix& planning_scene::PlanningScene::getAllowedCollisionMatrix(void)
{
  if (!acm_)
    acm_.reset(new collision_detection::AllowedCollisionMatrix(parent_->getAllowedCollisionMatrix()));
  return *acm_;
}

const planning_models::TransformsPtr& planning_scene::PlanningScene::getTransforms(void)
{
  if (!ftf_)
  {
    ftf_.reset(new planning_models::Transforms(*parent_->getTransforms()));
    ftf_const_ = ftf_;
  }
  return ftf_;
}

void planning_scene::PlanningScene::getPlanningSceneDiffMsg(moveit_msgs::PlanningScene &scene) const
{
  scene.name = name_;
  scene.robot_model_root = getKinematicModel()->getRootLinkName();
  scene.robot_model_name = getKinematicModel()->getName();
  scene.is_diff = true;
  
  if (ftf_)
    ftf_->getTransforms(scene.fixed_frame_transforms);
  else
    scene.fixed_frame_transforms.clear();

  if (kstate_)
  {
    planning_models::kinematicStateToRobotState(*kstate_, scene.robot_state);
    getPlanningSceneMsgAttachedBodies(scene);
  }
  else
  {
    scene.robot_state = moveit_msgs::RobotState();
    scene.attached_collision_objects.clear();
  }

  if (acm_)
    acm_->getMessage(scene.allowed_collision_matrix);
  else
    scene.allowed_collision_matrix = moveit_msgs::AllowedCollisionMatrix();

  if (crobot_)
  {
    crobot_->getPadding(scene.link_padding);
    crobot_->getScale(scene.link_scale);
  }
  else
  {
    scene.link_padding.clear();
    scene.link_scale.clear();
  }

  if (cworld_->isRecordingChanges())
  {
    scene.world.collision_objects.clear();
    scene.world.collision_map = moveit_msgs::CollisionMap();
    scene.world.octomap = octomap_msgs::OctomapBinary();
    
    bool do_cmap = false;
    bool do_omap = false;
    const std::vector<collision_detection::CollisionWorld::Change> &changes = cworld_->getChanges();
    for (std::size_t i = 0 ; i < changes.size() ; ++i)
      if (changes[i].id_ == COLLISION_MAP_NS)
        do_cmap = true;
      else
      if (changes[i].id_ == OCTOMAP_NS)
        do_omap = true;
      else
      {
        if (changes[i].type_ == collision_detection::CollisionWorld::Change::ADD)
        {
          addPlanningSceneMsgCollisionObject(scene, changes[i].id_);
        }
        else
          if (changes[i].type_ == collision_detection::CollisionWorld::Change::REMOVE)
          {
            moveit_msgs::CollisionObject co;
            co.header.frame_id = getPlanningFrame();
            co.id = changes[i].id_;
            co.operation = moveit_msgs::CollisionObject::REMOVE;
            scene.world.collision_objects.push_back(co);
          }
          else
            ROS_ERROR("Unknown change on collision world");
      }
    if (do_cmap)
      getPlanningSceneMsgCollisionMap(scene);
    if (do_omap)
      getPlanningSceneMsgOctomap(scene);
  }
}

namespace planning_scene
{         
class ShapeVisitorAddToCollisionObject : public boost::static_visitor<void>
{
public:
  
  ShapeVisitorAddToCollisionObject(moveit_msgs::CollisionObject *obj) :
    boost::static_visitor<void>(), obj_(obj)
  {
  }

  void setPoseMessage(const geometry_msgs::Pose *pose)
  {
    pose_ = pose;
  }
    
  void operator()(const shape_msgs::Plane &shape_msg) const
  {
    obj_->planes.push_back(shape_msg);   
    obj_->plane_poses.push_back(*pose_);
  }
  
  void operator()(const shape_msgs::Mesh &shape_msg) const
  {
    obj_->meshes.push_back(shape_msg);
    obj_->mesh_poses.push_back(*pose_);
  }
  
  void operator()(const shape_msgs::SolidPrimitive &shape_msg) const
  {
    obj_->primitives.push_back(shape_msg);
    obj_->primitive_poses.push_back(*pose_);
  }
  
private:
  
  moveit_msgs::CollisionObject *obj_;
  const geometry_msgs::Pose *pose_;
};
}


void planning_scene::PlanningScene::getPlanningSceneMsgAttachedBodies(moveit_msgs::PlanningScene &scene) const
{
  scene.attached_collision_objects.clear();
  std::vector<const planning_models::KinematicState::AttachedBody*> ab;
  getCurrentState().getAttachedBodies(ab);

  for (std::size_t i = 0 ; i < ab.size() ; ++i)
  {
    moveit_msgs::AttachedCollisionObject aco;
    aco.link_name = ab[i]->getAttachedLinkName();
    const std::set<std::string> &touch_links = ab[i]->getTouchLinks();
    for (std::set<std::string>::const_iterator it = touch_links.begin() ; it != touch_links.end() ; ++it)
      aco.touch_links.push_back(*it);
    aco.object.header.frame_id = aco.link_name;
    aco.object.id = ab[i]->getName();
    aco.object.operation = moveit_msgs::CollisionObject::ADD;
    const std::vector<shapes::ShapeConstPtr>& ab_shapes = ab[i]->getShapes();
    const std::vector<Eigen::Affine3d>& ab_tf = ab[i]->getFixedTransforms();
    ShapeVisitorAddToCollisionObject sv(&aco.object);
    for (std::size_t j = 0 ; j < ab_shapes.size() ; ++j)
    {
      shapes::ShapeMsg sm;
      if (constructMsgFromShape(ab_shapes[j].get(), sm))
      {
        geometry_msgs::Pose p;
        planning_models::msgFromPose(ab_tf[j], p);
        sv.setPoseMessage(&p);
        boost::apply_visitor(sv, sm);
      }
    }
    if (!aco.object.primitives.empty() || !aco.object.meshes.empty() || !aco.object.planes.empty())
    {
      scene.attached_collision_objects.push_back(aco);
      if (hasColor(aco.object.id))
        scene.attached_collision_objects_colors.push_back(getColor(aco.object.id));
    }
  }
}

bool planning_scene::PlanningScene::getCollisionObjectMsg(const std::string& ns, moveit_msgs::CollisionObject& co) const 
{
  co = moveit_msgs::CollisionObject();
  co.header.frame_id = getPlanningFrame();
  co.id = ns;
  co.operation = moveit_msgs::CollisionObject::ADD;
  collision_detection::CollisionWorld::ObjectConstPtr obj = getCollisionWorld()->getObject(ns);
  if (!obj) 
    return false;  
  ShapeVisitorAddToCollisionObject sv(&co);
  for (std::size_t j = 0 ; j < obj->shapes_.size() ; ++j)
  {
    shapes::ShapeMsg sm;
    if (constructMsgFromShape(obj->shapes_[j].get(), sm))
    {
      geometry_msgs::Pose p;
      planning_models::msgFromPose(obj->shape_poses_[j], p);
      sv.setPoseMessage(&p);
      boost::apply_visitor(sv, sm);
    }
  }
  return true;
}

void planning_scene::PlanningScene::getCollisionObjectMarkers(visualization_msgs::MarkerArray& arr,
                                                              const std_msgs::ColorRGBA& default_color,
                                                              const std::string& ns,
                                                              const ros::Duration& lifetime) const
{
  const std::vector<std::string> &ids = getCollisionWorld()->getObjectIds();
  for (std::size_t i = 0 ; i < ids.size() ; ++i)
  {
    collision_detection::CollisionWorld::ObjectConstPtr o = getCollisionWorld()->getObject(ids[i]);
    std_msgs::ColorRGBA color = default_color;
    if (hasColor(ids[i]))
      color = getColor(ids[i]);
    
    unsigned int tot_count = 0;
    for(std::size_t j = 0; j < o->shapes_.size() ; ++j, ++tot_count)
    {
      visualization_msgs::Marker mk;
      shapes::constructMarkerFromShape(o->shapes_[j].get(), mk, true);
      mk.header.frame_id = getPlanningFrame();
      mk.header.stamp = ros::Time::now();
      mk.color = color;
      if (ns.empty())
      {
        mk.ns = ids[i];
        mk.id = j;
      } 
      else
      {
        mk.ns = ns;
        mk.id = tot_count;
      }
      mk.action = visualization_msgs::Marker::ADD;
      planning_models::msgFromPose(o->shape_poses_[j], mk.pose);
      mk.lifetime = lifetime;
      arr.markers.push_back(mk);
    }
  }
}

void planning_scene::PlanningScene::addPlanningSceneMsgCollisionObject(moveit_msgs::PlanningScene &scene, const std::string &ns) const
{
  moveit_msgs::CollisionObject co;
  if (getCollisionObjectMsg(ns, co))
  {
    if (!co.primitives.empty() || !co.meshes.empty() || !co.planes.empty())
    {
      scene.world.collision_objects.push_back(co);
      if (hasColor(co.id))
        scene.world.colors.push_back(getColor(co.id));
    }
  }
}

void planning_scene::PlanningScene::getPlanningSceneMsgCollisionObjects(moveit_msgs::PlanningScene &scene) const
{
  scene.world.collision_objects.clear();
  const std::vector<std::string> &ns = getCollisionWorld()->getObjectIds();
  for (std::size_t i = 0 ; i < ns.size() ; ++i)
    if (ns[i] != COLLISION_MAP_NS)
      addPlanningSceneMsgCollisionObject(scene, ns[i]);
}

void planning_scene::PlanningScene::getPlanningSceneMsgCollisionMap(moveit_msgs::PlanningScene &scene) const
{
  scene.world.collision_map.header.frame_id = getPlanningFrame();
  scene.world.collision_map.boxes.clear();
  if (getCollisionWorld()->hasObject(COLLISION_MAP_NS))
  {
    collision_detection::CollisionWorld::ObjectConstPtr map = getCollisionWorld()->getObject(COLLISION_MAP_NS);
    for (std::size_t i = 0 ; i < map->shapes_.size() ; ++i)
    {
      const shapes::Box *b = static_cast<const shapes::Box*>(map->shapes_[i].get());
      moveit_msgs::OrientedBoundingBox obb;
      obb.extents.x = b->size[0]; obb.extents.y = b->size[1]; obb.extents.z = b->size[2];
      planning_models::msgFromPose(map->shape_poses_[i], obb.pose);
      scene.world.collision_map.boxes.push_back(obb);
    }
  }
}

void planning_scene::PlanningScene::getPlanningSceneMsgOctomap(moveit_msgs::PlanningScene &scene) const
{  
  scene.world.octomap.header.frame_id = getPlanningFrame();
  scene.world.octomap.data.clear();
  if (getCollisionWorld()->hasObject(OCTOMAP_NS))
  {
    collision_detection::CollisionWorld::ObjectConstPtr map = getCollisionWorld()->getObject(OCTOMAP_NS);
    for (std::size_t i = 0 ; i < map->shapes_.size() ; ++i)
    {
      const shapes::Box *b = static_cast<const shapes::Box*>(map->shapes_[i].get());
      moveit_msgs::OrientedBoundingBox obb;
      obb.extents.x = b->size[0]; obb.extents.y = b->size[1]; obb.extents.z = b->size[2];
      planning_models::msgFromPose(map->shape_poses_[i], obb.pose);
      // we do not have previous information about the octomap here, so we can only create a CollisionMap
      // This needs to be changed at some point
      scene.world.collision_map.boxes.push_back(obb);
    }
  }
}

void planning_scene::PlanningScene::getPlanningSceneMsg(moveit_msgs::PlanningScene &scene) const
{
  scene.name = name_;
  scene.is_diff = false;
  scene.robot_model_root = getKinematicModel()->getRootLinkName();
  scene.robot_model_name = getKinematicModel()->getName();
  getTransforms()->getTransforms(scene.fixed_frame_transforms);
  planning_models::kinematicStateToRobotState(getCurrentState(), scene.robot_state);
  getAllowedCollisionMatrix().getMessage(scene.allowed_collision_matrix);
  getCollisionRobot()->getPadding(scene.link_padding);
  getCollisionRobot()->getScale(scene.link_scale);

  // add collision objects
  getPlanningSceneMsgCollisionObjects(scene);

  // add the attached bodies
  getPlanningSceneMsgAttachedBodies(scene);

  // get the octomap
  getPlanningSceneMsgOctomap(scene);

  // get the collision map
  getPlanningSceneMsgCollisionMap(scene);
}

void planning_scene::PlanningScene::setCurrentState(const moveit_msgs::RobotState &state)
{
  if (parent_)
  {
    if (!kstate_)
      kstate_.reset(new planning_models::KinematicState(parent_->getCurrentState()));
    planning_models::robotStateToKinematicState(*getTransforms(), state, *kstate_);
  }
  else
    planning_models::robotStateToKinematicState(*ftf_, state, *kstate_);
}

void planning_scene::PlanningScene::setCurrentState(const planning_models::KinematicState &state)
{
  if (!kstate_)
    kstate_.reset(new planning_models::KinematicState(getKinematicModel()));
  *kstate_ = state;
}

void planning_scene::PlanningScene::decoupleParent(void)
{
  if (!parent_)
    return;
  if (parent_->isConfigured())
  {
    urdf_model_ = parent_->urdf_model_;
    srdf_model_ = parent_->srdf_model_;
    kmodel_ = parent_->kmodel_;
    kmodel_const_ = kmodel_;

    if (!ftf_)
    {
      ftf_.reset(new planning_models::Transforms(*parent_->getTransforms()));
      ftf_const_ = ftf_;
    }

    if (!kstate_)
      kstate_.reset(new planning_models::KinematicState(parent_->getCurrentState()));

    if (!acm_)
      acm_.reset(new collision_detection::AllowedCollisionMatrix(parent_->getAllowedCollisionMatrix()));

    if (!crobot_unpadded_)
    {
      crobot_unpadded_ = collision_detection_allocator_->allocateRobot(parent_->getCollisionRobotUnpadded());
      crobot_unpadded_const_ = crobot_unpadded_;
    }
    if (!crobot_)
    {
      crobot_ = collision_detection_allocator_->allocateRobot(parent_->getCollisionRobot());
      crobot_const_ = crobot_;
    }

    if (!cworld_)
    {
      cworld_ = collision_detection_allocator_->allocateWorld(parent_->getCollisionWorld());
      cworld_const_ = cworld_;
    }
    else
    {
      cworld_->recordChanges(false);
      cworld_->clearChanges();
    }

    if (!colors_)
    {
      std::map<std::string, std_msgs::ColorRGBA> kc;
      parent_->getKnownColors(kc);
      colors_.reset(new std::map<std::string, std_msgs::ColorRGBA>(kc));
    }
    else
    {
      std::map<std::string, std_msgs::ColorRGBA> kc;
      parent_->getKnownColors(kc);
      for (std::map<std::string, std_msgs::ColorRGBA>::const_iterator it = kc.begin() ; it != kc.end() ; ++it)
        if (colors_->find(it->first) == colors_->end())
          (*colors_)[it->first] = it->second;
    }
        
    configured_ = true;
  }

  parent_.reset();
}

void planning_scene::PlanningScene::setPlanningSceneDiffMsg(const moveit_msgs::PlanningScene &scene)
{
  ROS_DEBUG("Adding planning scene diff");
  if (!scene.name.empty())
    name_ = scene.name;
  
  if (!scene.robot_model_name.empty() && scene.robot_model_name != getKinematicModel()->getName())
    ROS_WARN("Setting the scene for model '%s' but model '%s' is loaded.", scene.robot_model_name.c_str(), getKinematicModel()->getName().c_str());
  
  if (!scene.robot_model_root.empty() && scene.robot_model_root != getKinematicModel()->getRootLinkName())
    ROS_WARN("Setting scene with robot model root '%s' but the current planning scene uses link '%s' as root.", scene.robot_model_root.c_str(), getKinematicModel()->getRootLinkName().c_str());
  
  // there is at least one transform in the list of fixed transform: from model frame to itself;
  // if the list is empty, then nothing has been set
  if (!scene.fixed_frame_transforms.empty())
  {
    if (!ftf_)
    {
      ftf_.reset(new planning_models::Transforms(getKinematicModel()->getModelFrame()));
      ftf_const_ = ftf_;
    }
    ftf_->setTransforms(scene.fixed_frame_transforms);
  }

  // if at least some joints have been specified, we set them
  if (!scene.robot_state.multi_dof_joint_state.joint_names.empty() ||
      !scene.robot_state.joint_state.name.empty())
    setCurrentState(scene.robot_state);

  if (!scene.attached_collision_objects.empty())
    for (std::size_t i = 0 ; i < scene.attached_collision_objects.size() ; ++i)
    {
      processAttachedCollisionObjectMsg(scene.attached_collision_objects[i]);
      if (scene.attached_collision_objects_colors.size() > i)
        setColor(scene.attached_collision_objects[i].object.id, scene.attached_collision_objects_colors[i]);
    }
  

  // if at least some links are mentioned in the allowed collision matrix, then we have an update
  if (!scene.allowed_collision_matrix.entry_names.empty())
    acm_.reset(new collision_detection::AllowedCollisionMatrix(scene.allowed_collision_matrix));

  if (!scene.link_padding.empty() || !scene.link_scale.empty())
  {
    if (!crobot_)
    { // this means we have a parent too
      crobot_ = collision_detection_allocator_->allocateRobot(parent_->getCollisionRobot());
      crobot_const_ = crobot_;
    }
    crobot_->setPadding(scene.link_padding);
    crobot_->setScale(scene.link_scale);
  }
  
  for (std::size_t i = 0 ; i < scene.world.collision_objects.size() ; ++i)
  {
    processCollisionObjectMsg(scene.world.collision_objects[i]);
    if (scene.world.colors.size() >= scene.world.collision_objects.size() && scene.world.collision_objects[i].operation == moveit_msgs::CollisionObject::ADD)
      setColor(scene.world.collision_objects[i].id, scene.world.colors[i]);
    else
      removeColor(scene.world.collision_objects[i].id);
  }
  
  processOctomapMsg(scene.world.octomap);
  processCollisionMapMsg(scene.world.collision_map);
}

void planning_scene::PlanningScene::setPlanningSceneMsg(const moveit_msgs::PlanningScene &scene)
{
  ROS_DEBUG("Setting new planning scene");
  name_ = scene.name;
  
  if (!scene.robot_model_name.empty() && scene.robot_model_name != getKinematicModel()->getName())
    ROS_WARN("Setting the scene for model '%s' but model '%s' is loaded.", scene.robot_model_name.c_str(), getKinematicModel()->getName().c_str());

  if (parent_)
  {
    // if we have a parent, but we set a new planning scene, then we do not care about the parent any more
    // and we no longer represent the scene as a diff
    urdf_model_ = parent_->urdf_model_;
    srdf_model_ = parent_->srdf_model_;
    kmodel_ = parent_->kmodel_;
    kmodel_const_ = kmodel_;

    if (!ftf_)
    {
      ftf_.reset(new planning_models::Transforms(kmodel_->getModelFrame()));
      ftf_const_ = ftf_;
    }

    if (!kstate_)
      kstate_.reset(new planning_models::KinematicState(kmodel_));

    if (!crobot_)
    {
      crobot_ = collision_detection_allocator_->allocateRobot(kmodel_);
      crobot_const_ = crobot_;
    }
    crobot_unpadded_ = collision_detection_allocator_->allocateRobot(kmodel_);
    crobot_unpadded_const_ = crobot_unpadded_;

    cworld_->recordChanges(false);
    cworld_->clearChanges();

    configured_ = true;
    parent_.reset();
  }
  // re-parent the robot model if needed
  if (!scene.robot_model_root.empty() && scene.robot_model_root != getKinematicModel()->getRootLinkName())
    configure(urdf_model_, srdf_model_, scene.robot_model_root);
  
  ftf_->setTransforms(scene.fixed_frame_transforms);
  setCurrentState(scene.robot_state);
  acm_.reset(new collision_detection::AllowedCollisionMatrix(scene.allowed_collision_matrix));
  crobot_->setPadding(scene.link_padding);
  crobot_->setScale(scene.link_scale);
  cworld_->clearObjects(); 
  colors_.reset(new std::map<std::string, std_msgs::ColorRGBA>());
  for (std::size_t i = 0 ; i < scene.world.collision_objects.size() ; ++i)
  {
    processCollisionObjectMsg(scene.world.collision_objects[i]);
    if (scene.world.colors.size() >= scene.world.collision_objects.size() && scene.world.collision_objects[i].operation == moveit_msgs::CollisionObject::ADD)
      setColor(scene.world.collision_objects[i].id, scene.world.colors[i]);
    else
      removeColor(scene.world.collision_objects[i].id);
  }
  
  kstate_->clearAttachedBodies();
  for (std::size_t i = 0 ; i < scene.attached_collision_objects.size() ; ++i)
  {
    processAttachedCollisionObjectMsg(scene.attached_collision_objects[i]);
    if (scene.attached_collision_objects_colors.size() > i)
      setColor(scene.attached_collision_objects[i].object.id, scene.attached_collision_objects_colors[i]);
  }
  
  processOctomapMsg(scene.world.octomap);
  processCollisionMapMsg(scene.world.collision_map);
}

void planning_scene::PlanningScene::usePlanningSceneMsg(const moveit_msgs::PlanningScene &scene)
{
  if (scene.is_diff)
    setPlanningSceneDiffMsg(scene);
  else
    setPlanningSceneMsg(scene);
}

void planning_scene::PlanningScene::processCollisionMapMsg(const moveit_msgs::CollisionMap &map)
{
  // each collision map replaces any previous one
  cworld_->removeObject(COLLISION_MAP_NS);

  if (map.boxes.empty())
    return;
  const Eigen::Affine3d &t = getTransforms()->getTransform(getCurrentState(), map.header.frame_id);
  for (std::size_t i = 0 ; i < map.boxes.size() ; ++i)
  {
    Eigen::Affine3d p;
    if(!planning_models::poseFromMsg(map.boxes[i].pose, p))
    {
      ROS_ERROR("Failed to convert from pose message to Eigen Affine3f");
    }
 
    shapes::Shape *s = new shapes::Box(map.boxes[i].extents.x, map.boxes[i].extents.y, map.boxes[i].extents.z);
    cworld_->addToObject(COLLISION_MAP_NS, shapes::ShapeConstPtr(s), t * p);
  }
}

void planning_scene::PlanningScene::processOctomapMsg(const octomap_msgs::OctomapBinary &map)
{
  // each octomap replaces any previous one
  cworld_->removeObject(OCTOMAP_NS);

  if (map.data.empty())
	  return;
  octomap::OcTree om(0.1); /// \TODO this should be a parameter maybe? 
  octomap::octomapMsgToMap(map, om);
  const Eigen::Affine3d &t = getTransforms()->getTransform(getCurrentState(), map.header.frame_id); 
  Eigen::Affine3d p; p.setIdentity();
  for (octomap::OcTree::iterator it = om.begin(om.getTreeDepth()), end = om.end(); it != end; ++it)
  {
    if (om.isNodeOccupied(*it))
    {      
      double size = it.getSize();
      shapes::Shape *s = new shapes::Box(size, size, size);
      p.translation().x() = it.getX();
      p.translation().y() = it.getY();
      p.translation().z() = it.getZ();
      cworld_->addToObject(OCTOMAP_NS, shapes::ShapeConstPtr(s), t * p);
    }
  }
}

bool planning_scene::PlanningScene::processAttachedCollisionObjectMsg(const moveit_msgs::AttachedCollisionObject &object)
{
  if (!getKinematicModel()->hasLinkModel(object.link_name))
  {
    ROS_ERROR("Unable to attach a body to link '%s' (link not found)", object.link_name.c_str());
    return false;
  }

  if (object.object.id == COLLISION_MAP_NS)
  {
    ROS_ERROR("The ID '%s' cannot be used for collision objects (name reserved)", COLLISION_MAP_NS.c_str());
    return false;
  }

  if (!kstate_) // there must be a parent in this case
    kstate_.reset(new planning_models::KinematicState(parent_->getCurrentState()));

  if (object.object.operation == moveit_msgs::CollisionObject::ADD)
  {
    if (object.object.primitives.size() != object.object.primitive_poses.size())
    {
      ROS_ERROR("Number of primitive shapes does not match number of poses in attached collision object message");
      return false;
    }

    if (object.object.meshes.size() != object.object.mesh_poses.size())
    {
      ROS_ERROR("Number of meshes does not match number of poses in attached collision object message");
      return false;
    }

    if (object.object.planes.size() != object.object.plane_poses.size())
    {
      ROS_ERROR("Number of planes does not match number of poses in attached collision object message");
      return false;
    }

    planning_models::KinematicState::LinkState *ls = kstate_->getLinkState(object.link_name);
    if (ls)
    {
      std::vector<shapes::ShapeConstPtr> shapes;
      std::vector<Eigen::Affine3d> poses;

      // we need to add some shapes; if the message is empty, maybe the object is already in the world
      if (object.object.primitives.empty() && object.object.meshes.empty() && object.object.planes.empty())
      {
        if (cworld_->hasObject(object.object.id))
        {
          ROS_DEBUG("Attaching world object '%s' to link '%s'", object.object.id.c_str(), object.link_name.c_str());

          // extract the shapes from the world
          collision_detection::CollisionWorld::ObjectConstPtr obj = cworld_->getObject(object.object.id);
          shapes = obj->shapes_;
          poses = obj->shape_poses_;
          // remove the pointer to the objects from the collision world
          cworld_->removeObject(object.object.id);
          
          // need to transform poses to the link frame
          const Eigen::Affine3d &i_t = ls->getGlobalLinkTransform().inverse();
          for (std::size_t i = 0 ; i < poses.size() ; ++i)
            poses[i] = i_t * poses[i];
        }
        else
        {
          ROS_ERROR("Attempting to attach object '%s' to link '%s' but no geometry specified and such an object does not exist in the collision world",
                    object.object.id.c_str(), object.link_name.c_str());
          return false;
        }
      }
      else
      {
        // we clear the world objects with the same name, since we got an update on their geometry
        if (cworld_->hasObject(object.object.id))
        {
          ROS_DEBUG("Removing wold object with the same name as newly attached object: '%s'", object.object.id.c_str());
          cworld_->removeObject(object.object.id);
        }

        for (std::size_t i = 0 ; i < object.object.primitives.size() ; ++i)
        {
          shapes::Shape *s = shapes::constructShapeFromMsg(object.object.primitives[i]);
          if (s)
          {
            Eigen::Affine3d p;
            if(!planning_models::poseFromMsg(object.object.primitive_poses[i], p))
            {
              ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.object.id.c_str());
              return false;
            }

            shapes.push_back(shapes::ShapeConstPtr(s));
            poses.push_back(p);
          }
        }       
        for (std::size_t i = 0 ; i < object.object.meshes.size() ; ++i)
        {
          shapes::Shape *s = shapes::constructShapeFromMsg(object.object.meshes[i]);
          if (s)
          {
            Eigen::Affine3d p;
            if(!planning_models::poseFromMsg(object.object.mesh_poses[i], p))
            {
              ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.object.id.c_str());
              return false;
            }

            shapes.push_back(shapes::ShapeConstPtr(s));
            poses.push_back(p);
          }
        }
        for (std::size_t i = 0 ; i < object.object.planes.size() ; ++i)
        {
          shapes::Shape *s = shapes::constructShapeFromMsg(object.object.planes[i]);
          if (s)
          {
            Eigen::Affine3d p;
            if(!planning_models::poseFromMsg(object.object.plane_poses[i], p))
            {
              ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.object.id.c_str());
              return false;
            }

            shapes.push_back(shapes::ShapeConstPtr(s));
            poses.push_back(p);
          }
        }

        // transform poses to link frame
        if (object.object.header.frame_id != object.link_name)
        {
          const Eigen::Affine3d &t = ls->getGlobalLinkTransform().inverse() * getTransforms()->getTransform(*kstate_, object.object.header.frame_id);
          for (std::size_t i = 0 ; i < poses.size() ; ++i)
            poses[i] = t * poses[i];
        }
      }

      if (shapes.empty())
      {
        ROS_ERROR("There is no geometry to attach to link '%s' as part of attached body '%s'", object.link_name.c_str(), object.object.id.c_str());
        return false;
      }

      // there should not exist an attached object with this name
      if (ls->clearAttachedBody(object.object.id))
        ROS_WARN("The kinematic state already had an object named '%s' attached to link '%s'. The object was replaced.",
                 object.object.id.c_str(), object.link_name.c_str());
      ls->attachBody(object.object.id, shapes, poses, object.touch_links);      
      ROS_DEBUG("Attached object '%s' to link '%s'", object.object.id.c_str(), object.link_name.c_str());
      return true;
    }
    else
      ROS_FATAL("Kinematic state is not compatible with kinematic model");
  }
  else
    if (object.object.operation == moveit_msgs::CollisionObject::REMOVE)
    {
      planning_models::KinematicState::LinkState *ls = kstate_->getLinkState(object.link_name);
      if (ls)
      {
        const planning_models::KinematicState::AttachedBody *ab = ls->getAttachedBody(object.object.id);
        if (ab)
        {
          std::vector<shapes::ShapeConstPtr> shapes = ab->getShapes();
          std::vector<Eigen::Affine3d> poses = ab->getGlobalCollisionBodyTransforms();
          ls->clearAttachedBody(object.object.id);
          
          if (cworld_->hasObject(object.object.id))
            ROS_WARN("The collision world already has an object with the same name as the body about to be detached. NOT adding the detached body '%s' to the collision world.", object.object.id.c_str());
          else
          {
            cworld_->addToObject(object.object.id, shapes, poses);
            ROS_DEBUG("Detached object '%s' from link '%s' and added it back in the collision world", object.object.id.c_str(), object.link_name.c_str());
          }
          
          return true;
        }
        else
          ROS_ERROR("No object named '%s' is attached to link '%s'", object.object.id.c_str(), object.link_name.c_str());
      }
      else
        ROS_FATAL("Kinematic state is not compatible with kinematic model");
    }
    else
      ROS_ERROR("Unknown collision object operation: %d", object.object.operation);
  return false;
}

bool planning_scene::PlanningScene::processCollisionObjectMsg(const moveit_msgs::CollisionObject &object)
{
  if (object.id == COLLISION_MAP_NS)
  {
    ROS_ERROR("The ID '%s' cannot be used for collision objects (name reserved)", COLLISION_MAP_NS.c_str());
    return false;
  }
  //  collision_detection::AllowedCollisionMatrix& acm = getAllowedCollisionMatrix();

  if (object.operation == moveit_msgs::CollisionObject::ADD)
  {
    if (object.primitives.empty() && object.meshes.empty() && object.planes.empty())
    {
      ROS_ERROR("There are no shapes specified in the collision object message");
      return false;
    }
    
    if (object.primitives.size() != object.primitive_poses.size())
    {
      ROS_ERROR("Number of primitive shapes does not match number of poses in collision object message");
      return false;
    }

    if (object.meshes.size() != object.mesh_poses.size())
    {
      ROS_ERROR("Number of meshes does not match number of poses in collision object message");
      return false;
    }

    if (object.planes.size() != object.plane_poses.size())
    {
      ROS_ERROR("Number of planes does not match number of poses in collision object message");
      return false;
    }
    
    const Eigen::Affine3d &t = getTransforms()->getTransform(getCurrentState(), object.header.frame_id);
    for (std::size_t i = 0 ; i < object.primitives.size() ; ++i)
    {
      shapes::Shape *s = shapes::constructShapeFromMsg(object.primitives[i]);
      if (s)
      {
        Eigen::Affine3d p; 
        if(!planning_models::poseFromMsg(object.primitive_poses[i], p))
        {
          ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.id.c_str());
          return false;
        }
        cworld_->addToObject(object.id, shapes::ShapeConstPtr(s), t * p);
      }
    }
    for (std::size_t i = 0 ; i < object.meshes.size() ; ++i)
    {
      shapes::Shape *s = shapes::constructShapeFromMsg(object.meshes[i]);
      if (s)
      {
        Eigen::Affine3d p; 
        if(!planning_models::poseFromMsg(object.mesh_poses[i], p))
        {
          ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.id.c_str());
          return false;
        }
        cworld_->addToObject(object.id, shapes::ShapeConstPtr(s), t * p);
      }
    }
    for (std::size_t i = 0 ; i < object.planes.size() ; ++i)
    {
      shapes::Shape *s = shapes::constructShapeFromMsg(object.planes[i]);
      if (s)
      {
        Eigen::Affine3d p; 
        if(!planning_models::poseFromMsg(object.plane_poses[i], p))
        {
          ROS_ERROR("Failed to convert from pose message to Eigen Affine3f for %s", object.id.c_str());
          return false;
        }
        cworld_->addToObject(object.id, shapes::ShapeConstPtr(s), t * p);
      }
    }
    return true;
  }
  else
    if (object.operation == moveit_msgs::CollisionObject::REMOVE)
    {
      cworld_->removeObject(object.id);
      return true;
    }
    else
      ROS_ERROR("Unknown collision object operation: %d", object.operation);
  return false;
}

bool planning_scene::PlanningScene::hasColor(const std::string &id) const
{
  if (colors_)
    if (colors_->find(id) != colors_->end())
      return true;
  if (parent_)
    return parent_->hasColor(id);
  return false;
}

const std_msgs::ColorRGBA& planning_scene::PlanningScene::getColor(const std::string &id) const
{
  if (colors_)
  {
    std::map<std::string, std_msgs::ColorRGBA>::const_iterator it = colors_->find(id);
    if (it != colors_->end())
      return it->second;
  }
  if (parent_)
    return parent_->getColor(id);
  static const std_msgs::ColorRGBA empty;
  return empty;
}

void planning_scene::PlanningScene::getKnownColors(std::map<std::string, std_msgs::ColorRGBA> &kc) const
{
  kc.clear();
  if (parent_)
    parent_->getKnownColors(kc);
  if (colors_)
    for (std::map<std::string, std_msgs::ColorRGBA>::const_iterator it = colors_->begin(); it != colors_->end() ; ++it)
      kc[it->first] = it->second;
}

void planning_scene::PlanningScene::setColor(const std::string &id, const std_msgs::ColorRGBA &color)
{
  if (!colors_)
    colors_.reset(new std::map<std::string, std_msgs::ColorRGBA>());
  (*colors_)[id] = color;
}

void planning_scene::PlanningScene::removeColor(const std::string &id)
{
  if (colors_)
    colors_->erase(id);
}

bool planning_scene::PlanningScene::isStateColliding(const moveit_msgs::RobotState &state, bool verbose) const
{
  planning_models::KinematicState s(getCurrentState());
  planning_models::robotStateToKinematicState(*getTransforms(), state, s);
  return isStateColliding(s, verbose);
}

bool planning_scene::PlanningScene::isStateColliding(bool verbose) const
{
  return isStateColliding(getCurrentState(), verbose);
}

bool planning_scene::PlanningScene::isStateColliding(const planning_models::KinematicState &state, bool verbose) const
{ 
  collision_detection::CollisionRequest req;
  req.verbose = verbose;
  collision_detection::CollisionResult  res;
  checkCollision(req, res, state);
  return res.collision;
}

bool planning_scene::PlanningScene::isStateFeasible(const moveit_msgs::RobotState &state, bool verbose) const
{
  if (state_feasibility_)
  {
    planning_models::KinematicState s(getCurrentState());
    planning_models::robotStateToKinematicState(*getTransforms(), state, s);
    return state_feasibility_(s, verbose);
  }
  return true;
}

bool planning_scene::PlanningScene::isStateFeasible(const planning_models::KinematicState &state, bool verbose) const
{
  if (state_feasibility_)
    return state_feasibility_(state, verbose);
  return true;
}

bool planning_scene::PlanningScene::isStateConstrained(const moveit_msgs::RobotState &state, const moveit_msgs::Constraints &constr, bool verbose) const
{   
  planning_models::KinematicState s(getCurrentState());
  planning_models::robotStateToKinematicState(*getTransforms(), state, s);
  return isStateConstrained(s, constr, verbose);
}


bool planning_scene::PlanningScene::isStateConstrained(const planning_models::KinematicState &state, const moveit_msgs::Constraints &constr, bool verbose) const
{
  kinematic_constraints::KinematicConstraintSetPtr ks(new kinematic_constraints::KinematicConstraintSet(getKinematicModel(), getTransforms()));
  ks->add(constr);
  if (ks->empty())
    return true;
  else
    return isStateConstrained(state, *ks, verbose);
}

bool planning_scene::PlanningScene::isStateConstrained(const moveit_msgs::RobotState &state, const kinematic_constraints::KinematicConstraintSet &constr, bool verbose) const
{
  planning_models::KinematicState s(getCurrentState());
  planning_models::robotStateToKinematicState(*getTransforms(), state, s);
  return isStateConstrained(s, constr, verbose);
}

bool planning_scene::PlanningScene::isStateConstrained(const planning_models::KinematicState &state,  const kinematic_constraints::KinematicConstraintSet &constr, bool verbose) const
{ 
  return constr.decide(state, verbose).satisfied;
}

bool planning_scene::PlanningScene::isStateValid(const planning_models::KinematicState &state, bool verbose) const
{
  static const moveit_msgs::Constraints emp_constraints;
  return isStateValid(state, emp_constraints, verbose);
}

bool planning_scene::PlanningScene::isStateValid(const moveit_msgs::RobotState &state, bool verbose) const
{
  static const moveit_msgs::Constraints emp_constraints;
  return isStateValid(state, emp_constraints, verbose);
}

bool planning_scene::PlanningScene::isStateValid(const moveit_msgs::RobotState &state, const moveit_msgs::Constraints &constr, bool verbose) const
{
  planning_models::KinematicState s(getCurrentState());
  planning_models::robotStateToKinematicState(*getTransforms(), state, s);
  return isStateValid(s, constr, verbose);
}

bool planning_scene::PlanningScene::isStateValid(const planning_models::KinematicState &state, const moveit_msgs::Constraints &constr, bool verbose) const
{
  if (isStateColliding(state, verbose))
    return false;
  if (!isStateFeasible(state, verbose))
    return false;
  return isStateConstrained(state, constr, verbose);
}

bool planning_scene::PlanningScene::isStateValid(const planning_models::KinematicState &state, const kinematic_constraints::KinematicConstraintSet &constr, bool verbose) const
{
  if (isStateColliding(state, verbose))
    return false;
  if (!isStateFeasible(state, verbose))
    return false;
  return isStateConstrained(state, constr, verbose);
}

bool planning_scene::PlanningScene::isPathValid(const moveit_msgs::RobotState &start_state, 
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{
  static const moveit_msgs::Constraints emp_constraints;
  static const std::vector<moveit_msgs::Constraints> emp_constraints_vector;
  return isPathValid(start_state, trajectory, emp_constraints, emp_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const moveit_msgs::RobotState &start_state,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{  
  static const std::vector<moveit_msgs::Constraints> emp_constraints_vector;
  return isPathValid(start_state, trajectory, path_constraints, emp_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const moveit_msgs::RobotState &start_state,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                const moveit_msgs::Constraints& goal_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{
  std::vector<moveit_msgs::Constraints> goal_constraints_vector(1, goal_constraints);
  return isPathValid(start_state, trajectory, path_constraints, goal_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const moveit_msgs::RobotState &start_state,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                const std::vector<moveit_msgs::Constraints>& goal_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{  
  planning_models::KinematicState start(getCurrentState());
  planning_models::robotStateToKinematicState(*getTransforms(), start_state, start);
  return isPathValid(start, trajectory, path_constraints, goal_constraints, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const planning_models::KinematicState &start,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{
  static const moveit_msgs::Constraints emp_constraints;
  static const std::vector<moveit_msgs::Constraints> emp_constraints_vector;
  return isPathValid(start, trajectory, emp_constraints, emp_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const planning_models::KinematicState &start,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{ 
  static const std::vector<moveit_msgs::Constraints> emp_constraints_vector;
  return isPathValid(start, trajectory, path_constraints, emp_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const planning_models::KinematicState &start,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                const moveit_msgs::Constraints& goal_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{
  std::vector<moveit_msgs::Constraints> goal_constraints_vector(1, goal_constraints);
  return isPathValid(start, trajectory, path_constraints, goal_constraints_vector, verbose, invalid_index);
}

bool planning_scene::PlanningScene::isPathValid(const planning_models::KinematicState &start,
                                                const moveit_msgs::RobotTrajectory &trajectory,
                                                const moveit_msgs::Constraints& path_constraints,
                                                const std::vector<moveit_msgs::Constraints>& goal_constraints,
                                                bool verbose,
                                                std::vector<std::size_t> *invalid_index) const
{
  bool result = true;
  if (invalid_index)
    invalid_index->clear();
  std::size_t state_count = std::max(trajectory.joint_trajectory.points.size(),
                                     trajectory.multi_dof_joint_trajectory.points.size());
  kinematic_constraints::KinematicConstraintSet ks_p(getKinematicModel(), getTransforms());
  ks_p.add(path_constraints);
  for (std::size_t i = 0 ; i < state_count ; ++i)
  {
    moveit_msgs::RobotState rs;
    planning_models::robotTrajectoryPointToRobotState(trajectory, i, rs);
    planning_models::KinematicStatePtr st(new planning_models::KinematicState(start));
    planning_models::robotStateToKinematicState(*getTransforms(), rs, *st);

    bool this_state_valid = true;
    if (isStateColliding(*st, verbose))
      this_state_valid = false;
    if (!isStateFeasible(*st, verbose))
      this_state_valid = false;
    if (!ks_p.empty() && !ks_p.decide(*st, verbose).satisfied)
      this_state_valid = false;
    
    if (!this_state_valid)
    {
      if (invalid_index)
        invalid_index->push_back(i);
      else
        return false;
      result = false;
    }
    
    // check goal for last state
    if (i + 1 == state_count && !goal_constraints.empty())
    {
      bool found = false;
      for (std::size_t k = 0 ; k < goal_constraints.size() ; ++k)
      {
        if (isStateConstrained(*st, goal_constraints[k]))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        if (verbose)
          ROS_INFO("Goal not satisfied");
        if (invalid_index)
          invalid_index->push_back(i);
        result = false;
      }
    }
  }
  return result;
} 
