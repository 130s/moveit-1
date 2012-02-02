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
#include <planning_scene_monitor/planning_scene_monitor.h>

static const std::string ROBOT_DESCRIPTION="robot_description";

void onSceneUpdate(planning_scene_monitor::PlanningSceneMonitor *psm, moveit_warehouse::PlanningSceneStorage *pss)
{
  ROS_INFO("Received an update to the planning scene...");

  if (!psm->getPlanningScene()->getName().empty())
  {
    moveit_msgs::PlanningScene psmsg;
    psm->getPlanningScene()->getPlanningSceneMsg(psmsg);
    pss->addPlanningScene(psmsg); 
  }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "demo", ros::init_options::AnonymousName);

    ros::AsyncSpinner spinner(1);
    spinner.start();
    
    ros::NodeHandle nh;
    tf::TransformListener tf;
    planning_scene_monitor::PlanningSceneMonitor psm(ROBOT_DESCRIPTION, &tf);
    psm.startSceneMonitor();
    psm.startWorldGeometryMonitor();
    moveit_warehouse::PlanningSceneStorage pss;
    std::vector<std::string> names;
    pss.getPlanningSceneNames(names);
    if (names.empty())
      ROS_INFO("There are no previously stored scenes");
    else
    {
      ROS_INFO("Previously stored scenes:");
      for (std::size_t i = 0 ; i < names.size() ; ++i)
        ROS_INFO(" * %s", names[i].c_str());
    }
    
    
    psm.setUpdateCallback(boost::bind(&onSceneUpdate, &psm, &pss));
    
    ros::waitForShutdown();
    
    return 0;
}
