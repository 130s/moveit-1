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

/* Author: Ioan Sucan, E. Gil Jones */

#include "planning_scene_monitor/planning_scene_monitor.h"

planning_scene_monitor::PlanningSceneMonitor::PlanningSceneMonitor(const std::string &robot_description, tf::Transformer *tf) :
    nh_("~"), tf_(tf)
{
    initialize(planning_scene::PlanningSceneConstPtr(), robot_description);
}

planning_scene_monitor::PlanningSceneMonitor::PlanningSceneMonitor(const planning_scene::PlanningSceneConstPtr &parent, const std::string &robot_description, tf::Transformer *tf) :
    nh_("~"), tf_(tf)
{
    initialize(parent, robot_description);
}

planning_scene_monitor::PlanningSceneMonitor::~PlanningSceneMonitor(void)
{
    delete collision_object_subscriber_;
    delete collision_object_filter_;
    delete attached_collision_object_subscriber_;
    delete collision_map_subscriber_;
    delete collision_map_filter_;
}

void planning_scene_monitor::PlanningSceneMonitor::initialize(const planning_scene::PlanningSceneConstPtr &parent, const std::string &robot_description)
{
    RobotModelLoader rml(robot_description);
    robot_description_ = rml.getRobotDescription();
    if (rml.getURDF() && rml.getSRDF())
    {
        if (parent)
            scene_.reset(new planning_scene::PlanningScene(parent));
        else
            scene_.reset(new planning_scene::PlanningScene());
        scene_const_ = scene_;
        if (scene_->configure(rml.getURDF(), rml.getSRDF()))
        {
            configureDefaultCollisionMatrix();
            configureDefaultPadding();
            if (scene_->isConfigured())
            {
                scene_->getCollisionRobot()->setPadding(default_robot_padd_);
                scene_->getCollisionRobot()->setScale(default_robot_scale_);
            }
        }
    }

    // listen for planning scene updates; these messages include transforms, so no need for filters
    planning_scene_subscriber_ = root_nh_.subscribe("planning_scene", 2, &PlanningSceneMonitor::newPlanningSceneCallback, this);
    planning_scene_diff_subscriber_ = root_nh_.subscribe("planning_scene_diff", 100, &PlanningSceneMonitor::newPlanningSceneDiffCallback, this);
    ROS_DEBUG_STREAM("Listening to 'planning_scene' and 'planning_scene_diff'");

    // listen for world geometry updates using message filters
    collision_object_subscriber_ = new message_filters::Subscriber<moveit_msgs::CollisionObject>(root_nh_, "collision_object", 1024);
    collision_object_filter_ = new tf::MessageFilter<moveit_msgs::CollisionObject>(*collision_object_subscriber_, *tf_, scene_->getPlanningFrame(), 1024);
    collision_object_filter_->registerCallback(boost::bind(&PlanningSceneMonitor::collisionObjectCallback, this, _1));
    ROS_DEBUG("Listening to 'collision_object' using message notifier with target frame '%s'", collision_object_filter_->getTargetFramesString().c_str());

    // using regular message filter as there's no header
    attached_collision_object_subscriber_ = new message_filters::Subscriber<moveit_msgs::AttachedCollisionObject>(root_nh_, "attached_collision_object", 1024);
    attached_collision_object_subscriber_->registerCallback(boost::bind(&PlanningSceneMonitor::attachObjectCallback, this, _1));

    // listen to collision map using filters

    collision_map_subscriber_ = new message_filters::Subscriber<moveit_msgs::CollisionMap>(root_nh_, "collision_map", 2);
    collision_map_filter_ = new tf::MessageFilter<moveit_msgs::CollisionMap>(*collision_map_subscriber_, *tf_, scene_->getPlanningFrame(), 2);
    collision_map_filter_->registerCallback(boost::bind(&PlanningSceneMonitor::collisionMapCallback, this, _1));
    ROS_INFO("Listening to 'collision_map' using message notifier with target frame '%s'", collision_map_filter_->getTargetFramesString().c_str());
}

void planning_scene_monitor::PlanningSceneMonitor::newPlanningSceneCallback(const moveit_msgs::PlanningSceneConstPtr &scene)
{
    if (scene_)
    {
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        scene_->setPlanningSceneMsg(*scene);
    }
}

void planning_scene_monitor::PlanningSceneMonitor::newPlanningSceneDiffCallback(const moveit_msgs::PlanningSceneConstPtr &scene)
{
    if (scene_)
    {
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        scene_->setPlanningSceneDiffMsg(*scene);
    }
}

void planning_scene_monitor::PlanningSceneMonitor::collisionObjectCallback(const moveit_msgs::CollisionObjectConstPtr &obj)
{
    if (scene_)
    {
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        scene_->processCollisionObjectMsg(*obj);
    }
}

void planning_scene_monitor::PlanningSceneMonitor::attachObjectCallback(const moveit_msgs::AttachedCollisionObjectConstPtr &obj)
{
    if (scene_)
    {
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        scene_->processAttachedCollisionObjectMsg(*obj);
    }
}

void planning_scene_monitor::PlanningSceneMonitor::collisionMapCallback(const moveit_msgs::CollisionMapConstPtr &map)
{
    if (scene_)
    {
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        scene_->processCollisionMapMsg(*map);
    }
}

void planning_scene_monitor::PlanningSceneMonitor::lockScene(void)
{
    scene_update_mutex_.lock();
}

void planning_scene_monitor::PlanningSceneMonitor::unlockScene(void)
{
    scene_update_mutex_.unlock();
}

void planning_scene_monitor::PlanningSceneMonitor::startStateMonitor(void)
{
    if (scene_->isConfigured())
    {
        if (!csm_)
            csm_.reset(new CurrentStateMonitor(scene_->getKinematicModel(), tf_));
        csm_->startStateMonitor();
    }
    else
        ROS_ERROR("Cannot monitor robot state because planning scene is not configured");
}

void planning_scene_monitor::PlanningSceneMonitor::stopStateMonitor(void)
{
    if (csm_)
        csm_->stopStateMonitor();
}

void planning_scene_monitor::PlanningSceneMonitor::useMonitoredState(void)
{
    if (csm_)
    {
        if (!csm_->haveCompleteState())
            ROS_ERROR("The complete state of the robot is not yet known");
        boost::mutex::scoped_lock slock(scene_update_mutex_);
        const std::map<std::string, double> &v = csm_->getCurrentStateValues();
        scene_->getCurrentState().setStateValues(v);
    }
    else
        ROS_ERROR("State monitor is not active. Unable to set the planning scene state");
}

void planning_scene_monitor::PlanningSceneMonitor::updateFixedTransforms(void)
{
    if (!tf_ || !scene_)
        return;
    std::vector<geometry_msgs::TransformStamped> transforms;
    const std::string &target = scene_->getPlanningFrame();

    std::vector<std::string> all_frame_names;
    tf_->getFrameStrings(all_frame_names);
    for (std::size_t i = 0 ; i < all_frame_names.size() ; ++i)
    {
        if (!all_frame_names[i].empty() && all_frame_names[i][0] == '/')
            all_frame_names[i].erase(all_frame_names[i].begin());

        if (all_frame_names[i] == target || scene_->getKinematicModel()->hasLinkModel(all_frame_names[i]))
            continue;

        ros::Time stamp;
        std::string err_string;
        if (tf_->getLatestCommonTime(target, all_frame_names[i], stamp, &err_string) != tf::NO_ERROR)
        {
            ROS_WARN_STREAM("No transform available between frame '" << all_frame_names[i] << "' and planning frame '" <<
                            target << "' (" << err_string << ")");
            continue;
        }

        tf::StampedTransform t;
        try
        {
            tf_->lookupTransform(target, all_frame_names[i], stamp, t);
        }
        catch (tf::TransformException& ex)
        {
            ROS_WARN_STREAM("Unable to transform object from frame '" << all_frame_names[i] << "' to planning frame '" <<
                            target << "' (" << ex.what() << ")");
            continue;
        }
        geometry_msgs::TransformStamped f;
        f.header.frame_id = all_frame_names[i];
        f.child_frame_id = target;
        f.transform.translation.x = t.getOrigin().x();
        f.transform.translation.y = t.getOrigin().y();
        f.transform.translation.z = t.getOrigin().z();
        const tf::Quaternion &q = t.getRotation();
        f.transform.rotation.x = q.x();
        f.transform.rotation.y = q.y();
        f.transform.rotation.z = q.z();
        f.transform.rotation.w = q.w();
        transforms.push_back(f);
    }
    boost::mutex::scoped_lock slock(scene_update_mutex_);
    scene_->getTransforms()->recordTransforms(transforms);
}

void planning_scene_monitor::PlanningSceneMonitor::configureDefaultCollisionMatrix(void)
{
    collision_detection::AllowedCollisionMatrix &acm = scene_->getAllowedCollisionMatrix();

    // no collisions allowed by default
    acm.setEntry(scene_->getKinematicModel()->getLinkModelNamesWithCollisionGeometry(),
                 scene_->getKinematicModel()->getLinkModelNamesWithCollisionGeometry(), false);

    // allow collisions for pairs that have been disabled
    const std::vector<std::pair<std::string, std::string> >&dc = scene_->getSrdfModel()->getDisabledCollisions();
    for (std::size_t i = 0 ; i < dc.size() ; ++i)
        acm.setEntry(dc[i].first, dc[i].second, true);

    // read overriding values from the param server

    // first we do default collision operations
    if (!nh_.hasParam(robot_description_ + "_planning/default_collision_operations"))
        ROS_DEBUG("No additional default collision operations specified");
    else
    {
        ROS_DEBUG("Reading additional default collision operations");

        XmlRpc::XmlRpcValue coll_ops;
        nh_.getParam(robot_description_ + "_planning/default_collision_operations", coll_ops);

        if (coll_ops.getType() != XmlRpc::XmlRpcValue::TypeArray)
        {
            ROS_WARN("default_collision_operations is not an array");
            return;
        }

        if (coll_ops.size() == 0)
        {
            ROS_WARN("No collision operations in default collision operations");
            return;
        }

        for (int i = 0 ; i < coll_ops.size() ; ++i)
        {
            if (!coll_ops[i].hasMember("object1") || !coll_ops[i].hasMember("object2") || !coll_ops[i].hasMember("operation"))
            {
                ROS_WARN("All collision operations must have two objects and an operation");
                continue;
            }
            acm.setEntry(std::string(coll_ops[i]["object1"]), std::string(coll_ops[i]["object2"]), std::string(coll_ops[i]["operation"]) == "disable");
        }
    }
}

void planning_scene_monitor::PlanningSceneMonitor::configureDefaultPadding(void)
{
    nh_.param(robot_description_ + "_planning/default_robot_padding", default_robot_padd_, 0.0);
    nh_.param(robot_description_ + "_planning/default_robot_scale", default_robot_scale_, 1.0);
    nh_.param(robot_description_ + "_planning/default_object_padding", default_object_padd_, 0.0);
    nh_.param(robot_description_ + "_planning/default_attached_padding", default_attached_padd_, 0.0);
}
