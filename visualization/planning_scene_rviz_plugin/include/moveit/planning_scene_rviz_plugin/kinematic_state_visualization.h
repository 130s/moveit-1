/*
 * Copyright (c) 2008, Willow Garage, Inc.
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

#ifndef MOVEIT_PLANNING_SCENE_RVIZ_PLUGIN_KINEMATIC_STATE_VISUALIZATION_
#define MOVEIT_PLANNING_SCENE_RVIZ_PLUGIN_KINEMATIC_STATE_VISUALIZATION_

#include <moveit/kinematic_state/kinematic_state.h>
#include <rviz/robot/robot.h>

namespace moveit_rviz_plugin
{

class RenderShapes;
typedef boost::shared_ptr<RenderShapes> RenderShapesPtr;

/** \brief Update the links of an rviz::Robot using a kinematic_state::KinematicState */
class KinematicStateVisualization
{
public:
  
  KinematicStateVisualization(Ogre::SceneNode* root_node, rviz::DisplayContext* context,
                              const std::string& name, rviz::Property* parent_property);

  rviz::Robot& getRobot(void)
  {
    return robot_;
  }
  
  void load(const urdf::ModelInterface &descr, bool visual = true, bool collision = true);
  void clear();

  void update(const kinematic_state::KinematicStateConstPtr &kinematic_state);
  void update(const kinematic_state::KinematicStateConstPtr &kinematic_state, const std_msgs::ColorRGBA &default_attached_object_color);
  void update(const kinematic_state::KinematicStateConstPtr &kinematic_state, const std_msgs::ColorRGBA &default_attached_object_color, 
              const std::map<std::string, std_msgs::ColorRGBA> &color_map);
  
  /**
   * \brief Set the robot as a whole to be visible or not
   * @param visible Should we be visible?
   */
  void setVisible(bool visible);
  
  /**
   * \brief Set whether the visual meshes of the robot should be visible
   * @param visible Whether the visual meshes of the robot should be visible
   */
  void setVisualVisible(bool visible);
  
  /**
   * \brief Set whether the collision meshes/primitives of the robot should be visible
   * @param visible Whether the collision meshes/primitives should be visible
   */
  void setCollisionVisible(bool visible);

  void setAlpha(float alpha);

private:
  
  void updateHelper(const kinematic_state::KinematicStateConstPtr &kinematic_state,
                    const std_msgs::ColorRGBA &default_attached_object_color,
                    const std::map<std::string, std_msgs::ColorRGBA> *color_map);
  rviz::Robot robot_;
  RenderShapesPtr render_shapes_;
  bool visible_;
  bool visual_visible_;
  bool collision_visible_;
  
};

typedef boost::shared_ptr<KinematicStateVisualization> KinematicStateVisualizationPtr;
typedef boost::shared_ptr<const KinematicStateVisualization> KinematicStateVisualizationConstPtr;

}

#endif
