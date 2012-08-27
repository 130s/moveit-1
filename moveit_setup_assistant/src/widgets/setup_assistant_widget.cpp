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

// SA
#include "setup_screen_widget.h" // a base class for screens in the setup assistant
#include "setup_assistant_widget.h"
// Qt
#include <QStackedLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDebug>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include <QMessageBox>
#include <pluginlib/class_loader.h> // for loading all avail kinematic planners
// Rviz
#include <rviz/render_panel.h>
#include <rviz/visualization_manager.h>
#include <rviz/view_manager.h>
#include <rviz/default_plugin/view_controllers/orbit_view_controller.h>
#include <moveit_rviz_plugin/planning_display.h>
// ROS
#include <ros/master.h> // for checking if roscore is started

namespace moveit_setup_assistant
{

// ******************************************************************************************
// Outer User Interface for MoveIt Configuration Assistant
// ******************************************************************************************
SetupAssistantWidget::SetupAssistantWidget( QWidget *parent, boost::program_options::variables_map args )
  : QWidget( parent )
{
  // Create object to hold all moveit configuration data
  config_data_.reset( new MoveItConfigData() );

  // Set debug mode flag if necessary
  if (args.count("debug"))
    config_data_->debug_ = true;

  // Basic widget container -----------------------------------------
  QHBoxLayout *layout = new QHBoxLayout();
  layout->setAlignment( Qt::AlignTop );

  // Create main content stack for various screens
  main_content_ = new QStackedLayout();

  // Wrap main_content_ with a widget
  middle_frame_ = new QWidget( this );
  middle_frame_->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
  middle_frame_->setLayout( main_content_ );

  // Screens --------------------------------------------------------

  // Start Screen
  ssw_ = new StartScreenWidget( this, config_data_ );
  ssw_->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
  connect( ssw_, SIGNAL( readyToProgress() ), this, SLOT( progressPastStartScreen() ) );
  connect( ssw_, SIGNAL( loadRviz() ), this, SLOT( loadRviz() ) );
  main_content_->addWidget(ssw_);

  // Pass command arg values to start screen
  if (args.count( "urdf_path" ))
  {
    ssw_->urdf_file_->setPath( args["urdf_path"].as<std::string>() );
  }
  if (args.count( "config_pkg" ))
  {
    ssw_->stack_path_->setPath( args["config_pkg"].as<std::string>() );

    // Show this part of screen
    ssw_->select_mode_->btn_exist_->click();
  }

  // Add Navigation Buttons (but do not load widgets yet except start screen)
  nav_name_list_ << "Start";
  nav_name_list_ << "Self-Collisions";
  nav_name_list_ << "Planning Groups";
  nav_name_list_ << "Robot Poses";
  nav_name_list_ << "End Effectors";
  nav_name_list_ << "Virtual Joints";
  nav_name_list_ << "Configuration Files";

  // Navigation Left Pane --------------------------------------------------
  navs_view_ = new NavigationWidget( this );
  navs_view_->setNavs(nav_name_list_);
  navs_view_->setDisabled( true );
  navs_view_->setSelected( 0 ); // start screen

  // Rviz View Right Pane ---------------------------------------------------
  rviz_container_ = new QWidget( this );
  rviz_container_->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  rviz_container_->hide(); // do not show until after the start screen

  // Split screen -----------------------------------------------------
  splitter_ = new QSplitter( Qt::Horizontal, this );
  splitter_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  splitter_->addWidget( navs_view_ );
  splitter_->addWidget( middle_frame_ );
  splitter_->addWidget( rviz_container_ );
  splitter_->setHandleWidth( 6 );
  //splitter_->setCollapsible( 0, false ); // don't let navigation collapse
  layout->addWidget( splitter_ );

  // Add event for switching between screens -------------------------
  connect( navs_view_, SIGNAL(clicked(const QModelIndex&)), this, SLOT(navigationClicked(const QModelIndex&)) );

  // Final Layout Setup ---------------------------------------------
  this->setLayout(layout);

  // Title
  this->setWindowTitle("MoveIt Setup Assistant"); // title of window

  // Show screen before message
  QApplication::processEvents();

  // Check that ROS Core is running
  bool do_check = true;
  while( do_check )
  {
    if( ! ros::master::check() )
    {
      // roscore is not running
      QMessageBox::warning( this, "ROS Error",
                            "ROS Core does not appear to be started. Be sure to run the command 'roscore' at command line before using this application.");
    }
    else
    {
      do_check = false;
    }
  }
}

// ******************************************************************************************
// Decontructor
// ******************************************************************************************
SetupAssistantWidget::~SetupAssistantWidget()
{
  if( rviz_manager_ != NULL )
    rviz_manager_->removeAllDisplays();
  if ( rviz_render_panel_ != NULL )
    delete rviz_render_panel_;
  if ( rviz_manager_ != NULL )
    delete rviz_manager_;
}

// ******************************************************************************************
// Change screens of Setup Assistant
// ******************************************************************************************
void SetupAssistantWidget::navigationClicked( const QModelIndex& index )
{
  // Convert QModelIndex to int
  moveToScreen( index.row() );
}

// ******************************************************************************************
// Change screens
// ******************************************************************************************
void SetupAssistantWidget::moveToScreen( const int index )
{
  // Use this static variable to prevent double clicks on navigation from slowing down system
  static int current_index = 0;

  if( current_index != index )
  {
    current_index = index;

    //rviz_container_->show();

    // Unhighlight anything on robot
    unhighlightAll();

    // Change screens
    main_content_->setCurrentIndex( index );

    // Send the focus given command to the screen widget
    SetupScreenWidget *ssw = qobject_cast< SetupScreenWidget* >( main_content_->widget( index ) );
    ssw->focusGiven();

    // Change navigation selected option
    navs_view_->setSelected( index ); // Select first item in list
  }
}

// ******************************************************************************************
// Loads other windows, enables navigation and goes to screen 2
// ******************************************************************************************
void SetupAssistantWidget::progressPastStartScreen()
{
  // Load all widgets ------------------------------------------------

  // Self-Collisions
  dcw_ = new DefaultCollisionsWidget( this, config_data_);
  main_content_->addWidget( dcw_ );
  connect( dcw_, SIGNAL( highlightLink( const std::string& ) ), this, SLOT( highlightLink( const std::string& ) ) );
  connect( dcw_, SIGNAL( highlightGroup( const std::string& ) ), this, SLOT( highlightGroup( const std::string& ) ) );
  connect( dcw_, SIGNAL( unhighlightAll() ), this, SLOT( unhighlightAll() ) );

  // Planning Groups
  pgw_ = new PlanningGroupsWidget( this, config_data_ );
  main_content_->addWidget(pgw_);
  connect( pgw_, SIGNAL( isModal( bool ) ), this, SLOT( setModalMode( bool ) ) );
  connect( pgw_, SIGNAL( highlightLink( const std::string& ) ), this, SLOT( highlightLink( const std::string& ) ) );
  connect( pgw_, SIGNAL( highlightGroup( const std::string& ) ), this, SLOT( highlightGroup( const std::string& ) ) );
  connect( pgw_, SIGNAL( unhighlightAll() ), this, SLOT( unhighlightAll() ) );

  // Robot Poses
  rpw_ = new RobotPosesWidget( this, config_data_ );
  main_content_->addWidget(rpw_);
  connect( rpw_, SIGNAL( isModal( bool ) ), this, SLOT( setModalMode( bool ) ) );
  connect( rpw_, SIGNAL( highlightLink( const std::string& ) ), this, SLOT( highlightLink( const std::string& ) ) );
  connect( rpw_, SIGNAL( highlightGroup( const std::string& ) ), this, SLOT( highlightGroup( const std::string& ) ) );
  connect( rpw_, SIGNAL( unhighlightAll() ), this, SLOT( unhighlightAll() ) );

  // End Effectors
  efw_ = new EndEffectorsWidget( this, config_data_ );
  main_content_->addWidget(efw_);
  connect( efw_, SIGNAL( isModal( bool ) ), this, SLOT( setModalMode( bool ) ) );
  connect( efw_, SIGNAL( highlightLink( const std::string& ) ), this, SLOT( highlightLink( const std::string& ) ) );
  connect( efw_, SIGNAL( highlightGroup( const std::string& ) ), this, SLOT( highlightGroup( const std::string& ) ) );
  connect( efw_, SIGNAL( unhighlightAll() ), this, SLOT( unhighlightAll() ) );

  // Virtual Joints
  vjw_ = new VirtualJointsWidget( this, config_data_ );
  main_content_->addWidget(vjw_);
  connect( vjw_, SIGNAL( isModal( bool ) ), this, SLOT( setModalMode( bool ) ) );
  connect( vjw_, SIGNAL( highlightLink( const std::string& ) ), this, SLOT( highlightLink( const std::string& ) ) );
  connect( vjw_, SIGNAL( highlightGroup( const std::string& ) ), this, SLOT( highlightGroup( const std::string& ) ) );
  connect( vjw_, SIGNAL( unhighlightAll() ), this, SLOT( unhighlightAll() ) );

  // Configuration Files
  cfw_ = new ConfigurationFilesWidget( this, config_data_ );
  main_content_->addWidget(cfw_);

  // Enable all nav buttons -------------------------------------------
  for( int i = 0; i < nav_name_list_.count(); ++i)
  {
    navs_view_->setEnabled( i, true );
  }

  // Enable navigation
  navs_view_->setDisabled( false );

  // Replace logo with Rviz screen
  rviz_container_->show();
}

// ******************************************************************************************
// Ping ROS on internval
// ******************************************************************************************
void SetupAssistantWidget::updateTimer()
{
  ros::spinOnce(); // keep ROS node alive
}

// ******************************************************************************************
// Load Rviz once we have a robot description ready
// ******************************************************************************************
void SetupAssistantWidget::loadRviz()
{
  // Create rviz frame
  rviz_render_panel_ = new rviz::RenderPanel();
  rviz_render_panel_->setMinimumWidth( 200 );
  rviz_render_panel_->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );

  rviz_manager_ = new rviz::VisualizationManager( rviz_render_panel_ );
  rviz_render_panel_->initialize( rviz_manager_->getSceneManager(), rviz_manager_ );
  rviz_manager_->initialize();
  rviz_manager_->startUpdate();

  // Set the fixed and target frame
  rviz_manager_->setFixedFrame( QString::fromStdString( config_data_->getPlanningScene()->getPlanningFrame() ) );

  // Create the MoveIt Rviz Plugin and attach to display
  planning_display_ = new moveit_rviz_plugin::PlanningDisplay();
  planning_display_->setName( "Motion Planning" );
  rviz_manager_->addDisplay( planning_display_, true );

  // Turn off planned path
  planning_display_->displayRobotPath( false );

  // Set the topic on which the moveit_msgs::PlanningScene messages are recieved
  planning_display_->setPlanningSceneTopic( MOVEIT_PLANNING_SCENE );

  // Set robot description
  planning_display_->setRobotDescription( ROBOT_DESCRIPTION );

  // Zoom into robot
  rviz::ViewController* view = rviz_manager_->getViewManager()->getCurrent();
  view->subProp( "Distance" )->setValue( 4.0f );

  // Add Rviz to Planning Groups Widget
  QHBoxLayout *rviz_layout = new QHBoxLayout();
  rviz_layout->addWidget( rviz_render_panel_ );
  rviz_container_->setLayout( rviz_layout );

  rviz_container_->show();
}

// ******************************************************************************************
// Highlight a robot link
// ******************************************************************************************
void SetupAssistantWidget::highlightLink( const std::string& link_name )
{
  planning_display_->setLinkColor( link_name, 1, 0, 0 );
}

// ******************************************************************************************
// Highlight a robot group
// ******************************************************************************************
void SetupAssistantWidget::highlightGroup( const std::string& group_name )
{
  // Highlight the selected planning group by looping through the links

  const planning_models::KinematicModel::JointModelGroup *joint_model_group =
    config_data_->getKinematicModel()->getJointModelGroup( group_name );

  std::vector<const planning_models::KinematicModel::LinkModel*> link_models = joint_model_group->getLinkModels();

  // Iterate through the links
  for( std::vector<const planning_models::KinematicModel::LinkModel*>::const_iterator link_it = link_models.begin();
       link_it < link_models.end(); ++link_it )
  {
    highlightLink( (*link_it)->getName() );
  }
}

// ******************************************************************************************
// Unhighlight all robot links
// ******************************************************************************************
void SetupAssistantWidget::unhighlightAll()
{
  // Get the names of the all links robot
  const std::vector<std::string> links = config_data_->getKinematicModel()->getLinkModelNames();

  // Quit if no links found
  if( links.empty() )
  {
    return;
  }

  // Iterate through the links
  for( std::vector<std::string>::const_iterator link_it = links.begin();
       link_it < links.end(); ++link_it )
  {
    planning_display_->unsetLinkColor( *link_it );
  }

}

// ******************************************************************************************
// Qt close event function for reminding user to save
// ******************************************************************************************
void SetupAssistantWidget::closeEvent( QCloseEvent * event )
{
  // Only prompt to close if not in debug mode
  if( !config_data_->debug_ )
  {
    if( QMessageBox::question( this, "Exit Setup Assistant",
                               QString("Are you sure you want to exit the MoveIt Setup Assistant?"),
                               QMessageBox::Ok | QMessageBox::Cancel)
        == QMessageBox::Cancel )
    {
      event->ignore();
      return;
    }
  }

  // Shutdown app
  event->accept();
}

// ******************************************************************************************
// Qt Error Handling - TODO
// ******************************************************************************************
bool SetupAssistantWidget::notify( QObject * reciever, QEvent * event )
{
  QMessageBox::critical( this, "Error", "An error occurred and was caught by Qt notify event handler.", QMessageBox::Ok);

  return false;
}

// ******************************************************************************************
// Change the widget modal state based on subwidgets state
// ******************************************************************************************
void SetupAssistantWidget::setModalMode( bool isModal )
{
  navs_view_->setDisabled( isModal );
  
  for( int i = 0; i < nav_name_list_.count(); ++i)
  {
    navs_view_->setEnabled( i, !isModal );
  }
}


} // namespace
