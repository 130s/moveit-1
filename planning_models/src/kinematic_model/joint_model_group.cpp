/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
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

#include <moveit/planning_models/kinematic_model.h>
#include <algorithm>
#include <set>

namespace planning_models
{
static bool orderLinksByIndex(const KinematicModel::LinkModel *a, const KinematicModel::LinkModel *b)
{
  return a->getTreeIndex() < b->getTreeIndex();
}
}

planning_models::KinematicModel::JointModelGroup::JointModelGroup(const std::string& group_name,
                                                                  const std::vector<const JointModel*> &group_joints,
                                                                  const KinematicModel* parent_model) :
  parent_model_(parent_model), name_(group_name), variable_count_(0), is_end_effector_(false), is_chain_(false)
{
  for (std::size_t i = 0 ; i < group_joints.size() ; ++i)
  {
    joint_model_map_[group_joints[i]->getName()] = group_joints[i];
    unsigned int vc = group_joints[i]->getVariableCount();
    if (vc > 0)
    {
      if (group_joints[i]->getMimic() == NULL)
      {
        joint_model_vector_.push_back(group_joints[i]);
        joint_model_name_vector_.push_back(group_joints[i]->getName());
        variable_count_ += vc;
      }
      else
        mimic_joints_.push_back(group_joints[i]);
    }
    else
      fixed_joints_.push_back(group_joints[i]);
  }
  
  //now we need to find all the set of joints within this group
  //that root distinct subtrees
  for (std::size_t i = 0 ; i < joint_model_vector_.size() ; ++i)
  {
    bool found = false;
    const JointModel *joint = joint_model_vector_[i];
    //if we find that an ancestor is also in the group, then the joint is not a root
    while (joint->getParentLinkModel() != NULL)
    {
      joint = joint->getParentLinkModel()->getParentJointModel();
      if (hasJointModel(joint->name_) && joint->getVariableCount() > 0 && joint->getMimic() == NULL)
      {
        found = true;
        break;
      }
    }
    if (!found)
      joint_roots_.push_back(joint_model_vector_[i]);
  }
  
  // compute joint_variable_index_map_
  unsigned int vector_index_counter = 0;
  for (std::size_t i = 0 ; i < joint_model_vector_.size() ; ++i)
  {
    const std::vector<std::string>& name_order = joint_model_vector_[i]->getVariableNames();
    for (std::size_t j = 0; j < name_order.size(); ++j)
    {
      joint_variables_index_map_[name_order[j]] = vector_index_counter + j;
      active_dof_names_.push_back(name_order[j]);
    }
    joint_variables_index_map_[joint_model_vector_[i]->getName()] = vector_index_counter;
    vector_index_counter += name_order.size();
  }

  for (std::size_t i = 0 ; i < mimic_joints_.size() ; ++i)
  {
    const std::vector<std::string>& name_order = mimic_joints_[i]->getVariableNames();
    const std::vector<std::string>& mim_name_order = mimic_joints_[i]->mimic_->getVariableNames();
    for (std::size_t j = 0; j < name_order.size(); ++j)
      joint_variables_index_map_[name_order[j]] = joint_variables_index_map_[mim_name_order[j]];
    joint_variables_index_map_[mimic_joints_[i]->getName()] = joint_variables_index_map_[mimic_joints_[i]->mimic_->getName()];
  }

  // now we need to make another pass for group links (we include the fixed joints here)
  std::set<const LinkModel*> group_links_set;
  for (std::size_t i = 0 ; i < group_joints.size() ; ++i)
    group_links_set.insert(group_joints[i]->getChildLinkModel());
  
  for (std::set<const LinkModel*>::iterator it = group_links_set.begin(); it != group_links_set.end(); ++it)
    link_model_vector_.push_back(*it);
  
  std::sort(link_model_vector_.begin(), link_model_vector_.end(), &orderLinksByIndex);
  for (std::size_t i = 0 ; i < link_model_vector_.size() ; ++i)
    link_model_name_vector_.push_back(link_model_vector_[i]->getName());

  // compute updated links
  std::set<const LinkModel*> u_links;
  for (std::size_t i = 0 ; i < joint_roots_.size() ; ++i)
  {
    std::vector<const LinkModel*> links;
    parent_model->getChildLinkModels(joint_roots_[i], links);
    u_links.insert(links.begin(), links.end());
  }
  for (std::set<const LinkModel*>::iterator it = u_links.begin(); it != u_links.end(); ++it)
  {
    updated_link_model_vector_.push_back(*it);
    if ((*it)->getShape())
    {
      updated_link_model_with_geometry_vector_.push_back(*it);
      updated_link_model_with_geometry_set_.insert(*it);
    }
  }
  std::sort(updated_link_model_vector_.begin(), updated_link_model_vector_.end(), &orderLinksByIndex);
  std::sort(updated_link_model_with_geometry_vector_.begin(), updated_link_model_with_geometry_vector_.end(), &orderLinksByIndex);
  for (std::size_t i = 0; i < updated_link_model_vector_.size(); ++i)
    updated_link_model_name_vector_.push_back(updated_link_model_vector_[i]->getName());
  for (std::size_t i = 0; i < updated_link_model_with_geometry_vector_.size(); ++i)
    updated_link_model_with_geometry_name_vector_.push_back(updated_link_model_with_geometry_vector_[i]->getName());
}

planning_models::KinematicModel::JointModelGroup::~JointModelGroup(void)
{
}

bool planning_models::KinematicModel::JointModelGroup::isSubgroup(const std::string& group) const
{
  for (std::size_t i = 0; i < subgroup_names_.size(); ++i)
    if (group == subgroup_names_[i])
      return true;
  return false;
}

bool planning_models::KinematicModel::JointModelGroup::hasJointModel(const std::string &joint) const
{
  return joint_model_map_.find(joint) != joint_model_map_.end();
}

bool planning_models::KinematicModel::JointModelGroup::hasLinkModel(const std::string &link) const
{
  return std::find(link_model_name_vector_.begin(), link_model_name_vector_.end(), link) != link_model_name_vector_.end();
}

const planning_models::KinematicModel::JointModel* planning_models::KinematicModel::JointModelGroup::getJointModel(const std::string &name) const
{
  std::map<std::string, const JointModel*>::const_iterator it = joint_model_map_.find(name);
  if (it == joint_model_map_.end())
  {
    logError("Joint '%s' not found in group '%s'", name.c_str(), name_.c_str());
    return NULL;
  }
  else
    return it->second;
}

void planning_models::KinematicModel::JointModelGroup::getRandomValues(random_numbers::RandomNumberGenerator &rng, std::vector<double> &values) const
{
  for (std::size_t i = 0  ; i < joint_model_vector_.size() ; ++i)
    joint_model_vector_[i]->getRandomValues(rng, values);
}

bool planning_models::KinematicModel::JointModelGroup::getDefaultValues(const std::string &name, std::map<std::string, double> &values) const
{
  std::map<std::string, std::map<std::string, double> >::const_iterator it = default_states_.find(name);
  if (it == default_states_.end())
    return false;
  values = it->second;
  return true;
}

void planning_models::KinematicModel::JointModelGroup::getDefaultValues(std::vector<double> &values) const
{
  values.reserve(values.size() + joint_model_vector_.size());
  for (std::size_t i = 0  ; i < joint_model_vector_.size() ; ++i)
    joint_model_vector_[i]->getDefaultValues(values);
}

void planning_models::KinematicModel::JointModelGroup::getDefaultValues(std::map<std::string, double> &values) const
{
  for (std::size_t i = 0  ; i < joint_model_vector_.size() ; ++i)
    joint_model_vector_[i]->getDefaultValues(values);
}

std::vector<moveit_msgs::JointLimits> planning_models::KinematicModel::JointModelGroup::getVariableLimits(void) const
{
  std::vector<moveit_msgs::JointLimits> ret_vec;
  for(unsigned int i = 0; i < joint_model_vector_.size(); i++)
  {
    std::vector<moveit_msgs::JointLimits> jvec = joint_model_vector_[i]->getVariableLimits();
    ret_vec.insert(ret_vec.end(), jvec.begin(), jvec.end());
  }
  return ret_vec;
}

void planning_models::KinematicModel::JointModelGroup::setSolverAllocators(const std::pair<SolverAllocatorFn, SolverAllocatorMapFn> &solvers)
{
  solver_allocators_ = solvers;
  if (solver_allocators_.first)
  {
    solver_instance_ = solver_allocators_.first(this);
    if (solver_instance_)
    {
      ik_joint_bijection_.clear();   
      const std::vector<std::string> &ik_jnames = solver_instance_->getJointNames(); 
      for (std::size_t i = 0 ; i < ik_jnames.size() ; ++i)
      {
        std::map<std::string, unsigned int>::const_iterator it = joint_variables_index_map_.find(ik_jnames[i]);
        if (it == joint_variables_index_map_.end())
        {
          logError("IK solver computes joint values for joint '%s' but group '%s' does not contain such a joint.", ik_jnames[i].c_str(), getName().c_str());
          solver_instance_.reset();
          return;
        }
        const planning_models::KinematicModel::JointModel *jm = getJointModel(ik_jnames[i]);
        for (unsigned int k = 0 ; k < jm->getVariableCount() ; ++k)
          ik_joint_bijection_.push_back(it->second + k);
      }
    }
  }
}

bool planning_models::KinematicModel::JointModelGroup::canSetStateFromIK(const std::string &tip) const
{
  const kinematics::KinematicsBaseConstPtr& solver = getSolverInstance();  
  const std::string &tip_frame = solver->getTipFrame();
  if (tip != tip_frame)
  {
    const KinematicModel::LinkModel *lm = getParentModel()->getLinkModel(tip);
    if (!lm)
      return false;
    const KinematicModel::LinkModel::AssociatedFixedTransformMap &fixed_links = lm->getAssociatedFixedTransforms();
    for (std::map<const KinematicModel::LinkModel*, Eigen::Affine3d>::const_iterator it = fixed_links.begin() ; it != fixed_links.end() ; ++it)
      if (it->first->getName() == tip_frame)
        return true;
    return false;
  }
  else
    return true;
}

void planning_models::KinematicModel::JointModelGroup::printGroupInfo(std::ostream &out) const
{
  out << "Group '" << name_ << "':" << std::endl;
  for (std::size_t i = 0 ; i < joint_model_vector_.size() ; ++i)
  {
    const std::vector<std::string> &vn = joint_model_vector_[i]->getVariableNames();
    for (std::vector<std::string>::const_iterator it = vn.begin() ; it != vn.end() ; ++it)
    {
      out << "   " << *it << " [";
      std::pair<double, double> b;
      joint_model_vector_[i]->getVariableBounds(*it, b);
      if (b.first <= -std::numeric_limits<double>::max())
        out << "DBL_MIN";
      else
        out << b.first;
      out << ", ";
      if (b.second >= std::numeric_limits<double>::max())
        out << "DBL_MAX";
      else
        out << b.second;
      out << "]";
      if (joint_model_vector_[i]->mimic_)
        out << " *";
      out << std::endl;
    }
  }
}
