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

#include <actionlib/server/simple_action_server.h>
#include <moveit_msgs/MoveGroupAction.h>

#include <tf/transform_listener.h>
#include <planning_scene_monitor/planning_scene_monitor.h>
#include <trajectory_execution_ros/trajectory_execution_monitor_ros.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <move_group/move_group_pipeline.h>
#include <planning_scene_monitor_tools/kinematic_state_joint_state_publisher.h>

static const std::string ROBOT_DESCRIPTION = "robot_description";      // name of the robot description (a param name, so it can be changed externally)
static const std::string DISPLAY_PATH_PUB_TOPIC = "display_trajectory";

class MoveGroupAction
{
public:
  
  enum MoveGroupState
    {
      IDLE,
      PLANNING,
      MONITOR
    };
  
  MoveGroupAction(const planning_scene_monitor::PlanningSceneMonitorConstPtr& psm) : 
    nh_("~"), psm_(psm), move_group_pipeline_(psm), state_(IDLE)
  {
    bool allow_trajectory_execution = true;
    nh_.param("allow_trajectory_execution", allow_trajectory_execution, true);
    
    if(allow_trajectory_execution) {
      bool manage_controllers= false;
      nh_.param("manage_controllers", manage_controllers, true);
      trajectory_execution_.reset(new trajectory_execution_ros::TrajectoryExecutionMonitorRos(psm_->getPlanningScene()->getKinematicModel(),
                                                                                              manage_controllers));
    }
    display_path_publisher_ = root_nh_.advertise<moveit_msgs::DisplayTrajectory>("move_group/" + DISPLAY_PATH_PUB_TOPIC, 1, true);

    // start the action server
    action_server_.reset(new actionlib::SimpleActionServer<moveit_msgs::MoveGroupAction>(root_nh_, "move_group", false));
    action_server_->registerGoalCallback(boost::bind(&MoveGroupAction::goalCallback, this));
    action_server_->registerPreemptCallback(boost::bind(&MoveGroupAction::preemptCallback, this));
    action_server_->start();
  }
  
  void goalCallback(void)
  {
    if (service_goal_thread_)
    {
      terminate_service_thread_ = true;
      service_goal_thread_->join();
      service_goal_thread_.reset();
    }
    goal_ = action_server_->acceptNewGoal();
    if (!goal_)
    {
      ROS_ERROR("Something unexpected happened. No goal found in callback for goal...");
      return;
    }
    terminate_service_thread_ = false;
    service_goal_thread_.reset(new boost::thread(boost::bind(&MoveGroupAction::serviceGoalRequest, this)));
  }
  
  void preemptCallback(void)
  {
    action_server_->setPreempted();
    terminate_service_thread_ = true;
  }

  void serviceGoalRequest(void)
  {
    moveit_msgs::MoveGroupResult action_res;
    moveit_msgs::GetMotionPlan::Request mreq;
    mreq.motion_plan_request = goal_->request;
    if (mreq.motion_plan_request.group_name.empty()) {
      ROS_WARN_STREAM("Must specify group in motion plan request");
      action_res.error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_GROUP_NAME;
      action_server_->setAborted(action_res, "Must specify group in motion plan request");
      setState(IDLE);
      return;    
    }
    setState(PLANNING);
    moveit_msgs::GetMotionPlan::Response mres;
    const planning_scene::PlanningSceneConstPtr &the_scene = 
      planning_scene::PlanningScene::isEmpty(goal_->planning_scene_diff) ? psm_->getPlanningScene() : planning_scene::PlanningScene::diff(psm_->getPlanningScene(), goal_->planning_scene_diff);


    bool solved = move_group_pipeline_.generatePlan(the_scene,
                                                    mreq,
                                                    mres);
        
    if(!solved) {
      action_res.error_code.val = moveit_msgs::MoveItErrorCodes::PLANNING_FAILED;
      action_server_->setAborted(action_res, "No motion plan found. No execution attempted.");
      setState(IDLE);
      return;
    }
    if(!psm_->getPlanningScene()->isPathValid(mres.trajectory_start, mres.trajectory)) {
      action_res.error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_MOTION_PLAN;
      action_server_->setAborted(action_res, "Motion plan was found but it seems to be invalid (possibly due to postprocessing). No execum");
      setState(IDLE);
      return;
    }
    // display the trajectory
    moveit_msgs::DisplayTrajectory disp;
    disp.model_id = psm_->getPlanningScene()->getKinematicModel()->getName();
    disp.trajectory_start = mres.trajectory_start;
    disp.trajectory = mres.trajectory;
    display_path_publisher_.publish(disp);      

    action_res.planned_trajectory = mres.trajectory;
    if(!goal_->plan_only && !trajectory_execution_) {
      ROS_WARN_STREAM("Move group asked for execution and was not configured to allow execution");
    }
    if(goal_->plan_only || !trajectory_execution_) {
      action_res.error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
      action_server_->setSucceeded(action_res, "Solution was found and returned but not executed.");
      setState(IDLE);
      return;
    }
    
    setState(MONITOR);
    execution_complete_ = false;
    
    ROS_INFO_STREAM("Sending joint trajectory");

    trajectory_execution::TrajectoryExecutionRequest ter;
    ter.group_name_ = mreq.motion_plan_request.group_name;      
    ter.trajectory_ = mres.trajectory.joint_trajectory; // \TODO This should take in a RobotTrajectory
    if (trajectory_execution_->executeTrajectory(ter, boost::bind(&MoveGroupAction::doneWithTrajectoryExecution, this, _1)))
    {
      ros::WallDuration d(0.01);
      while (nh_.ok() && !execution_complete_ && !terminate_service_thread_) {
        /// \TODO Check if the remainder of the path is still valid; If not, replan.
        /// We need a callback in the trajectory monitor for this
        d.sleep();
      } 
      if(last_trajectory_execution_data_vector_.size() == 0) {
        ROS_WARN_STREAM("No recorded trajectory for execution");
      } else {
        action_res.executed_trajectory.joint_trajectory = last_trajectory_execution_data_vector_[0].recorded_trajectory_;
        if(last_trajectory_execution_data_vector_[0].result_ == trajectory_execution::SUCCEEDED) {
          action_res.error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
          action_server_->setSucceeded(action_res, "Solution was found and executed.");
        } else {
          action_res.error_code.val = moveit_msgs::MoveItErrorCodes::CONTROL_FAILED;
          action_server_->setAborted(action_res, "Solution found but controller failed during execution");
        }
      }
    } else {
      ROS_INFO_STREAM("Apparently trajectory initialization failed");
      action_res.error_code.val = moveit_msgs::MoveItErrorCodes::CONTROL_FAILED;
      action_server_->setAborted(action_res, "Solution found but could not initiate trajectory execution");
    }
    setState(IDLE);
  }

  bool doneWithTrajectoryExecution(trajectory_execution::TrajectoryExecutionDataVector data)
  {
    last_trajectory_execution_data_vector_ = data;
    execution_complete_ = true;
    return true;
  }
  
  void setState(MoveGroupState state)
  {
    state_ = state;
    switch (state_)
    {
    case IDLE:
      feedback_.state = "IDLE";
      feedback_.time_to_completion = ros::Duration(0.0);
      break;
    case PLANNING:
      feedback_.state = "PLANNING";
      feedback_.time_to_completion = ros::Duration(0.0);
      break;
    case MONITOR:
      feedback_.state = "MONITOR";
      feedback_.time_to_completion = ros::Duration(0.0);
      break;
    }
    action_server_->publishFeedback(feedback_);
  }
  
  void status(void)
  {
    ROS_INFO_STREAM("MoveGroup action running using planning plugin " << move_group_pipeline_.getPlanningPluginName());
  }
  
private:
  
  ros::NodeHandle root_nh_;
  ros::NodeHandle nh_;
  planning_scene_monitor::PlanningSceneMonitorConstPtr psm_;

  move_group::MoveGroupPipeline move_group_pipeline_;

  boost::shared_ptr<actionlib::SimpleActionServer<moveit_msgs::MoveGroupAction> > action_server_;
  moveit_msgs::MoveGroupGoalConstPtr goal_;
  moveit_msgs::MoveGroupFeedback feedback_;

  boost::scoped_ptr<boost::thread> service_goal_thread_;
  
  boost::shared_ptr<trajectory_execution_ros::TrajectoryExecutionMonitorRos> trajectory_execution_;  
  bool terminate_service_thread_;
  bool execution_complete_;
  MoveGroupState state_;
  trajectory_execution::TrajectoryExecutionDataVector last_trajectory_execution_data_vector_;

  ros::Publisher display_path_publisher_;
};

planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor_;
boost::shared_ptr<tf::TransformListener> tf_;
boost::shared_ptr<KinematicStateJointStatePublisher> joint_state_publisher_;

void publisherFunction() { 
  ros::WallRate r(10.0);

  while(ros::ok())
  {
    joint_state_publisher_->broadcastRootTransform(planning_scene_monitor_->getPlanningScene()->getCurrentState());
    joint_state_publisher_->publishKinematicState(planning_scene_monitor_->getPlanningScene()->getCurrentState());
    r.sleep();
  }
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "move_group", ros::init_options::AnonymousName);
  
  ros::AsyncSpinner spinner(1);
  spinner.start();

  ros::NodeHandle nh("~");
  
  bool monitor_robot_state = true;
  nh.param("monitor_robot_state", monitor_robot_state, true);

  if(monitor_robot_state) {
    tf_.reset(new tf::TransformListener());
    planning_scene_monitor_.reset(new planning_scene_monitor::PlanningSceneMonitor(ROBOT_DESCRIPTION, tf_));
  } else {
    planning_scene_monitor_.reset(new planning_scene_monitor::PlanningSceneMonitor(ROBOT_DESCRIPTION));    
  }
  
  if (planning_scene_monitor_->getPlanningScene()->isConfigured())
  {
    if(monitor_robot_state) {
      planning_scene_monitor_->startWorldGeometryMonitor();
      planning_scene_monitor_->startSceneMonitor();
      planning_scene_monitor_->startStateMonitor();
    } else {
      bool publish_joint_states = true;
      nh.param("publish_joint_states", publish_joint_states, true);
      if(publish_joint_states) {  
        joint_state_publisher_.reset(new KinematicStateJointStatePublisher());
        boost::thread publisher_thread(boost::bind(&publisherFunction));
      }
    }
    MoveGroupAction mga(planning_scene_monitor_);
    mga.status();
    ros::waitForShutdown();
  }
  else
    ROS_ERROR("Planning scene not configured");
  
  return 0;
}
