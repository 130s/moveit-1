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

/* Author: Ioan Sucan */

#include "moveit_warehouse/warehouse.h"

static const std::string DATABASE_NAME = "moveit_planning_scenes";
static const std::string PLANNING_SCENE_ID_NAME = "planning_scene_id";
static const std::string PLANNING_SCENE_TIME_NAME = "planning_scene_time";
static const std::string MOTION_PLAN_REQUEST_ID_NAME = "motion_request_id";

static const std::string TRAJECTORY_ID_NAME = "trajectory_id";
static const std::string TRAJECTORY_MOTION_REQUEST_ID_NAME = "trajectory_motion_request_id";
static const std::string PAUSED_COLLISION_MAP_TIME_NAME = "paused_collision_map_time";


void moveit_warehouse::PlanningSceneStorage::addPlanningScene(const moveit_msgs::PlanningScene &scene)
{
    mongo_ros::Metadata metadata(PLANNING_SCENE_ID_NAME, scene.name,
				 PLANNING_SCENE_TIME_NAME, scene.robot_state.joint_state.header.stamp.toSec());
    planning_scene_collection_->insert(scene, metadata);
}

std::string moveit_warehouse::PlanningSceneStorage::getMotionPlanRequestName(const moveit_msgs::MotionPlanRequest &planning_query, const std::string &scene_name) const
{
    // get all existing motion planning requests for this planning scene
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    std::vector<MotionPlanRequestWithMetadata> existing_requests = motion_plan_request_collection_->pullAllResults(q, false);
    
    // if there are no requests stored, we are done
    if (existing_requests.empty())
	return "";
    
    // compute the serialization of the message passed as argument
    const size_t serial_size_arg = ros::serialization::serializationLength(planning_query);
    boost::shared_array<uint8_t> buffer_arg(new uint8_t[serial_size_arg]);
    ros::serialization::OStream stream_arg(buffer_arg.get(), serial_size_arg);
    ros::serialization::serialize(stream_arg, planning_query);
    const void* data_arg = buffer_arg.get();
    
    for (std::size_t i = 0 ; i < existing_requests.size() ; ++i)
    {
	const size_t serial_size = ros::serialization::serializationLength(static_cast<const moveit_msgs::MotionPlanRequest&>(*existing_requests[i]));
	if (serial_size != serial_size_arg)
	    continue;
	boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);
	ros::serialization::OStream stream(buffer.get(), serial_size);
	ros::serialization::serialize(stream, static_cast<const moveit_msgs::MotionPlanRequest&>(*existing_requests[i]));
	const void* data = buffer.get();
	if (memcmp(data_arg, data, serial_size) == 0)
	    // we found the same message twice
	    return existing_requests[i]->lookupString(MOTION_PLAN_REQUEST_ID_NAME);
    }
    return "";
}

void moveit_warehouse::PlanningSceneStorage::addPlanningRequest(const moveit_msgs::MotionPlanRequest &planning_query, const std::string &scene_name, const std::string &query_name)
{   
    std::string id = getMotionPlanRequestName(planning_query, scene_name);
    if (id != query_name || id == "")
        addNewPlanningRequest(planning_query, scene_name, query_name);
}

std::string moveit_warehouse::PlanningSceneStorage::addNewPlanningRequest(const moveit_msgs::MotionPlanRequest &planning_query, const std::string &scene_name, const std::string &query_name)
{ 
    std::string id = query_name;
    if (id.empty())
    {	
	std::set<std::string> used;
	mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
	std::vector<MotionPlanRequestWithMetadata> existing_requests = motion_plan_request_collection_->pullAllResults(q, true);
	for (std::size_t i = 0 ; i < existing_requests.size() ; ++i)
	    used.insert(existing_requests[i]->lookupString(MOTION_PLAN_REQUEST_ID_NAME));
	std::size_t index = existing_requests.size();
	do
	{
	    id = "Motion Plan Request " + boost::lexical_cast<std::string>(index);
	    index++;
	} while (used.find(id) != used.end());	
    }
    mongo_ros::Metadata metadata(PLANNING_SCENE_ID_NAME, scene_name,
				 MOTION_PLAN_REQUEST_ID_NAME, id);
    motion_plan_request_collection_->insert(planning_query, metadata);
    return id;
}

void moveit_warehouse::PlanningSceneStorage::addPlanningResult(const moveit_msgs::MotionPlanRequest &planning_query, const moveit_msgs::RobotTrajectory &result, const std::string &scene_name)
{
    std::string id = getMotionPlanRequestName(planning_query, scene_name);
    if (id.empty())
	id = addNewPlanningRequest(planning_query, scene_name, "");
    mongo_ros::Metadata metadata(PLANNING_SCENE_ID_NAME, scene_name,
				 MOTION_PLAN_REQUEST_ID_NAME, id);
    robot_trajectory_collection_->insert(result, metadata);
}

void moveit_warehouse::PlanningSceneStorage::getPlanningSceneNames(std::vector<std::string> &names) const
{
    names.clear();
    mongo_ros::Query q;
    std::vector<PlanningSceneWithMetadata> planning_scenes = planning_scene_collection_->pullAllResults(q, true, PLANNING_SCENE_TIME_NAME, true);
    for (std::size_t i = 0; i < planning_scenes.size() ; ++i) 
	if (planning_scenes[i]->metadata.hasField(PLANNING_SCENE_ID_NAME.c_str()))
	    names.push_back(planning_scenes[i]->lookupString(PLANNING_SCENE_ID_NAME));
}

bool moveit_warehouse::PlanningSceneStorage::getPlanningScene(PlanningSceneWithMetadata &scene_m, const std::string &scene_name) const
{
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    std::vector<PlanningSceneWithMetadata> planning_scenes = planning_scene_collection_->pullAllResults(q, false);
    if (planning_scenes.empty())
    {
	ROS_WARN("Planning scene '%s' was not found in the database", scene_name.c_str());
	return false;
    }
    if (planning_scenes.size() > 1)
	ROS_WARN("Multiple planning scenes named '%s' were found. Returning the first one.", scene_name.c_str());
    scene_m = planning_scenes[0];
    return true;
}

void moveit_warehouse::PlanningSceneStorage::getPlanningQueries(std::vector<MotionPlanRequestWithMetadata> &planning_queries, const std::string &scene_name) const
{
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    planning_queries = motion_plan_request_collection_->pullAllResults(q, false);
}

void moveit_warehouse::PlanningSceneStorage::getPlanningResults(std::vector<RobotTrajectoryWithMetadata> &planning_results,
								const moveit_msgs::MotionPlanRequest &planning_query, const std::string &scene_name) const
{
    std::string id = getMotionPlanRequestName(planning_query, scene_name);
    if (id.empty())
	planning_results.clear();
    else
	getPlanningResults(planning_results, id, scene_name);
}

void moveit_warehouse::PlanningSceneStorage::getPlanningResults(std::vector<RobotTrajectoryWithMetadata> &planning_results,
								const std::string &planning_query, const std::string &scene_name) const
{
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    q.append(MOTION_PLAN_REQUEST_ID_NAME, planning_query);
    planning_results = robot_trajectory_collection_->pullAllResults(q, false);
}

void moveit_warehouse::PlanningSceneStorage::removePlanningScene(const std::string &scene_name)
{
    removePlanningSceneQueries(scene_name);
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    unsigned int rem = planning_scene_collection_->removeMessages(q);
    ROS_DEBUG("Removed %u PlanningScene messages (named '%s')", rem, scene_name.c_str());
}

void moveit_warehouse::PlanningSceneStorage::removePlanningSceneQueries(const std::string &scene_name)
{
    mongo_ros::Query q(PLANNING_SCENE_ID_NAME, scene_name);
    unsigned int rem = robot_trajectory_collection_->removeMessages(q);
    ROS_DEBUG("Removed %u RobotTrajectory messages for scene '%s'", rem, scene_name.c_str());
    rem = motion_plan_request_collection_->removeMessages(q);
    ROS_DEBUG("Removed %u MotionPlanRequest messages for scene '%s'", rem, scene_name.c_str());
}
