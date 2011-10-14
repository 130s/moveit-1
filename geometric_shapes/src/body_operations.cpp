/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
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

/** \author Ioan Sucan, E. Gil Jones */

#include <geometric_shapes/body_operations.h>
#include <ros/console.h>

bodies::Body* bodies::createBodyFromShape(const shapes::Shape *shape)
{
  Body *body = NULL;
    
  if (shape)
    switch (shape->type)
    {
    case shapes::BOX:
      body = new bodies::Box(shape);
      break;
    case shapes::SPHERE:
      body = new bodies::Sphere(shape);
      break;
    case shapes::CYLINDER:
      body = new bodies::Cylinder(shape);
      break;
    case shapes::MESH:
      body = new bodies::ConvexMesh(shape);
      break;
    default:
      ROS_ERROR("Creating body from shape: Unknown shape type %d", (int)shape->type);
      break;
    }
    
  return body;
}

void bodies::maskPosesInsideBodyVectors(const std::vector<btTransform>& poses,
                                        const std::vector<bodies::BodyVector*>& bvs,
                                        std::vector<bool>& mask) 
{
    mask.resize(poses.size(), false);
    for (unsigned int i = 0; i < poses.size(); i++)
    {
	bool inside = false;
	const btVector3& pt = poses[i].getOrigin();
	for (unsigned int j = 0; !inside && j < bvs.size(); j++)
	{
	    for (unsigned int k = 0 ; !inside && k < bvs[j]->getSize(); ++k)
		inside = bvs[j]->containsPoint(pt);
	}
	mask[i] = !inside;
    }
}
