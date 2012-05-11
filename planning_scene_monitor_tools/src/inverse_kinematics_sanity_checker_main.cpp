#include <planning_scene_monitor/planning_scene_monitor.h>
#include <kinematics_constraint_aware/inverse_kinematics_sanity_checker.h>
#include <kinematics_plugin_loader/kinematics_plugin_loader_helpers.h>
#include <planning_scene_monitor_tools/kinematic_state_joint_state_publisher.h>

static const std::string VIS_TOPIC_NAME = "inverse_kinematics_sanity_checker";

boost::shared_ptr<KinematicStateJointStatePublisher> joint_state_publisher_;
boost::shared_ptr<tf::TransformListener> transformer_;
boost::shared_ptr<planning_scene_monitor::PlanningSceneMonitor> planning_scene_monitor_;

void publisherFunction(bool joint_states) { 
  ros::WallRate r(10.0);

  while(ros::ok())
  {
    joint_state_publisher_->broadcastRootTransform(planning_scene_monitor_->getPlanningScene()->getCurrentState());
    if(joint_states) {
      joint_state_publisher_->publishKinematicState(planning_scene_monitor_->getPlanningScene()->getCurrentState());
    }
    r.sleep();
  }
}

int main(int argc, char **argv) {

  ros::init(argc, argv, "inverse_kinematics_sanity_checker");

  ros::NodeHandle nh;

  ros::Publisher vis_marker_publisher_ = nh.advertise<visualization_msgs::Marker> (VIS_TOPIC_NAME, 128);
  ros::Publisher vis_marker_array_publisher_ = nh.advertise<visualization_msgs::MarkerArray> (VIS_TOPIC_NAME + "_array", 128);

  boost::shared_ptr<kinematics_plugin_loader::KinematicsPluginLoader> kinematics_plugin_loader_(new kinematics_plugin_loader::KinematicsPluginLoader());
  
  planning_scene_monitor_.reset(new planning_scene_monitor::PlanningSceneMonitor("robot_description", kinematics_plugin_loader_));

  joint_state_publisher_.reset(new KinematicStateJointStatePublisher());

  boost::thread publisher_thread(boost::bind(&publisherFunction, true));

  std::map<std::string, kinematics::KinematicsBasePtr> solver_map;
  kinematics_plugin_loader::generateKinematicsLoaderMap(planning_scene_monitor_->getPlanningScene()->getKinematicModel(),
                                                        planning_scene_monitor_->getPlanningScene()->getSrdfModel(),
                                                        kinematics_plugin_loader_,
                                                        solver_map);

  InverseKinematicsSanityChecker sanity(solver_map,
                                        planning_scene_monitor_->getPlanningScene()->getKinematicModel());

  ros::NodeHandle loc_nh("~");

  std::string group_name;
  if(!loc_nh.getParam("group", group_name)) {
    ROS_WARN_STREAM("Must specify group");
    ros::shutdown();
    exit(-1);
  }
  
  bool normalize = false;
  loc_nh.param("normalize", normalize, false);
  
  int num_samples = 10000;
  loc_nh.param("num_samples", num_samples, 10000);

  std::vector<std::pair<std::vector<double>, std::vector<double> > > wrong_solutions;

  sanity.runTest(group_name,
                 wrong_solutions,
                 num_samples,
                 normalize);

  if(wrong_solutions.size() == 0) {
    ros::shutdown();
    exit(0);
  }

  std_msgs::ColorRGBA cola;
  cola.r = cola.a = 1.0; 

  std_msgs::ColorRGBA colb;
  colb.r = colb.a = 1.0; 

  visualization_msgs::MarkerArray mark;
  
  planning_models::KinematicState& state =  planning_scene_monitor_->getPlanningScene()->getCurrentState();
  planning_models::KinematicState::JointStateGroup* jsg = state.getJointStateGroup(group_name);
  jsg->setStateValues(wrong_solutions[0].first);
  state.getRobotMarkers(cola,
                        "sample",
                        ros::Duration(0.0),
                        mark,
                        planning_scene_monitor_->getPlanningScene()->getSemanticModel()->getGroupLinks(group_name));
  jsg->setStateValues(wrong_solutions[0].second);
  state.getRobotMarkers(colb,
                        "solution",
                        ros::Duration(0.0),
                        mark,
                        planning_scene_monitor_->getPlanningScene()->getSemanticModel()->getGroupLinks(group_name));

  while(ros::ok()) {
    vis_marker_array_publisher_.publish(mark);
    ros::Duration(.5).sleep();
  }

  ros::shutdown();
}
