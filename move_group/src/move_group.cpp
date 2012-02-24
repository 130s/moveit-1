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

#include <actionlib/client/action_client.h>
#include <actionlib/server/simple_action_server.h>
#include <moveit_msgs/MoveGroupAction.h>

#include <tf/transform_listener.h>
#include <planning_interface/planning_interface.h>
#include <planning_scene_monitor/planning_scene_monitor.h>
#include <trajectory_execution_ros/trajectory_execution_monitor_ros.h>
  
#include <moveit_msgs/DisplayTrajectory.h>

static const std::string ROBOT_DESCRIPTION="robot_description";      // name of the robot description (a param name, so it can be changed externally)
static const std::string DISPLAY_PATH_PUB_TOPIC = "display_trajectory";

class MoveGroupAction
{
public:
  
  MoveGroupAction(planning_scene_monitor::PlanningSceneMonitor &psm) : 
    nh_("~"), psm_(psm), trajectory_execution_(psm_.getPlanningScene()->getKinematicModel())
  {
    // load the group name
    if (nh_.getParam("group", group_name_))
      ROS_INFO("Starting move_group for group '%s'", group_name_.c_str());
    else
      ROS_FATAL("Group name not specified. Cannot start move_group");
    
    // load the planning plugin
    try
    {
      planner_plugin_loader_.reset(new pluginlib::ClassLoader<planning_interface::Planner>("planning_interface", "planning_interface::Planner"));
    }
    catch(pluginlib::PluginlibException& ex)
    {
      ROS_FATAL_STREAM("Exception while creating planning plugin loader " << ex.what());
    }
    
    nh_.getParam("planning_plugin", planning_plugin_name_);
    const std::vector<std::string> &classes = planner_plugin_loader_->getDeclaredClasses();
    if (planning_plugin_name_.empty() && classes.size() == 1)
    {
      planning_plugin_name_ = classes[0];
      ROS_INFO("No 'planning_plugin' parameter specified, but only '%s' planning plugin is available. Using that one.", planning_plugin_name_.c_str());
    }
    if (planning_plugin_name_.empty() && classes.size() > 1)
    {      
      planning_plugin_name_ = classes[0];   
      ROS_INFO("Multiple planning plugins available. You shuold specify the 'planning_plugin' parameter. Using '%s' for now.", planning_plugin_name_.c_str());
    }
    try
    {
      planner_instance_.reset(planner_plugin_loader_->createClassInstance(planning_plugin_name_));
      planner_instance_->init(psm_.getPlanningScene()->getKinematicModel());
    }
    catch(pluginlib::PluginlibException& ex)
    {
      std::stringstream ss;
      for (std::size_t i = 0 ; i < classes.size() ; ++i)
        ss << classes[i] << " ";
      ROS_FATAL_STREAM("Exception while loading planner '" << planning_plugin_name_ << "': " << ex.what() << std::endl
                       << "Available plugins: " << ss.str());
    }

    display_path_publisher_ = root_nh_.advertise<moveit_msgs::DisplayTrajectory>(DISPLAY_PATH_PUB_TOPIC, 1, true);
    // start the action server
    action_server_.reset(new actionlib::SimpleActionServer<moveit_msgs::MoveGroupAction>(root_nh_, "move_" + group_name_, 
                                                                                         boost::bind(&MoveGroupAction::execute, this, _1), false));
    
    action_server_->start();


    // output status message
    ROS_INFO("MoveGroup action for group '%s' running using planning plugin '%s'", group_name_.c_str(), planning_plugin_name_.c_str());
  }
  
  void execute(const moveit_msgs::MoveGroupGoalConstPtr& goal)
  {
  }
  
  void status(void)
  {
  }
  
private:
  
  ros::NodeHandle                                         root_nh_;
  ros::NodeHandle                                         nh_;
  planning_scene_monitor::PlanningSceneMonitor           &psm_;
  trajectory_execution_ros::TrajectoryExecutionMonitorRos trajectory_execution_;  
  std::string                                             planning_plugin_name_;
  boost::shared_ptr<pluginlib::ClassLoader<planning_interface::Planner> > planner_plugin_loader_;
  boost::shared_ptr<planning_interface::Planner>                          planner_instance_;
  std::string                                                             group_name_;
  boost::shared_ptr<actionlib::SimpleActionServer<moveit_msgs::MoveGroupAction> > action_server_;
  ros::Publisher display_path_publisher_;
};


int main(int argc, char **argv)
{
  ros::init(argc, argv, "move_group", ros::init_options::AnonymousName); // this needs to be fixed so we can have multiple nodes named move_group_<group_name>
  
  ros::AsyncSpinner spinner(1);
  spinner.start();
  
  tf::TransformListener tf;
  planning_scene_monitor::PlanningSceneMonitor psm(ROBOT_DESCRIPTION, &tf);
  if (psm.getPlanningScene()->isConfigured())
  {
    psm.startWorldGeometryMonitor();
    psm.startSceneMonitor();
    psm.startStateMonitor();
    
    MoveGroupAction mga(psm);
    mga.status();
    ros::waitForShutdown();
  }
  else
    ROS_ERROR("Planning scene not configured");
  
  return 0;
}
