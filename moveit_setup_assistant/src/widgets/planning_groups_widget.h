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

/* Author: Dave Coleman */

#ifndef MOVEIT_ROS_MOVEIT_SETUP_ASSISTANT_WIDGETS_PLANNING_GROUPS_WIDGET_
#define MOVEIT_ROS_MOVEIT_SETUP_ASSISTANT_WIDGETS_PLANNING_GROUPS_WIDGET_

#include <QWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QStackedLayout>
#include "moveit_setup_assistant/tools/moveit_config_data.h"
#include "double_list_widget.h" // for joints, links and subgroups pages
#include "setup_screen_widget.h" // a base class for screens in the setup assistant

// Forward Declaration
class PlanGroupType;

// Custom Type
enum GroupType {
  JOINTS,
  LINKS,
  CHAIN,
  GROUP,
  SUBGROUP
};
  
// ******************************************************************************************
// ******************************************************************************************
// CLASS
// ******************************************************************************************
// ******************************************************************************************
class PlanningGroupsWidget : public SetupScreenWidget
{
  Q_OBJECT

  public:
  // ******************************************************************************************
  // Public Functions
  // ******************************************************************************************

  PlanningGroupsWidget( QWidget *parent, moveit_setup_assistant::MoveItConfigDataPtr config_data );

  void changeScreen( int index );

  /// Recieved when this widget is chosen from the navigation menu
  virtual void focusGiven();

private Q_SLOTS:

  // ******************************************************************************************
  // Slot Event Functions
  // ******************************************************************************************

  /// Displays data in the link_pairs_ data structure into a QtTableWidget
  void loadGroupsTree();

  /// Edit whatever element is selected in the tree view
  void editSelected();

  /// Create a new, empty group
  void addGroup();

  /// Call when joints edit sceen is done and needs to be saved
  void jointsSaveEditing();

  /// Call when edit screen is canceled
  void cancelEditing();

  /// Called when user clicks link part of bottom left label
  void alterTree( const QString &link );

private:

  // ******************************************************************************************
  // Qt Components
  // ******************************************************************************************

  /// Main table for holding groups
  QTreeWidget *groups_tree_;

  /// For changing between table and different add/edit views
  QStackedLayout *stacked_layout_;

  // Stacked Layout SUBPAGES -------------------------------------------

  QWidget *groups_tree_widget_;
  DoubleListWidget *joints_widget_;
  DoubleListWidget *links_widget_;
  DoubleListWidget *subgroups_widget_;
  //ChainWidget *chain_widget_;
  //GroupWidget *group_widget_;

  // ******************************************************************************************
  // Variables
  // ******************************************************************************************

  /// Contains all the configuration data for the setup assistant
  moveit_setup_assistant::MoveItConfigDataPtr config_data_;

  /// Remember what group we are editing when an edit screen is being shown
  std::string current_edit_group_;

  /// Remember what group element we are editing when an edit screen is being shown
  GroupType current_edit_element_;

  // ******************************************************************************************
  // Private Functions
  // ******************************************************************************************

  /// Builds the main screen list widget
  QWidget* createContentsWidget();

  /// Recursively build the SRDF tree
  void loadGroupsTreeRecursive( srdf::Model::Group &group_it, QTreeWidgetItem *parent );

  // Load joints from a specefied group into the edit screen
  void loadJointsScreen( srdf::Model::Group *this_group );


};


// ******************************************************************************************
// ******************************************************************************************
// Metatype Class For Holding Points to Group Parts
// ******************************************************************************************
// ******************************************************************************************

class PlanGroupType
{
public:

  //  explicit PlanGroupType();
  PlanGroupType() {}
  PlanGroupType( srdf::Model::Group *group, const GroupType type );
  virtual ~PlanGroupType() { ; }
  
  // Variables
  srdf::Model::Group *group_;
  GroupType type_;
};

Q_DECLARE_METATYPE(PlanGroupType);


#endif
