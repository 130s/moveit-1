/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
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

/* Author: Jon Binney */

#include <occupancy_map_monitor/point_cloud_occupancy_map_updater.h>
#include <tf/tf.h>
#include <tf/message_filter.h>
#include <message_filters/subscriber.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/ros/conversions.h>

namespace occupancy_map_monitor
{


PointCloudOccupancyMapUpdater::PointCloudOccupancyMapUpdater(const boost::shared_ptr<tf::Transformer> &tf, const std::string &map_frame)
  : tf_(tf), map_frame_(map_frame)
{
}

PointCloudOccupancyMapUpdater::~PointCloudOccupancyMapUpdater(void)
{
  delete point_cloud_subscriber_;
  delete point_cloud_filter_;
}

bool PointCloudOccupancyMapUpdater::setParams(XmlRpc::XmlRpcValue &params)
{
  if(!params.hasMember("point_cloud_topic"))
    return false;
  std::string point_cloud_topic = std::string (params["point_cloud_topic"]);

  if(!params.hasMember("max_range"))
    return false;
  double max_range = double (params["max_range"]);

  if(!params.hasMember("frame_subsample"))
    return false;
  size_t frame_subsample = int (params["frame_subsample"]);

  if(!params.hasMember("point_subsample"))
    return false;
  size_t point_subsample = int (params["point_subsample"]);

  return this->setParams(point_cloud_topic, max_range, frame_subsample, point_subsample);
}

bool PointCloudOccupancyMapUpdater::setParams(const std::string &point_cloud_topic, double max_range,  size_t frame_subsample,  size_t point_subsample)
{
  point_cloud_topic_ = point_cloud_topic;
  max_range_ = max_range;
  frame_subsample_ = frame_subsample;
  point_subsample_ = point_subsample;
  return true;
}

void PointCloudOccupancyMapUpdater::initialize()
{
  // subscribe to point cloud topic using tf filter
  point_cloud_subscriber_ = new message_filters::Subscriber<sensor_msgs::PointCloud2>(root_nh_, point_cloud_topic_, 1024);
  point_cloud_filter_ = new tf::MessageFilter<sensor_msgs::PointCloud2>(*point_cloud_subscriber_, *tf_, map_frame_, 1024);
  point_cloud_filter_->registerCallback(boost::bind(&PointCloudOccupancyMapUpdater::cloudMsgCallback, this, _1));
  
  ROS_INFO("Listening to '%s' using message filter with target frame '%s'", point_cloud_topic_.c_str(), point_cloud_filter_->getTargetFramesString().c_str());
}

void PointCloudOccupancyMapUpdater::cloudMsgCallback(const sensor_msgs::PointCloud2::ConstPtr &cloud_msg)
{
  ROS_DEBUG("Got a point cloud message");
  {
    boost::lock_guard<boost::mutex> _lock(last_point_cloud_mutex_);
    last_point_cloud_ = cloud_msg;
  }
  /* tell the monitor that we are ready to update the map */
  notifyUpdateReady();
}

void PointCloudOccupancyMapUpdater::process(const OccMapTreePtr &tree)
{
  ROS_DEBUG("Updating occupancy map with new cloud");
  sensor_msgs::PointCloud2::ConstPtr cloud;
  {
    boost::lock_guard<boost::mutex> _lock(last_point_cloud_mutex_);
    cloud = last_point_cloud_;
    last_point_cloud_.reset();
  }
  
  if (cloud)
  {
    processCloud(tree, cloud);
    ROS_DEBUG("Done updating occupancy map");
  }
  else
    ROS_DEBUG("No point cloud to process");
}

void PointCloudOccupancyMapUpdater::processCloud(const OccMapTreePtr &tree, const sensor_msgs::PointCloud2::ConstPtr &cloud_msg)
{
  /* get transform for cloud into map frame */
  tf::StampedTransform map_H_sensor;
  try
  {
    tf_->lookupTransform(map_frame_, cloud_msg->header.frame_id, cloud_msg->header.stamp, map_H_sensor);
  }
  catch (tf::TransformException& ex)
  {
    ROS_ERROR_STREAM("Transform error of sensor data: " << ex.what() << ", quitting callback");
    return;
  }
  
  /* convert cloud message to pcl cloud object */
  pcl::PointCloud<pcl::PointXYZ> cloud;
  pcl::fromROSMsg(*cloud_msg, cloud);
  
  /* compute sensor origin in map frame */
  tf::Vector3 sensor_origin_tf = map_H_sensor.getOrigin();
  octomap::point3d sensor_origin(sensor_origin_tf.getX(), sensor_origin_tf.getY(), sensor_origin_tf.getZ());
  
  ROS_DEBUG("Looping through points to find free and occupied areas");
  
  /* do ray tracing to find which cells this point cloud indicates should be free, and which it indicates
   * should be occupied */
  octomap::KeySet free_cells, occupied_cells;
  unsigned int row, col;
  for(row = 0; row < cloud.height; row += point_subsample_)
  {
    for(col = 0; col < cloud.width; col += point_subsample_)
    {
      pcl::PointXYZ p = cloud(col, row);
      
      /* check for NaN */
      if(!((p.x == p.x) && (p.y == p.y) && (p.z == p.z)))
        continue;
      
      /* transform to map frame */
      tf::Vector3 point_tf = map_H_sensor * tf::Vector3(p.x, p.y, p.z);
      octomap::point3d point(point_tf.getX(), point_tf.getY(), point_tf.getZ());
      
      /* free cells along ray */
      if (tree->computeRayKeys(sensor_origin, point, key_ray_))
        free_cells.insert(key_ray_.begin(), key_ray_.end());
      
      /* occupied cell at ray endpoint if ray is shorter than max range */
      double range = (point - sensor_origin).norm();
      if(range < max_range_)
      {
        octomap::OcTreeKey key;
        if (tree->genKey(point, key))
          occupied_cells.insert(key);
      }
    }
  }
  
  ROS_DEBUG("Marking free cells in octomap");
  
  /* mark free cells only if not seen occupied in this cloud */
  for (octomap::KeySet::iterator it = free_cells.begin(), end = free_cells.end(); it != end; ++it)
    /* this check seems unnecessary since we would just overwrite them in the next loop? -jbinney */
    if (occupied_cells.find(*it) == occupied_cells.end())
      tree->updateNode(*it, false);
  
  ROS_DEBUG("Marking occupied cells in octomap");
  
  /* now mark all occupied cells */
  for (octomap::KeySet::iterator it = occupied_cells.begin(), end = free_cells.end(); it != end; it++)
    tree->updateNode(*it, true);
}

}
