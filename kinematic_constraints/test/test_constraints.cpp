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

#include <kinematic_constraints/kinematic_constraint.h>
#include <gtest/gtest.h>

class LoadPlanningModelsPr2 : public testing::Test
{
protected:

    virtual void SetUp()
    {
	urdf_model.reset(new urdf::Model());
	srdf_model.reset(new srdf::Model());
	urdf_model->initFile("../planning_models/test/urdf/robot.xml");
        kmodel.reset(new planning_models::KinematicModel(urdf_model, srdf_model));
    };

    virtual void TearDown()
    {
    }

protected:

    boost::shared_ptr<urdf::Model>     urdf_model;
    boost::shared_ptr<srdf::Model>     srdf_model;
    planning_models::KinematicModelPtr kmodel;
};

TEST_F(LoadPlanningModelsPr2, JointConstraintsSimple)
{
    planning_models::KinematicState ks(kmodel);
    ks.setToDefaultValues();
    planning_models::TransformsPtr tf(new planning_models::Transforms(kmodel->getModelFrame()));

    kinematic_constraints::JointConstraint jc(kmodel, tf);
    moveit_msgs::JointConstraint jcm;
    jcm.joint_name = "head_pan_joint";
    jcm.position = 0.4;
    jcm.tolerance_above = 0.1;
    jcm.tolerance_below = 0.05;
    jcm.weight = 1.0;

    EXPECT_TRUE(jc.configure(jcm));
    double p1_d;
    EXPECT_FALSE(jc.decide(ks, p1_d));
    EXPECT_NEAR(p1_d, jcm.position, 1e-6);

    std::map<std::string, double> jvals;
    jvals[jcm.joint_name] = 0.41;
    ks.setStateValues(jvals);

    double p2_d;
    EXPECT_TRUE(jc.decide(ks, p2_d));
    EXPECT_NEAR(p2_d, 0.01, 1e-6);

    double dummy;
    jvals[jcm.joint_name] = 0.46;
    ks.setStateValues(jvals);
    EXPECT_TRUE(jc.decide(ks, dummy));

    jvals[jcm.joint_name] = 0.501;
    ks.setStateValues(jvals);
    EXPECT_FALSE(jc.decide(ks, dummy));

    jvals[jcm.joint_name] = 0.39;
    ks.setStateValues(jvals);
    EXPECT_TRUE(jc.decide(ks, dummy));

    jvals[jcm.joint_name] = 0.34;
    ks.setStateValues(jvals);
    EXPECT_FALSE(jc.decide(ks, dummy));
    EXPECT_TRUE(jc.equal(jc, 1e-12));
}

TEST_F(LoadPlanningModelsPr2, JointConstraintsCont)
{
    planning_models::KinematicState ks(kmodel);
    ks.setToDefaultValues();
    planning_models::TransformsPtr tf(new planning_models::Transforms(kmodel->getModelFrame()));

    kinematic_constraints::JointConstraint jc(kmodel, tf);
    moveit_msgs::JointConstraint jcm;

    jcm.joint_name = "l_wrist_roll_joint";
    jcm.position = 3.14;
    jcm.tolerance_above = 0.04;
    jcm.tolerance_below = 0.02;
    jcm.weight = 1.0;

    EXPECT_TRUE(jc.configure(jcm));

    std::map<std::string, double> jvals;
    jvals[jcm.joint_name] = 3.17;
    ks.setStateValues(jvals);

    double p1_d;
    EXPECT_TRUE(jc.decide(ks, p1_d));
    EXPECT_NEAR(p1_d, 0.03, 1e-6);


    jvals[jcm.joint_name] = -3.14;
    ks.setStateValues(jvals);

    double p2_d;
    EXPECT_TRUE(jc.decide(ks, p2_d));
    EXPECT_NEAR(p2_d, 0.003185, 1e-4);
}

TEST_F(LoadPlanningModelsPr2, PositionConstraintsFixed)
{
    planning_models::KinematicState ks(kmodel);
    ks.setToDefaultValues();
    planning_models::TransformsPtr tf(new planning_models::Transforms(kmodel->getModelFrame()));

    kinematic_constraints::PositionConstraint pc(kmodel, tf);
    moveit_msgs::PositionConstraint pcm;

    pcm.link_name = "l_wrist_roll_link";
    pcm.target_point_offset.x = 0;
    pcm.target_point_offset.y = 0;
    pcm.target_point_offset.z = 0;
    pcm.constraint_region_shape.type = shape_msgs::Shape::SPHERE;
    pcm.constraint_region_shape.dimensions.push_back(0.1);

    pcm.constraint_region_pose.header.frame_id = kmodel->getModelFrame();
    pcm.constraint_region_pose.pose.position.x = 0.55;
    pcm.constraint_region_pose.pose.position.y = 0.2;
    pcm.constraint_region_pose.pose.position.z = 1.25;
    pcm.constraint_region_pose.pose.orientation.x = 0.0;
    pcm.constraint_region_pose.pose.orientation.y = 0.0;
    pcm.constraint_region_pose.pose.orientation.z = 0.0;
    pcm.constraint_region_pose.pose.orientation.w = 1.0;
    pcm.weight = 1.0;

    EXPECT_TRUE(pc.configure(pcm));
    double dummy;
    EXPECT_TRUE(pc.decide(ks, dummy));

    std::map<std::string, double> jvals;
    jvals["torso_lift_joint"] = 0.4;
    ks.setStateValues(jvals);
    EXPECT_FALSE(pc.decide(ks, dummy)); 
    EXPECT_TRUE(pc.equal(pc, 1e-12));
}

TEST_F(LoadPlanningModelsPr2, PositionConstraintsMobile)
{
    planning_models::KinematicState ks(kmodel);
    ks.setToDefaultValues();
    planning_models::TransformsPtr tf(new planning_models::Transforms(kmodel->getModelFrame()));

    kinematic_constraints::PositionConstraint pc(kmodel, tf);
    moveit_msgs::PositionConstraint pcm;

    pcm.link_name = "l_wrist_roll_link";
    pcm.target_point_offset.x = 0;
    pcm.target_point_offset.y = 0;
    pcm.target_point_offset.z = 0;
    pcm.constraint_region_shape.type = shape_msgs::Shape::SPHERE;
    pcm.constraint_region_shape.dimensions.push_back(0.38);

    pcm.constraint_region_pose.header.frame_id = "r_wrist_roll_link";
    pcm.constraint_region_pose.pose.position.x = 0.0;
    pcm.constraint_region_pose.pose.position.y = 0.0;
    pcm.constraint_region_pose.pose.position.z = 0.0;
    pcm.constraint_region_pose.pose.orientation.x = 0.0;
    pcm.constraint_region_pose.pose.orientation.y = 0.0;
    pcm.constraint_region_pose.pose.orientation.z = 0.0;
    pcm.constraint_region_pose.pose.orientation.w = 1.0;
    pcm.weight = 1.0;

    EXPECT_FALSE(tf->isFixedFrame(pcm.link_name));
    EXPECT_TRUE(pc.configure(pcm));

    double dummy;
    EXPECT_TRUE(pc.decide(ks, dummy));

    pcm.constraint_region_shape.type = shape_msgs::Shape::BOX;
    pcm.constraint_region_shape.dimensions.resize(3);
    pcm.constraint_region_shape.dimensions[0] = 0.2;
    pcm.constraint_region_shape.dimensions[1] = 1.25;
    pcm.constraint_region_shape.dimensions[2] = 0.1;
    EXPECT_TRUE(pc.configure(pcm));

    std::map<std::string, double> jvals;
    jvals["l_shoulder_pan_joint"] = 0.4;
    ks.setStateValues(jvals);
    EXPECT_TRUE(pc.decide(ks, dummy));
    EXPECT_TRUE(pc.equal(pc, 1e-12));
}


TEST_F(LoadPlanningModelsPr2, OrientationConstraintsSimple)
{
    planning_models::KinematicState ks(kmodel);
    ks.setToDefaultValues();
    planning_models::TransformsPtr tf(new planning_models::Transforms(kmodel->getModelFrame()));

    kinematic_constraints::OrientationConstraint oc(kmodel, tf);
    moveit_msgs::OrientationConstraint ocm;

    ocm.link_name = "r_wrist_roll_link";
    ocm.orientation.header.frame_id = kmodel->getModelFrame();
    ocm.orientation.quaternion.x = 0.0;
    ocm.orientation.quaternion.y = 0.0;
    ocm.orientation.quaternion.z = 0.0;
    ocm.orientation.quaternion.w = 1.0;
    ocm.absolute_x_axis_tolerance = 0.1;
    ocm.absolute_y_axis_tolerance = 0.1;
    ocm.absolute_z_axis_tolerance = 0.1;
    ocm.weight = 1.0;

    EXPECT_TRUE(oc.configure(ocm));
    
    double dummy;
    EXPECT_FALSE(oc.decide(ks, dummy));

    ocm.orientation.header.frame_id = ocm.link_name;
    EXPECT_TRUE(oc.configure(ocm));
    EXPECT_TRUE(oc.decide(ks, dummy));  
    EXPECT_TRUE(oc.equal(oc, 1e-12));
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
