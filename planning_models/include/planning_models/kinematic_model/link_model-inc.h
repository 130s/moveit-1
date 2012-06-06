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

/*------------------------------------------------------*/
/*   DO NOT INCLUDE THIS FILE DIRECTLY                  */
/*------------------------------------------------------*/

/** \brief A link from the robot. Contains the constant transform applied to the link and its geometry */
class LinkModel
{
  friend class KinematicModel;
public:
  
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  
  LinkModel(void);
  ~LinkModel(void);
  
  /** \brief The name of this link */
  const std::string& getName(void) const
  {
    return name_;
  }
  
  /** \brief Get the filename of the mesh resource for this link */
  const std::string& getFilename(void) const
  {
    return filename_;
  }
  
  /** \brief Get the filename of the mesh resource for this link */
  const std::string& getVisualFilename(void) const
  {
    return visual_filename_;
  }
  
  /** \brief The index of this joint when traversing the kinematic tree in depth first fashion */
  int getTreeIndex(void) const
  {
    return tree_index_;
  }
  
  /** \brief Get the joint model whose child this link is. There will always be a parent joint */
  const JointModel* getParentJointModel(void) const
  {
    return parent_joint_model_;
  }
  
  /** \brief A link may have 0 or more child joints. From those joints there will certainly be other descendant links */
  const std::vector<JointModel*>& getChildJointModels(void) const
  {
    return child_joint_models_;
  }
  
  /** \brief When transforms are computed for this link,
      they are usually applied to the link's origin. The
      joint origin transform acts as an offset -- it is
      pre-applied before any other transform */
  const Eigen::Affine3d& getJointOriginTransform(void) const
  {
    return joint_origin_transform_;
  }
  
  /** \brief In addition to the link transform, the geometry
      of a link that is used for collision checking may have
      a different offset itself, with respect to the origin */
  const Eigen::Affine3d& getCollisionOriginTransform(void) const
  {
    return collision_origin_transform_;
  }
  
  /** \brief Get shape associated to the collision geometry for this link */
  const shapes::ShapeConstPtr& getShape(void) const
  {
    return shape_;
  }

  /** \brief Get shape associated to the collision geometry for this link */
  const shape_msgs::Shape& getShapeMsg(void) const
  {
    return shape_msg_;
  }
  
private:
  
  /** \brief Name of the link */
  std::string               name_;
  
  /** \brief JointModel that connects this link to the parent link */
  JointModel               *parent_joint_model_;
  
  /** \brief List of descending joints (each connects to a child link) */
  std::vector<JointModel*>  child_joint_models_;
  
  /** \brief The constant transform applied to the link (local) */
  Eigen::Affine3d           joint_origin_transform_;
  
  /** \brief The constant transform applied to the collision geometry of the link (local) */
  Eigen::Affine3d           collision_origin_transform_;
  
  /** \brief The collision geometry of the link */
  shapes::ShapeConstPtr     shape_;
  
  /** \brief The collision geometry of the link as a message */
  shape_msgs::Shape         shape_msg_;
  
  /** \brief Filename associated with the collision geometry mesh of this link (loaded in shape_). If empty, no mesh was used. */
  std::string               filename_;
  
  /** \brief Filename associated with the visual geometry mesh of this link (loaded in shape_). If empty, no mesh was used. */
  std::string               visual_filename_;
  
  /** \brief The index assigned to this link when traversing the kinematic tree in depth first fashion */
  int                       tree_index_;
  
};
