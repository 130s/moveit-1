Create an IKFast Plugin for MoveIt!
---------

Generates a IKFast kinematics plugin for MoveIt using OpenRave generated cpp files
 
You should have already created a MoveIt! package for your robot, by using the Setup Assistant and following this Tutorial.


Create Plugin
---------

Plugin Package Location

	catkin_create_pkg myrobot_moveit_plugins	

We will create the plugin inside this package

Create new directories:
$ roscd <plugin_package>
$ mkdir include src

Copy the IKFast-generated source file to the newly-created src directory:
$ cp /path/to/ikfast61-output.cpp src/my_robot_manipulator_ikfast_solver.cpp
NOTE: The resulting filename must be of the form: <robot_name>_<group_name>_ikfast_solver.cpp, where group_name is the name of the kinematic chain you defined while using the Wizard. For single-arm robots, this should have been named manipulator as per ROS standards.

Copy the IKFast header file (if available) to the new include directory:
$ cp <openravepy>/ikfast.h include/ikfast.h

Create the plugin source code:
$ rosrun moveit_ikfast_converter create_ikfast_moveit_plugin.py <you_robot_name> <moveit_plugin_package> <planning_group_name>
  - OR -
$ python /path/to/create_ikfast_moveit_plugin.py <you_robot_name> <moveit_plugin_package> <planning_group_name>  (works without ROS)

This will generate a new source file <robot_name>_<group_name>_ikfast_moveit_plugin.cpp in the src/ directory, and modify various configuration files.

Build the plugin library:
$ rosmake <plugin_package>
This will build the new plugin library lib/lib<robot_name>_moveit_arm_kinematics.so.


Usage
---------

The IKFast plugin should function identically to the default KDL IK Solver, but with greatly increased performance. You can switch between the KDL and IKFast solvers using the kinematics_solver parameter in the robot's kinematics.yaml file:

$ roscd my_robot_moveit_config
$ <edit> config/kinematics.yaml

manipulator:
  kinematics_solver: my_robot_manipulator_kinematics/IKFastKinematicsPlugin
      -OR-
  kinematics_solver: kdl_kinematics_plugin/KDLKinematicsPlugin


Test the Plugin
---------

Repeat these tests from the MoveIt! package tutorial to verify that the robot responds as expected in both interactive and planning modes.
