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

#include <tinyxml.h>
#include <moveit_setup_assistant/tools/srdf_writer.h>

namespace moveit_setup_assistant
{

// ******************************************************************************************
// Constructor
// ******************************************************************************************
SRDFWriter::SRDFWriter()
{
}

// ******************************************************************************************
// Destructor
// ******************************************************************************************
SRDFWriter::~SRDFWriter()
{
}

// ******************************************************************************************
// Load SRDF data from a pre-populated string
// ******************************************************************************************


bool SRDFWriter::initString( const urdf::ModelInterface &robot_model, const std::string &srdf_string )
{
  // Load from SRDF Model
  srdf::Model srdf_model;

  // Error check
  if( !srdf_model.initString( robot_model, srdf_string ) )
  {
    return false; // error loading file. improper format?
  }

  // Copy all read-only data from srdf model to this object
  disabled_collisions_ = srdf_model.getDisabledCollisionPairs();
  groups_ = srdf_model.getGroups();
  virtual_joints_ = srdf_model.getVirtualJoints();
  end_effectors_ = srdf_model.getEndEffectors();
  group_states_ = srdf_model.getGroupStates();

  return true;
}

// ******************************************************************************************
// Generate SRDF XML of all contained data and save to file
// ******************************************************************************************
bool SRDFWriter::writeSRDF( const std::string &file_path )
{
  TiXmlDocument document;
  TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "", "" );  
  document.LinkEndChild( decl ); 
  
  // Convenience comments
  TiXmlComment * comment = new TiXmlComment( "This does not replace URDF, and is not an extension of URDF.\n    This is a format for representing semantic information about the robot structure.\n    A URDF file must exist for this robot as well, where the joints and the links that are referenced are defined\n" );
  document.LinkEndChild( comment );  
  
  // Root
  TiXmlElement* robot_root = new TiXmlElement("robot");
  document.LinkEndChild( robot_root );

  // Add Groups
  createGroupsXML( robot_root );
  
  // Add Group States
  // TODO

  // Add End Effectors
  // TODO

  // Add Virtual Joints
  // TODO

  // Add Disabled Collisions
  createDisabledCollisionsXML( robot_root );

  // Save
  return document.SaveFile( file_path );
}

// ******************************************************************************************
// Generate XML for SRDF groups
// ******************************************************************************************
void SRDFWriter::createGroupsXML( TiXmlElement *root )
{
  // Convenience comments
  TiXmlComment *comment;
  comment = new TiXmlComment( "GROUPS: Representation of a set of joints and links. This can be useful for specifying DOF to plan for, defining arms, end effectors, etc" );  
  root->LinkEndChild( comment );  
  comment = new TiXmlComment( "LINKS: When a link is specified, the parent joint of that link (if it exists) is automatically included" );
  root->LinkEndChild( comment );  
  comment = new TiXmlComment( "JOINTS: When a joint is specified, the child link of that joint (which will always exist) is automatically included" );
  root->LinkEndChild( comment );  
  comment = new TiXmlComment( "CHAINS: When a chain is specified, all the links along the chain (including endpoints) are included in the group. Additionally, all the joints that are parents to included links are also included. This means that joints along the chain and the parent joint of the base link are included in the group");
  root->LinkEndChild( comment );
  comment = new TiXmlComment( "SUBGROUPS: Groups can also be formed by referencing to already defined group names" );
  root->LinkEndChild( comment );  

  // Loop through all of the top groups
  for( std::vector<srdf::Model::Group>::iterator group_it = groups_.begin(); 
       group_it != groups_.end();  ++group_it )
  {
    
    // Create group element
    TiXmlElement *group = new TiXmlElement("group");
    group->SetAttribute("name", group_it->name_ ); // group name
    root->LinkEndChild(group);

    // LINKS
    for( std::vector<std::string>::const_iterator link_it = group_it->links_.begin();
         link_it != group_it->links_.end(); ++link_it )
    {
      TiXmlElement *link = new TiXmlElement("link");
      link->SetAttribute("name", *link_it ); // link name
      group->LinkEndChild( link );
    }

    // JOINTS
    for( std::vector<std::string>::const_iterator joint_it = group_it->joints_.begin();
         joint_it != group_it->joints_.end(); ++joint_it )
    {
      TiXmlElement *joint = new TiXmlElement("joint");
      joint->SetAttribute("name", *joint_it ); // joint name
      group->LinkEndChild( joint );
    }

    // CHAINS
    for( std::vector<std::pair<std::string,std::string> >::const_iterator chain_it = group_it->chains_.begin();
         chain_it != group_it->chains_.end(); ++chain_it )
    {
      TiXmlElement *chain = new TiXmlElement("chain");
      chain->SetAttribute("base_link", chain_it->first );
      chain->SetAttribute("tip_link", chain_it->second );
      group->LinkEndChild( chain );
    }

    // SUBGROUPS
    for( std::vector<std::string>::const_iterator subgroup_it = group_it->subgroups_.begin();
         subgroup_it != group_it->subgroups_.end(); ++subgroup_it )
    {
      TiXmlElement *subgroup = new TiXmlElement("group");
      subgroup->SetAttribute("name", *subgroup_it ); // subgroup name
      group->LinkEndChild( subgroup );
    }
           
  }
}

// ******************************************************************************************
// Generate XML for SRDF disabled collisions of robot link pairs
// ******************************************************************************************
void SRDFWriter::createDisabledCollisionsXML( TiXmlElement *root )
{
  // Convenience comments
  TiXmlComment *comment = new TiXmlComment();
  comment->SetValue( "DISABLE COLLISIONS: By default it is assumed that any link of the robot could potentially come into collision with any other link in the robot. This tag disables collision checking between a specified pair of links. " );  
  root->LinkEndChild( comment );  

  for ( std::vector<srdf::Model::DisabledCollision>::const_iterator pair_it = disabled_collisions_.begin();
        pair_it != disabled_collisions_.end() ; ++pair_it)
  {
    // Create new element for each link pair
    TiXmlElement *link_pair = new TiXmlElement("disabled_collisions");
    link_pair->SetAttribute("link1", pair_it->link1_ );
    link_pair->SetAttribute("link2", pair_it->link2_ );
    link_pair->SetAttribute("reason", pair_it->reason_ );

    root->LinkEndChild( link_pair );
  }
}

}
