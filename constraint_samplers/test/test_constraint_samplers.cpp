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

#include <moveit/planning_scene/planning_scene.h>
#include <moveit/kinematic_constraints/kinematic_constraint.h>
#include <moveit/constraint_samplers/default_constraint_samplers.h>
#include <moveit/constraint_samplers/constraint_sampler_manager.h>
#include <moveit/constraint_samplers/constraint_sampler_tools.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <moveit/kinematic_state/conversions.h>

#include <geometric_shapes/shape_operations.h>
#include <visualization_msgs/MarkerArray.h>

#include <gtest/gtest.h>
#include <urdf_parser/urdf_parser.h>
#include <fstream>

#include "pr2_arm_kinematics_plugin.h"

class LoadPlanningModelsPr2 : public testing::Test
{
protected:

  boost::shared_ptr<kinematics::KinematicsBase> getKinematicsSolverRightArm(const kinematic_model::JointModelGroup *jmg)
  {
    {
      return pr2_kinematics_plugin_right_arm_;
    }
  }

  boost::shared_ptr<kinematics::KinematicsBase> getKinematicsSolverLeftArm(const kinematic_model::JointModelGroup *jmg)
  {
    {
      return pr2_kinematics_plugin_left_arm_;
    }
  }
  
  virtual void SetUp()
  {
    srdf_model.reset(new srdf::Model());
    std::string xml_string;
    std::fstream xml_file("../kinematic_state/test/urdf/robot.xml", std::fstream::in);
    if (xml_file.is_open())
    {
      while ( xml_file.good() )
      {
        std::string line;
        std::getline( xml_file, line);
        xml_string += (line + "\n");
      }
      xml_file.close();
      urdf_model = urdf::parseURDF(xml_string);
    }
    srdf_model->initFile(*urdf_model, "../kinematic_state/test/srdf/robot.xml");
    kmodel.reset(new kinematic_model::KinematicModel(urdf_model, srdf_model));

    pr2_kinematics_plugin_right_arm_.reset(new pr2_arm_kinematics::PR2ArmKinematicsPlugin);

    pr2_kinematics_plugin_right_arm_->setRobotModel(urdf_model);
    pr2_kinematics_plugin_right_arm_->initialize("right_arm",
                                                 "torso_lift_link",
                                                 "r_wrist_roll_link",
                                                 .01);

    pr2_kinematics_plugin_left_arm_.reset(new pr2_arm_kinematics::PR2ArmKinematicsPlugin);

    pr2_kinematics_plugin_left_arm_->setRobotModel(urdf_model);
    pr2_kinematics_plugin_left_arm_->initialize("left_arm",
                                                "torso_lift_link",
                                                "l_wrist_roll_link",
                                                .01);
    
    func_right_arm.reset(new kinematic_model::SolverAllocatorFn(boost::bind(&LoadPlanningModelsPr2::getKinematicsSolverRightArm, this, _1)));
    func_left_arm.reset(new kinematic_model::SolverAllocatorFn(boost::bind(&LoadPlanningModelsPr2::getKinematicsSolverLeftArm, this, _1)));
    
    std::map<std::string, kinematic_model::SolverAllocatorFn> allocators;
    allocators["right_arm"] = *func_right_arm;
    allocators["left_arm"] = *func_left_arm;
    allocators["whole_body"] = *func_right_arm;
    allocators["base"] = *func_left_arm;
    
    kmodel->setKinematicsAllocators(allocators);
    
    ps.reset(new planning_scene::PlanningScene());
    ps->configure(urdf_model, srdf_model, kmodel);
    
  };
  
  virtual void TearDown()
  {
  }
  
protected:
  
  boost::shared_ptr<urdf::ModelInterface>     urdf_model;
  boost::shared_ptr<srdf::Model>     srdf_model;
  kinematic_model::KinematicModelPtr kmodel;
  planning_scene::PlanningScenePtr ps;
  boost::shared_ptr<pr2_arm_kinematics::PR2ArmKinematicsPlugin> pr2_kinematics_plugin_right_arm_;
  boost::shared_ptr<pr2_arm_kinematics::PR2ArmKinematicsPlugin> pr2_kinematics_plugin_left_arm_;
  boost::shared_ptr<kinematic_model::SolverAllocatorFn> func_right_arm;
  boost::shared_ptr<kinematic_model::SolverAllocatorFn> func_left_arm;
};

TEST_F(LoadPlanningModelsPr2, JointConstraintsSamplerSimple)
{
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  
  kinematic_constraints::JointConstraint jc1(kmodel, tf);
  moveit_msgs::JointConstraint jcm1;
  //leaving off joint name
  jcm1.position = 0.42;
  jcm1.tolerance_above = 0.01;
  jcm1.tolerance_below = 0.05;
  jcm1.weight = 1.0;
  EXPECT_FALSE(jc1.configure(jcm1));

  std::vector<kinematic_constraints::JointConstraint> js;
  js.push_back(jc1);

  constraint_samplers::JointConstraintSampler jcs(ps, "right_arm");
  //no valid constraints
  EXPECT_FALSE(jcs.configure(js));

  //testing that this does the right thing
  jcm1.joint_name = "r_shoulder_pan_joint";
  EXPECT_TRUE(jc1.configure(jcm1));
  js.push_back(jc1);
  EXPECT_TRUE(jcs.configure(js));
  EXPECT_EQ(jcs.getConstrainedJointCount(), 1);
  EXPECT_EQ(jcs.getUnconstrainedJointCount(), 6);
  EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));  

  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));
    EXPECT_TRUE(jc1.decide(ks).satisfied);
  }  

  //redoing the configure leads to 6 unconstrained variables as well
  EXPECT_TRUE(jcs.configure(js));
  EXPECT_EQ(jcs.getUnconstrainedJointCount(), 6);

  kinematic_constraints::JointConstraint jc2(kmodel, tf);

  moveit_msgs::JointConstraint jcm2;
  jcm2.joint_name = "r_shoulder_pan_joint";
  jcm2.position = 0.54;
  jcm2.tolerance_above = 0.01;
  jcm2.tolerance_below = 0.01;
  jcm2.weight = 1.0;
  EXPECT_TRUE(jc2.configure(jcm2));
  js.push_back(jc2);

  //creating a constraint that conflicts with the other (leaves no sampleable region)
  EXPECT_FALSE(jcs.configure(js));
  EXPECT_FALSE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));

  //we can't sample for a different group
  constraint_samplers::JointConstraintSampler jcs2(ps, "arms");
  jcs2.configure(js);
  EXPECT_FALSE(jcs2.sample(ks.getJointStateGroup("right_arm"), ks, 1));  

  //not ok to not have any references to joints in this group in the constraints
  constraint_samplers::JointConstraintSampler jcs3(ps, "left_arm");
  EXPECT_FALSE(jcs3.configure(js));

  //testing that the most restrictive bounds are used
  js.clear();

  jcm1.position = .4;
  jcm1.tolerance_above = .05;
  jcm1.tolerance_below = .05;
  jcm2.position = .4;
  jcm2.tolerance_above = .1;
  jcm2.tolerance_below = .1;
  EXPECT_TRUE(jc1.configure(jcm1));
  EXPECT_TRUE(jc2.configure(jcm2));
  js.push_back(jc1);
  js.push_back(jc2);

  EXPECT_TRUE(jcs.configure(js));

  //should always be within narrower constraints
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));
    EXPECT_TRUE(jc1.decide(ks).satisfied);
  }  

  //too narrow range outside of joint limits
  js.clear();

  jcm1.position = -3.1;
  jcm1.tolerance_above = .05;
  jcm1.tolerance_below = .05;
  
  //the joint configuration corrects this
  EXPECT_TRUE(jc1.configure(jcm1));
  js.push_back(jc1);
  EXPECT_TRUE(jcs.configure(js));

  //partially overlapping regions will also work
  js.clear();
  jcm1.position = .35;
  jcm1.tolerance_above = .05;
  jcm1.tolerance_below = .05;
  jcm2.position = .45;
  jcm2.tolerance_above = .05;
  jcm2.tolerance_below = .05;
  EXPECT_TRUE(jc1.configure(jcm1));
  EXPECT_TRUE(jc2.configure(jcm2));
  js.push_back(jc1);
  js.push_back(jc2);

  //leads to a min and max of .4, so all samples should be exactly .4
  EXPECT_TRUE(jcs.configure(js));
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));
    std::map<std::string, double> var_values;
    ks.getJointStateGroup("right_arm")->getVariableValues(var_values);
    EXPECT_NEAR(var_values["r_shoulder_pan_joint"], .4, std::numeric_limits<double>::epsilon());
    EXPECT_TRUE(jc1.decide(ks).satisfied);
    EXPECT_TRUE(jc2.decide(ks).satisfied);
  }  

  //this leads to a larger sampleable region
  jcm1.position = .38;
  jcm2.position = .42;
  EXPECT_TRUE(jc1.configure(jcm1));
  EXPECT_TRUE(jc2.configure(jcm2));
  js.push_back(jc1);
  js.push_back(jc2);
  EXPECT_TRUE(jcs.configure(js));
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("right_arm"), ks, 1));
    EXPECT_TRUE(jc1.decide(ks).satisfied);
    EXPECT_TRUE(jc2.decide(ks).satisfied);
  }  
}

TEST_F(LoadPlanningModelsPr2, IKConstraintsSamplerSimple)
{
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  
  kinematic_constraints::PositionConstraint pc(kmodel, tf);
  moveit_msgs::PositionConstraint pcm;
  
  pcm.link_name = "l_wrist_roll_link";
  pcm.target_point_offset.x = 0;
  pcm.target_point_offset.y = 0;
  pcm.target_point_offset.z = 0;
  pcm.constraint_region.primitives.resize(1);
  pcm.constraint_region.primitives[0].type = shape_msgs::SolidPrimitive::SPHERE;
  pcm.constraint_region.primitives[0].dimensions.resize(1);
  pcm.constraint_region.primitives[0].dimensions[0] = 0.001;
  
  pcm.constraint_region.primitive_poses.resize(1);
  pcm.constraint_region.primitive_poses[0].position.x = 0.55;
  pcm.constraint_region.primitive_poses[0].position.y = 0.2;
  pcm.constraint_region.primitive_poses[0].position.z = 1.25;
  pcm.constraint_region.primitive_poses[0].orientation.x = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.y = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.z = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.w = 1.0;
  pcm.weight = 1.0;
  
  EXPECT_FALSE(pc.configure(pcm));

  constraint_samplers::IKConstraintSampler ik_bad(ps, "l_arm");
  EXPECT_FALSE(ik_bad.isValid());
  
  constraint_samplers::IKConstraintSampler iks(ps, "left_arm");
  EXPECT_TRUE(iks.isValid());

  EXPECT_FALSE(iks.configure(constraint_samplers::IKSamplingPose()));

  EXPECT_FALSE(iks.configure(constraint_samplers::IKSamplingPose(pc)));

  pcm.header.frame_id = kmodel->getModelFrame();
  EXPECT_TRUE(pc.configure(pcm));
  EXPECT_TRUE(iks.configure(constraint_samplers::IKSamplingPose(pc)));

  //ik link not in this group
  constraint_samplers::IKConstraintSampler ik_bad_2(ps, "right_arm");
  EXPECT_TRUE(ik_bad_2.isValid());
  EXPECT_FALSE(ik_bad_2.configure(constraint_samplers::IKSamplingPose(pc)));

  //not the ik link
  pcm.link_name = "l_shoulder_pan_link";
  EXPECT_TRUE(pc.configure(pcm));
  EXPECT_FALSE(iks.configure(constraint_samplers::IKSamplingPose(pc)));

  //solver for base doesn't cover group
  constraint_samplers::IKConstraintSampler ik_base(ps, "base");
  EXPECT_TRUE(ik_base.isValid());
  pcm.link_name = "l_wrist_roll_link";
  EXPECT_TRUE(pc.configure(pcm));
  EXPECT_FALSE(ik_base.configure(constraint_samplers::IKSamplingPose(pc)));

  //shouldn't work as no direct constraint solver
  constraint_samplers::IKConstraintSampler ik_arms(ps, "arms");
  EXPECT_FALSE(iks.isValid());
}


TEST_F(LoadPlanningModelsPr2, OrientationConstraintsSampler)
{
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  
  kinematic_constraints::OrientationConstraint oc(kmodel, tf);
  moveit_msgs::OrientationConstraint ocm;
  
  ocm.link_name = "r_wrist_roll_link";
  ocm.header.frame_id = ocm.link_name; 
  ocm.orientation.x = 0.5;
  ocm.orientation.y = 0.5;
  ocm.orientation.z = 0.5;
  ocm.orientation.w = 0.5;
  ocm.absolute_x_axis_tolerance = 0.01;
  ocm.absolute_y_axis_tolerance = 0.01;
  ocm.absolute_z_axis_tolerance = 0.01;
  ocm.weight = 1.0;
  
  EXPECT_TRUE(oc.configure(ocm));
  
  bool p1 = oc.decide(ks).satisfied;
  EXPECT_FALSE(p1);
  
  ocm.header.frame_id = kmodel->getModelFrame();
  EXPECT_TRUE(oc.configure(ocm));
  
  constraint_samplers::IKConstraintSampler iks(ps, "right_arm");
  EXPECT_TRUE(iks.configure(constraint_samplers::IKSamplingPose(oc)));
  for (int t = 0 ; t < 100; ++t)
  {
    EXPECT_TRUE(iks.sample(ks.getJointStateGroup("right_arm"), ks, 100));
    EXPECT_TRUE(oc.decide(ks).satisfied);
  }
}

TEST_F(LoadPlanningModelsPr2, PoseConstraintsSampler)
{
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  
  kinematic_constraints::PositionConstraint pc(kmodel, tf);
  moveit_msgs::PositionConstraint pcm;
  
  pcm.link_name = "l_wrist_roll_link";
  pcm.target_point_offset.x = 0;
  pcm.target_point_offset.y = 0;
  pcm.target_point_offset.z = 0;
  pcm.constraint_region.primitives.resize(1);
  pcm.constraint_region.primitives[0].type = shape_msgs::SolidPrimitive::SPHERE;
  pcm.constraint_region.primitives[0].dimensions.resize(1);
  pcm.constraint_region.primitives[0].dimensions[0] = 0.001;
  
  pcm.header.frame_id = kmodel->getModelFrame();

  pcm.constraint_region.primitive_poses.resize(1);
  pcm.constraint_region.primitive_poses[0].position.x = 0.55;
  pcm.constraint_region.primitive_poses[0].position.y = 0.2;
  pcm.constraint_region.primitive_poses[0].position.z = 1.25;
  pcm.constraint_region.primitive_poses[0].orientation.x = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.y = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.z = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.w = 1.0;
  pcm.weight = 1.0;
  
  EXPECT_TRUE(pc.configure(pcm));
  
  kinematic_constraints::OrientationConstraint oc(kmodel, tf);
  moveit_msgs::OrientationConstraint ocm;
  
  ocm.link_name = "l_wrist_roll_link";
  ocm.header.frame_id = kmodel->getModelFrame();
  ocm.orientation.x = 0.0;
  ocm.orientation.y = 0.0;
  ocm.orientation.z = 0.0;
  ocm.orientation.w = 1.0;
  ocm.absolute_x_axis_tolerance = 0.2;
  ocm.absolute_y_axis_tolerance = 0.1;
  ocm.absolute_z_axis_tolerance = 0.4;
  ocm.weight = 1.0;
  
  EXPECT_TRUE(oc.configure(ocm));

  constraint_samplers::IKConstraintSampler iks1(ps, "left_arm");
  EXPECT_TRUE(iks1.configure(constraint_samplers::IKSamplingPose(pc, oc)));
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(iks1.sample(ks.getJointStateGroup("left_arm"), ks, 100));
    EXPECT_TRUE(pc.decide(ks).satisfied);
    EXPECT_TRUE(oc.decide(ks).satisfied);
  }
  
  constraint_samplers::IKConstraintSampler iks2(ps, "left_arm");
  EXPECT_TRUE(iks2.configure(constraint_samplers::IKSamplingPose(pc)));
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(iks2.sample(ks.getJointStateGroup("left_arm"), ks, 100));
    EXPECT_TRUE(pc.decide(ks).satisfied);
  }
  
  constraint_samplers::IKConstraintSampler iks3(ps, "left_arm");
  EXPECT_TRUE(iks3.configure(constraint_samplers::IKSamplingPose(oc)));
  for (int t = 0 ; t < 100 ; ++t)
  {
    EXPECT_TRUE(iks3.sample(ks.getJointStateGroup("left_arm"), ks, 100));
    EXPECT_TRUE(oc.decide(ks).satisfied);
  }
  
  
  // test the automatic construction of constraint sampler
  moveit_msgs::Constraints c;
  c.position_constraints.push_back(pcm);
  c.orientation_constraints.push_back(ocm);
  
  constraint_samplers::ConstraintSamplerPtr s = constraint_samplers::ConstraintSamplerManager::selectDefaultSampler(ps, "left_arm", c);
  EXPECT_TRUE(s.get() != NULL);
  static const int NT = 1000;
  int succ = 0;
  for (int t = 0 ; t < NT ; ++t)
  {
    EXPECT_TRUE(s->sample(ks.getJointStateGroup("left_arm"), ks, 100));
    EXPECT_TRUE(pc.decide(ks).satisfied);
    EXPECT_TRUE(oc.decide(ks).satisfied);
    if (s->sample(ks.getJointStateGroup("left_arm"), ks, 1))
      succ++;
  }
  ROS_INFO("Success rate for IK Constraint Sampler with position & orientation constraints for one arm: %lf", (double)succ / (double)NT);
}

TEST_F(LoadPlanningModelsPr2, JointConstraintsSamplerManager)
{
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  
  kinematic_constraints::JointConstraint jc1(kmodel, tf);
  moveit_msgs::JointConstraint jcm1;
  jcm1.joint_name = "head_pan_joint";
  jcm1.position = 0.42;
  jcm1.tolerance_above = 0.01;
  jcm1.tolerance_below = 0.05;
  jcm1.weight = 1.0;
  EXPECT_TRUE(jc1.configure(jcm1));

  kinematic_constraints::JointConstraint jc2(kmodel, tf);
  moveit_msgs::JointConstraint jcm2;
  jcm2.joint_name = "l_shoulder_pan_joint";
  jcm2.position = 0.9;
  jcm2.tolerance_above = 0.1;
  jcm2.tolerance_below = 0.05;
  jcm2.weight = 1.0;
  EXPECT_TRUE(jc2.configure(jcm2));
  
  kinematic_constraints::JointConstraint jc3(kmodel, tf);
  moveit_msgs::JointConstraint jcm3;
  jcm3.joint_name = "r_wrist_roll_joint";
  jcm3.position = 0.7;
  jcm3.tolerance_above = 0.14;
  jcm3.tolerance_below = 0.005;
  jcm3.weight = 1.0;
  EXPECT_TRUE(jc3.configure(jcm3));
  
  kinematic_constraints::JointConstraint jc4(kmodel, tf);
  moveit_msgs::JointConstraint jcm4;
  jcm4.joint_name = "torso_lift_joint";
  jcm4.position = 0.2;
  jcm4.tolerance_above = 0.09;
  jcm4.tolerance_below = 0.01;
  jcm4.weight = 1.0;
  EXPECT_TRUE(jc4.configure(jcm4));

  std::vector<kinematic_constraints::JointConstraint> js;
  js.push_back(jc1);
  js.push_back(jc2);
  js.push_back(jc3);
  js.push_back(jc4);
  
  constraint_samplers::JointConstraintSampler jcs(ps, "arms");
  jcs.configure(js);
  EXPECT_EQ(jcs.getConstrainedJointCount(), 2);
  EXPECT_EQ(jcs.getUnconstrainedJointCount(), 12);

  for (int t = 0 ; t < 10 ; ++t)
  {
    EXPECT_TRUE(jcs.sample(ks.getJointStateGroup("arms"), ks, 1));
    EXPECT_TRUE(jc2.decide(ks).satisfied);
    EXPECT_TRUE(jc3.decide(ks).satisfied);
  }

  // test the automatic construction of constraint sampler
  moveit_msgs::Constraints c;
  
  // no constraints should give no sampler
  constraint_samplers::ConstraintSamplerPtr s0 = constraint_samplers::ConstraintSamplerManager::selectDefaultSampler(ps, "arms", c);
  EXPECT_TRUE(s0.get() == NULL);

  // add the constraints
  c.joint_constraints.push_back(jcm1);
  c.joint_constraints.push_back(jcm2);
  c.joint_constraints.push_back(jcm3);
  c.joint_constraints.push_back(jcm4);
  
  constraint_samplers::ConstraintSamplerPtr s = constraint_samplers::ConstraintSamplerManager::selectDefaultSampler(ps, "arms", c);
  EXPECT_TRUE(s.get() != NULL);
  
  // test the generated sampler
  for (int t = 0 ; t < 1000 ; ++t)
  {
    EXPECT_TRUE(s->sample(ks.getJointStateGroup("arms"), ks, 1));
    EXPECT_TRUE(jc2.decide(ks).satisfied);
    EXPECT_TRUE(jc3.decide(ks).satisfied);
  }
}

TEST_F(LoadPlanningModelsPr2, GenericConstraintsSampler)
{
  moveit_msgs::Constraints c;
  
  moveit_msgs::PositionConstraint pcm;
  pcm.link_name = "l_wrist_roll_link";
  pcm.target_point_offset.x = 0;
  pcm.target_point_offset.y = 0;
  pcm.target_point_offset.z = 0;

  pcm.constraint_region.primitives.resize(1);
  pcm.constraint_region.primitives[0].type = shape_msgs::SolidPrimitive::SPHERE;
  pcm.constraint_region.primitives[0].dimensions.resize(1);
  pcm.constraint_region.primitives[0].dimensions[0] = 0.001;
  
  pcm.header.frame_id = kmodel->getModelFrame();

  pcm.constraint_region.primitive_poses.resize(1);
  pcm.constraint_region.primitive_poses[0].position.x = 0.55;
  pcm.constraint_region.primitive_poses[0].position.y = 0.2;
  pcm.constraint_region.primitive_poses[0].position.z = 1.25;
  pcm.constraint_region.primitive_poses[0].orientation.x = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.y = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.z = 0.0;
  pcm.constraint_region.primitive_poses[0].orientation.w = 1.0;
  pcm.weight = 1.0;
  c.position_constraints.push_back(pcm);
  
  moveit_msgs::OrientationConstraint ocm;
  ocm.link_name = "l_wrist_roll_link";
  ocm.header.frame_id = kmodel->getModelFrame();
  ocm.orientation.x = 0.0;
  ocm.orientation.y = 0.0;
  ocm.orientation.z = 0.0;
  ocm.orientation.w = 1.0;
  ocm.absolute_x_axis_tolerance = 0.2;
  ocm.absolute_y_axis_tolerance = 0.1;
  ocm.absolute_z_axis_tolerance = 0.4;
  ocm.weight = 1.0;
  c.orientation_constraints.push_back(ocm);
  
  ocm.link_name = "r_wrist_roll_link";
  ocm.header.frame_id = kmodel->getModelFrame();
  ocm.orientation.x = 0.0;
  ocm.orientation.y = 0.0;
  ocm.orientation.z = 0.0;
  ocm.orientation.w = 1.0;
  ocm.absolute_x_axis_tolerance = 0.01;
  ocm.absolute_y_axis_tolerance = 0.01;
  ocm.absolute_z_axis_tolerance = 0.01;
  ocm.weight = 1.0;
  c.orientation_constraints.push_back(ocm);
  
  kinematic_state::TransformsPtr tf = ps->getTransforms();
  constraint_samplers::ConstraintSamplerPtr s = constraint_samplers::ConstraintSamplerManager::selectDefaultSampler(ps, "arms", c);
  EXPECT_TRUE(s.get() != NULL);
  
  kinematic_constraints::KinematicConstraintSet kset(kmodel, tf);
  kset.add(c);
  
  kinematic_state::KinematicState ks(kmodel);
  ks.setToDefaultValues();  
  static const int NT = 1000;
  int succ = 0;
  for (int t = 0 ; t < NT ; ++t)
  {
    EXPECT_TRUE(s->sample(ks.getJointStateGroup("arms"), ks, 1000));
    EXPECT_TRUE(kset.decide(ks).satisfied);
    if (s->sample(ks.getJointStateGroup("arms"), ks, 1))
      succ++;
  } 
  logInform("Success rate for IK Constraint Sampler with position & orientation constraints for both arms: %lf", (double)succ / (double)NT);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  ros::Time::init();
  return RUN_ALL_TESTS();
}
