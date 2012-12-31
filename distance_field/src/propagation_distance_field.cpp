/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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

/* Author: Mrinal Kalakrishnan, Ken Anderson */

#include <moveit/distance_field/propagation_distance_field.h>
#include <visualization_msgs/Marker.h>
#include <console_bridge/console.h>

namespace distance_field
{

PropagationDistanceField::PropagationDistanceField(double size_x, double size_y, double size_z, 
                                                   double resolution,
                                                   double origin_x, double origin_y, double origin_z, 
                                                   double max_distance,
                                                   bool propagate_negative):
  DistanceField(size_x, size_y, size_z, resolution, origin_x, origin_y, origin_z),
  voxel_grid_(size_x, size_y, size_z, resolution, origin_x, origin_y, origin_z, PropDistanceFieldVoxel(max_distance,0)),
  propagate_negative_(propagate_negative)
{
  max_distance_ = max_distance;
  int max_dist_int = ceil(max_distance_/resolution);
  max_distance_sq_ = (max_dist_int*max_dist_int);
  initNeighborhoods();
  
  bucket_queue_.resize(max_distance_sq_+1);
  negative_bucket_queue_.resize(max_distance_sq_+1);
  
  // create a sqrt table:
  sqrt_table_.resize(max_distance_sq_+1);
  for (int i=0; i<=max_distance_sq_; ++i)
    sqrt_table_[i] = sqrt(double(i))*resolution;

  reset();
}

PropagationDistanceField::~PropagationDistanceField()
{
}

int PropagationDistanceField::eucDistSq(Eigen::Vector3i point1, Eigen::Vector3i point2)
{
  int dx = point1.x() - point2.x();
  int dy = point1.y() - point2.y();
  int dz = point1.z() - point2.z();
  return dx*dx + dy*dy + dz*dz;
}

void PropagationDistanceField::print(const VoxelSet & set)
{
  logDebug( "[" );
  VoxelSet::const_iterator it;
  for( it=set.begin(); it!=set.end(); ++it)
  {
    Eigen::Vector3i loc1 = *it;
    logDebug( "%d, %d, %d ", loc1.x(), loc1.y(), loc1.z() );
  }
  logDebug( "] size=%u\n", (unsigned int)set.size());
}

void PropagationDistanceField::print(const EigenSTL::vector_Vector3d& points)
{
  logDebug( "[" );
  EigenSTL::vector_Vector3d::const_iterator it;
  for( it=points.begin(); it!=points.end(); ++it)
  {
    Eigen::Vector3d loc1 = *it;  
    logDebug( "%g, %g, %g ", loc1.x(), loc1.y(), loc1.z() );
  }  
  logDebug( "] size=%u\n", (unsigned int)points.size());
}


void PropagationDistanceField::updatePointsInField(const EigenSTL::vector_Vector3d& points, bool iterative)
{
  VoxelSet points_added;
  VoxelSet points_removed(object_voxel_locations_);

  logDebug( "obstacle_voxel_locations_=" );
  print(object_voxel_locations_);
  logDebug( "points=" );
  print(points);

  if( iterative )
  {

    // Compare and figure out what points are new,
    // and what points are to be deleted
    for( unsigned int i=0; i<points.size(); i++)
    {
      // Convert to voxel coordinates
      Eigen::Vector3i voxel_loc;
      bool valid = worldToGrid(points[i].x(), points[i].y(), points[i].z(),
                               voxel_loc.x(), voxel_loc.y(), voxel_loc.z() );
      if( valid )
      {
        logDebug( " checking for %d, %d, %d\n", voxel_loc.x(), voxel_loc.y(), voxel_loc.z() );
        bool already_obstacle_voxel = ( object_voxel_locations_.find(voxel_loc) != object_voxel_locations_.end() );
        if( !already_obstacle_voxel )
        {
          logDebug( " didn't find it" );
          // Not already in set of existing obstacles, so add to voxel list
          object_voxel_locations_.insert(voxel_loc);

          // Add point to the set for expansion
          points_added.insert(voxel_loc);
        }
        else
        {
          logDebug( " found it" );
          // Already an existing obstacle, so take off removal list
          points_removed.erase(voxel_loc);
        }
      }
    }

    removeObstacleVoxels( points_removed );
    addNewObstacleVoxels( points_added );
  }

  else	// !iterative
  {
    reset();

    for( unsigned int i=0; i<points.size(); i++)
    {
      // Convert to voxel coordinates
      Eigen::Vector3i voxel_loc;
      bool valid = worldToGrid(points[i].x(), points[i].y(), points[i].z(),
                               voxel_loc.x(), voxel_loc.y(), voxel_loc.z() );
      if( valid )
      {
        object_voxel_locations_.insert(voxel_loc);
        points_added.insert(voxel_loc);
      }
    }
    addNewObstacleVoxels( points_added );
  }

  logDebug( "new=" );
  print(points_added);
  logDebug( "removed=" );
  print(points_removed);
  logDebug( "obstacle_voxel_locations_=" );
  print(object_voxel_locations_);
  logDebug("");  
}

void PropagationDistanceField::addPointsToField(const EigenSTL::vector_Vector3d& points)
{
  VoxelSet voxel_locs;

  for( unsigned int i=0; i<points.size(); i++)
  {
    // Convert to voxel coordinates
    Eigen::Vector3i voxel_loc;
    bool valid = worldToGrid(points[i].x(), points[i].y(), points[i].z(),
                             voxel_loc.x(), voxel_loc.y(), voxel_loc.z() );

    if( valid )
    {
      bool already_obstacle_voxel = ( object_voxel_locations_.find(voxel_loc) != object_voxel_locations_.end() );
      if( !already_obstacle_voxel )
      {
        // Not already in set of existing obstacles, so add to voxel list
        object_voxel_locations_.insert(voxel_loc);

        // Add point to the queue for expansion
        voxel_locs.insert(voxel_loc);
      }
    }
  }

  addNewObstacleVoxels( voxel_locs );
}

void PropagationDistanceField::removePointsFromField(const EigenSTL::vector_Vector3d& points)
{
  VoxelSet voxel_locs;

  for( unsigned int i=0; i<points.size(); i++)
  {
    // Convert to voxel coordinates
    Eigen::Vector3i voxel_loc;
    bool valid = worldToGrid(points[i].x(), points[i].y(), points[i].z(),
                             voxel_loc.x(), voxel_loc.y(), voxel_loc.z() );

    if( valid )
    {
      bool already_obstacle_voxel = ( object_voxel_locations_.find(voxel_loc) != object_voxel_locations_.end() );
      if( already_obstacle_voxel )
      {
        // Not already in set of existing obstacles, so add to voxel list
        //object_voxel_locations_.erase(voxel_loc);

        // Add point to the queue for expansion
        voxel_locs.insert(voxel_loc);
      }
    }
  }

  removeObstacleVoxels( voxel_locs );
}

void PropagationDistanceField::addNewObstacleVoxels(const VoxelSet& locations)
{
  int initial_update_direction = getDirectionNumber(0,0,0);
  bucket_queue_[0].reserve(locations.size());
  if(propagate_negative_) {
    negative_bucket_queue_[0].reserve(locations.size());
  }

  VoxelSet::const_iterator it = locations.begin();
  for( it=locations.begin(); it!=locations.end(); ++it)
  {
    Eigen::Vector3i loc = (*it);
    bool valid = isCellValid( loc.x(),loc.y(),loc.z() );
    if (!valid) {
      continue;
    }
    PropDistanceFieldVoxel& voxel = voxel_grid_.getCell( loc.x(),loc.y(),loc.z() );
    voxel.distance_square_ = 0;
    voxel.closest_point_ = loc;
    voxel.update_direction_ = initial_update_direction;
    bucket_queue_[0].push_back(loc);
    if(propagate_negative_) {
      voxel.negative_distance_square_ = max_distance_sq_;
      voxel.closest_negative_point_.x() = PropDistanceFieldVoxel::UNINITIALIZED;
      voxel.closest_negative_point_.y() = PropDistanceFieldVoxel::UNINITIALIZED;
      voxel.closest_negative_point_.z() = PropDistanceFieldVoxel::UNINITIALIZED;
    }
  }
  propagatePositive();
  
  if(propagate_negative_) {
    VoxelSet::const_iterator it = locations.begin();
    for( it=locations.begin(); it!=locations.end(); ++it)
    {
      Eigen::Vector3i loc = *it;
      bool valid = isCellValid( loc.x(), loc.y(), loc.z());
      if (!valid)
        continue;
      PropDistanceFieldVoxel& voxel = voxel_grid_.getCell(loc.x(), loc.y(), loc.z());
      for( int neighbor=0; neighbor<27; neighbor++ )
      {
        Eigen::Vector3i diff = getLocationDifference(neighbor);
        Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );
        
        if( isCellValid(nloc.x(), nloc.y(), nloc.z()) )
        {
          PropDistanceFieldVoxel& nvoxel = voxel_grid_.getCell(nloc.x(), nloc.y(), nloc.z());
          if(nvoxel.closest_negative_point_.x() != PropDistanceFieldVoxel::UNINITIALIZED)
          {
            std::cout << "Adding neighbor " << nloc.x() << " " << nloc.y() << " " << nloc.z() << std::endl;
            nvoxel.negative_update_direction_ = initial_update_direction;
            nvoxel.closest_negative_point_.x() == PropDistanceFieldVoxel::UNINITIALIZED;
            nvoxel.closest_negative_point_.y() == PropDistanceFieldVoxel::UNINITIALIZED;
            nvoxel.closest_negative_point_.z() == PropDistanceFieldVoxel::UNINITIALIZED;
            negative_bucket_queue_[0].push_back(nloc);
          }
        }
      }
    }
    propagateNegative();
  }
}

void PropagationDistanceField::removeObstacleVoxels(const VoxelSet& locations )
{
  std::vector<Eigen::Vector3i> stack;
  std::vector<Eigen::Vector3i> negative_stack;
  int initial_update_direction = getDirectionNumber(0,0,0);

  stack.reserve(getXNumCells() * getYNumCells() * getZNumCells());
  bucket_queue_[0].reserve(locations.size());
  if(propagate_negative_) {
    negative_stack.reserve(getXNumCells() * getYNumCells() * getZNumCells());
    negative_bucket_queue_[0].reserve(locations.size());
  }

  // First reset the obstacle voxels,
  VoxelSet::const_iterator it = locations.begin();
  for( it=locations.begin(); it!=locations.end(); ++it)
  {
    Eigen::Vector3i loc = *it;
    bool valid = isCellValid( loc.x(), loc.y(), loc.z());
    if (!valid)
      continue;
    PropDistanceFieldVoxel& voxel = voxel_grid_.getCell(loc.x(), loc.y(), loc.z());
    voxel.distance_square_ = max_distance_sq_;
    voxel.closest_point_ = loc;
    voxel.update_direction_ = initial_update_direction;
    stack.push_back(loc);
    object_voxel_locations_.erase(loc);
    if(propagate_negative_) {
      voxel.negative_distance_square_ = 0.0;
      voxel.closest_negative_point_ = loc;
      voxel.negative_update_direction_ = initial_update_direction;
      negative_stack.push_back(loc);
    }
  }

  // Reset all neighbors who's closest point is now gone.
  while(stack.size() > 0)
  {
    Eigen::Vector3i loc = stack.back();
    stack.pop_back();

    for( int neighbor=0; neighbor<27; neighbor++ )
    {
      Eigen::Vector3i diff = getLocationDifference(neighbor);
      Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );

      if( isCellValid(nloc.x(), nloc.y(), nloc.z()) )
      {
        PropDistanceFieldVoxel& nvoxel = voxel_grid_.getCell(nloc.x(), nloc.y(), nloc.z());
        Eigen::Vector3i& close_point = nvoxel.closest_point_;
        if( !isCellValid( close_point.x(), close_point.y(), close_point.z() ) )
        {
          close_point = nloc;
        }
        PropDistanceFieldVoxel& closest_point_voxel = voxel_grid_.getCell( close_point.x(), close_point.y(), close_point.z() );

        if( closest_point_voxel.distance_square_ != 0 )
        {	// closest point no longer exists
          if( nvoxel.distance_square_!=max_distance_sq_)
          {
            nvoxel.distance_square_ = max_distance_sq_;
            nvoxel.closest_point_ = nloc;
            nvoxel.update_direction_ = initial_update_direction;
            stack.push_back(nloc);
          }
        }
        else
        {	// add to queue so we can propagate the values
          bucket_queue_[0].push_back(nloc);
        }
      }
    }
  }
  propagatePositive();

  if(propagate_negative_) {
    while(negative_stack.size() > 0)
    {
      Eigen::Vector3i loc = negative_stack.back();
      negative_stack.pop_back();
      
      for( int neighbor=0; neighbor<27; neighbor++ )
      {
        Eigen::Vector3i diff = getLocationDifference(neighbor);
        Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );

        if( isCellValid(nloc.x(), nloc.y(), nloc.z()) )
        {
          PropDistanceFieldVoxel& nvoxel = voxel_grid_.getCell(nloc.x(), nloc.y(), nloc.z());
          Eigen::Vector3i& close_point = nvoxel.closest_negative_point_;
          if( !isCellValid( close_point.x(), close_point.y(), close_point.z() ) )
          {
            close_point = nloc;
          }
          PropDistanceFieldVoxel& closest_point_voxel = voxel_grid_.getCell( close_point.x(), close_point.y(), close_point.z() );

          if( closest_point_voxel.negative_distance_square_ != 0 )
          {	// closest point no longer exists
            if( nvoxel.negative_distance_square_!=max_distance_sq_)
            {
              nvoxel.negative_distance_square_ = max_distance_sq_;
              nvoxel.closest_negative_point_ = nloc;
              nvoxel.update_direction_ = initial_update_direction;
              negative_stack.push_back(nloc);
            }
          }
          else
          {	// add to queue so we can propagate the values
            negative_bucket_queue_[0].push_back(nloc);
          }
        }
      }
    }
    propagateNegative();
  }
    
  //   VoxelSet::const_iterator it = locations.begin();
  //   for( it=locations.begin(); it!=locations.end(); ++it)
  //   {
  //     Eigen::Vector3i loc = *it;
  //     bool valid = isCellValid( loc.x(), loc.y(), loc.z());
  //     if (!valid)
  //       continue;
  //     PropDistanceFieldVoxel& voxel = voxel_grid_.getCell(loc.x(), loc.y(), loc.z());
  //     for( int neighbor=0; neighbor<27; neighbor++ )
  //     {
  //       Eigen::Vector3i diff = getLocationDifference(neighbor);
  //       Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );
        
  //       if( isCellValid(nloc.x(), nloc.y(), nloc.z()) )
  //       {
  //         PropDistanceFieldVoxel& nvoxel = voxel_grid_.getCell(nloc.x(), nloc.y(), nloc.z());
  //         if(nvoxel.negative_distance_square_ != 0 && nvoxel.negative_distance_square_ != PropDistanceFieldVoxel::UNINITIALIZED)
  //         {
  //           std::cout << "Adding neighbor " << nloc.x() << " " << nloc.y() << " " << nloc.z() << std::endl;
  //           //all neighbors are now one if they weren't 0 before
  //           nvoxel.negative_distance_square_ = 1;
  //           nvoxel.negative_update_direction_ = initial_update_direction;
  //           nvoxel.closest_negative_point_ = loc;
  //           negative_bucket_queue_[0].push_back(nloc);
  //         }
  //       }
  //     }
  //   }
  //   propagateNegative();
  // }
}

void PropagationDistanceField::propagatePositive()
{

  // now process the queue:
  for (unsigned int i=0; i<bucket_queue_.size(); ++i)
  {
    std::vector<Eigen::Vector3i>::iterator list_it = bucket_queue_[i].begin();
    while(list_it!=bucket_queue_[i].end())
    {
      Eigen::Vector3i loc = *list_it;
      PropDistanceFieldVoxel* vptr = &voxel_grid_.getCell(loc.x(), loc.y(), loc.z());

      // select the neighborhood list based on the update direction:
      std::vector<Eigen::Vector3i >* neighborhood;
      int D = i;
      if (D>1)
        D=1;
      // avoid a possible segfault situation:
      if (vptr->update_direction_<0 || vptr->update_direction_>26)
      {
        //     ROS_WARN("Invalid update direction detected: %d", vptr->update_direction_);
        ++list_it;
        continue;
      }

      neighborhood = &neighborhoods_[D][vptr->update_direction_];

      for (unsigned int n=0; n<neighborhood->size(); n++)
      {
        Eigen::Vector3i diff = (*neighborhood)[n];
        Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );
        if (!isCellValid(nloc.x(), nloc.y(), nloc.z()) )
          continue;

        // the real update code:
        // calculate the neighbor's new distance based on my closest filled voxel:
        PropDistanceFieldVoxel* neighbor = &voxel_grid_.getCell(nloc.x(),nloc.y(),nloc.z());
        int new_distance_sq = eucDistSq(vptr->closest_point_, nloc);
        if (new_distance_sq > max_distance_sq_)
          continue;
        if (new_distance_sq < neighbor->distance_square_)
        {
          // update the neighboring voxel
          neighbor->distance_square_ = new_distance_sq;
          neighbor->closest_point_ = vptr->closest_point_;
          neighbor->update_direction_ = getDirectionNumber(diff.x(), diff.y(), diff.z());

          // and put it in the queue:
          bucket_queue_[new_distance_sq].push_back(nloc);
        }
      }

      ++list_it;
    }
    bucket_queue_[i].clear();
  }
}

void PropagationDistanceField::propagateNegative()
{
  
  // now process the queue:
  for (unsigned int i=0; i<negative_bucket_queue_.size(); ++i)
  {
    //std::cout << " Here " << i << " " << negative_bucket_queue_[i].size() << std::endl;
  
    std::vector<Eigen::Vector3i>::iterator list_it = negative_bucket_queue_[i].begin();
    while(list_it!=negative_bucket_queue_[i].end())
    {
      Eigen::Vector3i loc = *list_it;
      PropDistanceFieldVoxel* vptr = &voxel_grid_.getCell(loc.x(), loc.y(), loc.z());

      // select the neighborhood list based on the update direction:
      std::vector<Eigen::Vector3i >* neighborhood;
      int D = i;
      if (D>1)
        D=1;
      // avoid a possible segfault situation:
      if (vptr->negative_update_direction_<0 || vptr->negative_update_direction_>26)
      {
        //     ROS_WARN("Invalid update direction detected: %d", vptr->update_direction_);
        ++list_it;
        continue;
      }

      neighborhood = &neighborhoods_[D][vptr->negative_update_direction_];

      for (unsigned int n=0; n<neighborhood->size(); n++)
      {
        Eigen::Vector3i diff = (*neighborhood)[n];
        Eigen::Vector3i nloc( loc.x() + diff.x(), loc.y() + diff.y(), loc.z() + diff.z() );
        if (!isCellValid(nloc.x(), nloc.y(), nloc.z()) )
          continue;

        // the real update code:
        // calculate the neighbor's new distance based on my closest filled voxel:
        PropDistanceFieldVoxel* neighbor = &voxel_grid_.getCell(nloc.x(),nloc.y(),nloc.z());
        int new_distance_sq = eucDistSq(vptr->closest_negative_point_, nloc);
        if (new_distance_sq > max_distance_sq_)
          continue;
        //std::cout << "Looking at " << nloc.x() << " " << nloc.y() << " " << nloc.z() << " " << new_distance_sq << " " << neighbor->negative_distance_square_ << std::endl;
        if (new_distance_sq < neighbor->negative_distance_square_)
        {
          std::cout << "Updating " << nloc.x() << " " << nloc.y() << " " << nloc.z() << " " << new_distance_sq << std::endl;
          // update the neighboring voxel
          neighbor->negative_distance_square_ = new_distance_sq;
          neighbor->closest_negative_point_ = vptr->closest_negative_point_;
          neighbor->negative_update_direction_ = getDirectionNumber(diff.x(), diff.y(), diff.z());

          // and put it in the queue:
          negative_bucket_queue_[new_distance_sq].push_back(nloc);
        }
      }

      ++list_it;
    }
    negative_bucket_queue_[i].clear();
  }
}

void PropagationDistanceField::reset()
{
  voxel_grid_.reset(PropDistanceFieldVoxel(max_distance_sq_,0));
  for(int x = 0; x < getXNumCells(); x++)
  {
    for(int y = 0; y < getYNumCells(); y++)
    {
      for(int z = 0; z < getZNumCells(); z++)
      {
        PropDistanceFieldVoxel& voxel = voxel_grid_.getCell(x,y,z);
        voxel.closest_negative_point_.x() = x;
        voxel.closest_negative_point_.y() = y;
        voxel.closest_negative_point_.z() = z;
        voxel.negative_distance_square_ = 0;
      }
    }
  }
  object_voxel_locations_.clear();
}

void PropagationDistanceField::initNeighborhoods()
{
  // first initialize the direction number mapping:
  direction_number_to_direction_.resize(27);
  for (int dx=-1; dx<=1; ++dx)
  {
    for (int dy=-1; dy<=1; ++dy)
    {
      for (int dz=-1; dz<=1; ++dz)
      {
        int direction_number = getDirectionNumber(dx, dy, dz);
        Eigen::Vector3i n_point( dx, dy, dz);
        direction_number_to_direction_[direction_number] = n_point;
      }
    }
  }

  neighborhoods_.resize(2);
  for (int n=0; n<2; n++)
  {
    neighborhoods_[n].resize(27);
    // source directions
    for (int dx=-1; dx<=1; ++dx)
    {
      for (int dy=-1; dy<=1; ++dy)
      {
        for (int dz=-1; dz<=1; ++dz)
        {
          int direction_number = getDirectionNumber(dx, dy, dz);
          // target directions:
          for (int tdx=-1; tdx<=1; ++tdx)
          {
            for (int tdy=-1; tdy<=1; ++tdy)
            {
              for (int tdz=-1; tdz<=1; ++tdz)
              {
                if (tdx==0 && tdy==0 && tdz==0)
                  continue;
                if (n>=1)
                {
                  if ((abs(tdx) + abs(tdy) + abs(tdz))!=1)
                    continue;
                  if (dx*tdx<0 || dy*tdy<0 || dz*tdz <0)
                    continue;
                }
                Eigen::Vector3i n_point(tdx,tdy,tdz);
                neighborhoods_[n][direction_number].push_back(n_point);
              }
            }
          }
          //printf("n=%d, dx=%d, dy=%d, dz=%d, neighbors = %d\n", n, dx, dy, dz, neighborhoods_[n][direction_number].size());
        }
      }
    }
  }

}

int PropagationDistanceField::getDirectionNumber(int dx, int dy, int dz) const
{
  return (dx+1)*9 + (dy+1)*3 + dz+1;
}

Eigen::Vector3i PropagationDistanceField::getLocationDifference(int directionNumber) const
{
  return direction_number_to_direction_[ directionNumber ];
}

double PropagationDistanceField::getDistance(double x, double y, double z) const
{
  return getDistance(voxel_grid_(x,y,z));
}

double PropagationDistanceField::getDistanceFromCell(int x, int y, int z) const
{
  return getDistance(voxel_grid_.getCell(x,y,z));
}

bool PropagationDistanceField::isCellValid(int x, int y, int z) const
{
  return voxel_grid_.isCellValid(x,y,z);
}

int PropagationDistanceField::getXNumCells() const
{
  return voxel_grid_.getNumCells(DIM_X);
}

int PropagationDistanceField::getYNumCells() const
{
  return voxel_grid_.getNumCells(DIM_Y);
}

int PropagationDistanceField::getZNumCells() const
{
  return voxel_grid_.getNumCells(DIM_Z);
}

bool PropagationDistanceField::gridToWorld(int x, int y, int z, double& world_x, double& world_y, double& world_z) const
{
  return voxel_grid_.gridToWorld(x, y, z, world_x, world_y, world_z);
}

bool PropagationDistanceField::worldToGrid(double world_x, double world_y, double world_z, int& x, int& y, int& z) const
{
  return voxel_grid_.worldToGrid(world_x, world_y, world_z, x, y, z);
}

}
