/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Author: Ioan Sucan */

#include <moveit/planning_scene_rviz_plugin/planning_scene_render.h>
#include <moveit/planning_scene_rviz_plugin/kinematic_state_visualization.h>
#include <moveit/planning_scene_rviz_plugin/render_shapes.h>
#include <rviz/display_context.h>

#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSceneManager.h>

namespace moveit_rviz_plugin
{

PlanningSceneRender::PlanningSceneRender(Ogre::SceneNode *node, rviz::DisplayContext *context, const KinematicStateVisualizationPtr &robot) :
  planning_scene_geometry_node_(node->createChildSceneNode()),
  context_(context),
  scene_robot_(robot)
{
  render_shapes_.reset(new RenderShapes(context));
}

PlanningSceneRender::~PlanningSceneRender(void)
{
  context_->getSceneManager()->destroySceneNode(planning_scene_geometry_node_->getName());
}

void PlanningSceneRender::clear(void)
{
  render_shapes_->clear();
}

void PlanningSceneRender::renderPlanningScene(const planning_scene::PlanningSceneConstPtr &scene, 
                                              const rviz::Color &env_color, const rviz::Color &attached_color,
                                              float scene_alpha, float robot_alpha)
{
  if (!scene)
    return;
  
  clear();
  
  if (scene_robot_)
  {
    kinematic_state::KinematicStateConstPtr ks(new kinematic_state::KinematicState(scene->getCurrentState()));
    std_msgs::ColorRGBA color;
    color.r = attached_color.r_;
    color.g = attached_color.g_;
    color.b = attached_color.b_;
    color.a = 1.0f;
    scene_robot_->update(ks, color, scene->getObjectColors());
  }
  
  collision_detection::CollisionWorldConstPtr cworld = scene->getCollisionWorld();
  const std::vector<std::string> &ids = cworld->getObjectIds();
  for (std::size_t i = 0 ; i < ids.size() ; ++i)
  {
    collision_detection::CollisionWorld::ObjectConstPtr o = cworld->getObject(ids[i]);
    rviz::Color color = env_color;
    if (scene->hasColor(ids[i]))
    {
      const std_msgs::ColorRGBA &c = scene->getColor(ids[i]);
      color.r_ = c.r; color.g_ = c.g; color.b_ = c.b;
    }
    for (std::size_t j = 0 ; j < o->shapes_.size() ; ++j)
      render_shapes_->renderShape(planning_scene_geometry_node_, o->shapes_[j].get(), o->shape_poses_[j], color, scene_alpha);
  }
}

}
