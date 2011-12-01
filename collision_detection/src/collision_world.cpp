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

#include "collision_detection/collision_world.h"
#include <geometric_shapes/shape_operations.h>
#include <ros/console.h>

collision_detection::CollisionWorld::CollisionWorld(void) : record_changes_(false)
{
}

collision_detection::CollisionWorld::CollisionWorld(const CollisionWorld &other) : record_changes_(false)
{
    boost::recursive_mutex::scoped_lock slock(other.objects_lock_);
    objects_ = other.objects_;
}

void collision_detection::CollisionWorld::checkCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const planning_models::KinematicState &state) const
{
    robot.checkSelfCollision(req, res, state);
    if (!res.collision || (req.contacts && res.contacts.size() < req.max_contacts))
        checkRobotCollision(req, res, robot, state);
}

/** \brief Check whether the model is in collision with itself or the world. Allowed collisions are ignored. */
void collision_detection::CollisionWorld::checkCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const planning_models::KinematicState &state, const AllowedCollisionMatrix &acm) const
{
    robot.checkSelfCollision(req, res, state, acm);
    if (!res.collision || (req.contacts && res.contacts.size() < req.max_contacts))
        checkRobotCollision(req, res, robot, state, acm);
}

void collision_detection::CollisionWorld::addObjects(const std::string &ns, const std::vector<shapes::Shape*> &shapes, const std::vector<btTransform> &poses)
{
    if (shapes.size() != poses.size())
        ROS_ERROR("Number of shapes and number of poses do not match. Not adding any objects to collision world.");
    else
        for (std::size_t i = 0 ; i < shapes.size() ; ++i)
            addObject(ns, shapes[i], poses[i]);
}

std::vector<std::string> collision_detection::CollisionWorld::getNamespaces(void) const
{
    std::vector<std::string> ns;
    for (std::map<std::string, NamespaceObjectsPtr>::const_iterator it = objects_.begin() ; it != objects_.end() ; ++it)
        ns.push_back(it->first);
    return ns;
}

collision_detection::CollisionWorld::NamespaceObjectsConstPtr collision_detection::CollisionWorld::getObjects(const std::string &ns) const
{
    static NamespaceObjectsConstPtr empty;
    std::map<std::string, NamespaceObjectsPtr>::const_iterator it = objects_.find(ns);
    if (it == objects_.end())
        return empty;
    else
        return it->second;
}

bool collision_detection::CollisionWorld::haveNamespace(const std::string &ns) const
{
    return objects_.find(ns) != objects_.end();
}

void collision_detection::CollisionWorld::ensureUnique(NamespaceObjectsPtr &ns)
{
    if (ns && !ns.unique())
        ns.reset(ns->clone());
}

void collision_detection::CollisionWorld::addObject(const std::string &ns, shapes::StaticShape *shape)
{
    // make sure that if a new namespace is created, it knows its name
    std::map<std::string, NamespaceObjectsPtr>::iterator it = objects_.find(ns);
    if (it == objects_.end())
    {
        objects_[ns].reset(new NamespaceObjects(ns));
        it = objects_.find(ns);
    }
    else
        if (record_changes_)
            changeRemoveObj(ns);
    {
        boost::recursive_mutex::scoped_lock slock(objects_lock_);
        ensureUnique(it->second);
        it->second->static_shapes_.push_back(shape);
    }

    if (record_changes_)
        changeAddObj(it->second.get());
}

void collision_detection::CollisionWorld::addObject(const std::string &ns, shapes::Shape *shape, const btTransform &pose)
{
    // make sure that if a new namespace is created, it knows its name
    std::map<std::string, NamespaceObjectsPtr>::iterator it = objects_.find(ns);
    if (it == objects_.end())
    {
        objects_[ns].reset(new NamespaceObjects(ns));
        it = objects_.find(ns);
    }
    else
        if (record_changes_)
            changeRemoveObj(ns);
    {
        boost::recursive_mutex::scoped_lock slock(objects_lock_);
        ensureUnique(it->second);
        it->second->shapes_.push_back(shape);
        it->second->shape_poses_.push_back(pose);
    }

    if (record_changes_)
        changeAddObj(it->second.get());
}

bool collision_detection::CollisionWorld::moveObject(const std::string &ns, const shapes::Shape *shape, const btTransform &pose)
{
    std::map<std::string, NamespaceObjectsPtr>::iterator it = objects_.find(ns);
    if (it != objects_.end())
    {
        unsigned int n = it->second->shapes_.size();
        for (unsigned int i = 0 ; i < n ; ++i)
            if (it->second->shapes_[i] == shape)
            {
                {
                    boost::recursive_mutex::scoped_lock slock(objects_lock_);
                    ensureUnique(it->second);
                    it->second->shape_poses_[i] = pose;
                }
                if (record_changes_)
                {
                    changeRemoveObj(ns);
                    changeAddObj(it->second.get());
                }
                return true;
            }
    }
    return false;
}

bool collision_detection::CollisionWorld::removeObject(const std::string &ns, const shapes::Shape *shape)
{
    std::map<std::string, NamespaceObjectsPtr>::iterator it = objects_.find(ns);
    if (it != objects_.end())
    {
        unsigned int n = it->second->shapes_.size();
        for (unsigned int i = 0 ; i < n ; ++i)
            if (it->second->shapes_[i] == shape)
            {
                boost::recursive_mutex::scoped_lock slock(objects_lock_);
                ensureUnique(it->second);
                it->second->shapes_.erase(it->second->shapes_.begin() + i);
                it->second->shape_poses_.erase(it->second->shape_poses_.begin() + i);
                if (it->second->shapes_.empty() && it->second->static_shapes_.empty())
                {
                    objects_.erase(it);
                    if (record_changes_)
                        changeRemoveObj(ns);
                }
                else
                    if (record_changes_)
                    {
                        changeRemoveObj(ns);
                        changeAddObj(it->second.get());
                    }
                return true;
            }
    }
    return false;
}

bool collision_detection::CollisionWorld::removeObject(const std::string &ns, const shapes::StaticShape *shape)
{
    std::map<std::string, NamespaceObjectsPtr>::iterator it = objects_.find(ns);
    if (it != objects_.end())
    {
        unsigned int n = it->second->static_shapes_.size();
        for (unsigned int i = 0 ; i < n ; ++i)
            if (it->second->static_shapes_[i] == shape)
            {
                boost::recursive_mutex::scoped_lock slock(objects_lock_);
                ensureUnique(it->second);
                it->second->static_shapes_.erase(it->second->static_shapes_.begin() + i);
                if (it->second->shapes_.empty() && it->second->static_shapes_.empty())
                {
                    objects_.erase(it);
                    if (record_changes_)
                        changeRemoveObj(ns);
                }
                else
                    if (record_changes_)
                    {
                        changeRemoveObj(ns);
                        changeAddObj(it->second.get());
                    }
                return true;
            }
    }
    return false;
}

void collision_detection::CollisionWorld::clearObjects(const std::string &ns)
{
    boost::recursive_mutex::scoped_lock slock(objects_lock_);
    if (objects_.erase(ns) == 1)
        if (record_changes_)
            changeRemoveObj(ns);
}

void collision_detection::CollisionWorld::clearObjects(void)
{
    if (record_changes_)
        for (std::map<std::string, NamespaceObjectsPtr>::const_iterator it = objects_.begin() ; it != objects_.end() ; ++it)
            changeRemoveObj(it->first);
    boost::recursive_mutex::scoped_lock slock(objects_lock_);
    objects_.clear();
}

collision_detection::CollisionWorld::NamespaceObjects::NamespaceObjects(const std::string &ns) : ns_(ns)
{
}

collision_detection::CollisionWorld::NamespaceObjects* collision_detection::CollisionWorld::NamespaceObjects::clone(void) const
{
    NamespaceObjects *o = new NamespaceObjects(ns_);
    for (std::size_t i = 0 ; i < shapes_.size() ; ++i)
        o->shapes_.push_back(shapes_[i]->clone());
    for (std::size_t i = 0 ; i < static_shapes_.size() ; ++i)
        o->static_shapes_.push_back(static_shapes_[i]->clone());
    o->shape_poses_ = shape_poses_;
    return o;
}

void collision_detection::CollisionWorld::changeRemoveObj(const std::string &ns)
{
    for (std::vector<Change>::reverse_iterator it = changes_.rbegin() ; it != changes_.rend() ; ++it)
        if (it->type_ == Change::ADD && it->ns_ == ns)
        {
            changes_.erase(--it.base());
            return;
        }
    Change c;
    c.type_ = Change::REMOVE;
    c.ns_ = ns;
    changes_.push_back(c);
}

void collision_detection::CollisionWorld::changeAddObj(const NamespaceObjects *obj)
{
    Change c;
    c.type_ = Change::ADD;
    c.ns_ = obj->ns_;
    changes_.push_back(c);
}

void collision_detection::CollisionWorld::recordChanges(bool flag)
{
    record_changes_ = flag;
}

bool collision_detection::CollisionWorld::isRecordingChanges(void) const
{
    return record_changes_;
}

const std::vector<collision_detection::CollisionWorld::Change>& collision_detection::CollisionWorld::getChanges(void) const
{
    return changes_;
}

void collision_detection::CollisionWorld::clearChanges(void)
{
    changes_.clear();
}

collision_detection::CollisionWorld::NamespaceObjects::~NamespaceObjects(void)
{
    for (std::size_t i = 0 ; i < static_shapes_.size() ; ++i)
        delete static_shapes_[i];
    for (std::size_t i = 0 ; i < shapes_.size() ; ++i)
        delete shapes_[i];
}
