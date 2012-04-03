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

/* Author: Ioan Sucan */

#include "geometric_shapes/shape_operations.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include <float.h>

#include <ros/console.h>
#include <resource_retriever/retriever.h>

#include <assimp/assimp.hpp>
#include <assimp/aiScene.h>
#include <assimp/aiPostProcess.h>

#include <Eigen/Geometry>

namespace shapes
{

namespace detail
{
struct myVertex
{
  Eigen::Vector3d    point;
  unsigned int index;
};

struct ltVertexValue
{
  bool operator()(const myVertex &p1, const myVertex &p2) const
  {
    const Eigen::Vector3d &v1 = p1.point;
    const Eigen::Vector3d &v2 = p2.point;
    if (v1.x() < v2.x())
      return true;
    if (v1.x() > v2.x())
      return false;
    if (v1.y() < v2.y())
      return true;
    if (v1.y() > v2.y())
      return false;
    if (v1.z() < v2.z())
      return true;
    return false;
  }
};

struct ltVertexIndex
{
  bool operator()(const myVertex &p1, const myVertex &p2) const
  {
    return p1.index < p2.index;
  }
};

}

Mesh* createMeshFromVertices(const std::vector<Eigen::Vector3d> &vertices, const std::vector<unsigned int> &triangles)
{
  unsigned int nt = triangles.size() / 3;
  Mesh *mesh = new Mesh(vertices.size(), nt);
  for (unsigned int i = 0 ; i < vertices.size() ; ++i)
  {
    mesh->vertices[3 * i    ] = vertices[i].x();
    mesh->vertices[3 * i + 1] = vertices[i].y();
    mesh->vertices[3 * i + 2] = vertices[i].z();
  }

  std::copy(triangles.begin(), triangles.end(), mesh->triangles);

  // compute normals
  for (unsigned int i = 0 ; i < nt ; ++i)
  {
    Eigen::Vector3d s1 = vertices[triangles[i * 3    ]] - vertices[triangles[i * 3 + 1]];
    Eigen::Vector3d s2 = vertices[triangles[i * 3 + 1]] - vertices[triangles[i * 3 + 2]];
    Eigen::Vector3d normal = s1.cross(s2);
    normal.normalize();
    mesh->normals[3 * i    ] = normal.x();
    mesh->normals[3 * i + 1] = normal.y();
    mesh->normals[3 * i + 2] = normal.z();
  }
  return mesh;
}

Mesh* createMeshFromVertices(const std::vector<Eigen::Vector3d> &source)
{
  if (source.size() < 3)
    return NULL;

  std::set<detail::myVertex, detail::ltVertexValue> vertices;
  std::vector<unsigned int>                         triangles;

  for (unsigned int i = 0 ; i < source.size() / 3 ; ++i)
  {
    // check if we have new vertices
    detail::myVertex vt;

    vt.point = source[3 * i];
    std::set<detail::myVertex, detail::ltVertexValue>::iterator p1 = vertices.find(vt);
    if (p1 == vertices.end())
    {
      vt.index = vertices.size();
      vertices.insert(vt);
    }
    else
      vt.index = p1->index;
    triangles.push_back(vt.index);

    vt.point = source[3 * i + 1];
    std::set<detail::myVertex, detail::ltVertexValue>::iterator p2 = vertices.find(vt);
    if (p2 == vertices.end())
    {
      vt.index = vertices.size();
      vertices.insert(vt);
    }
    else
      vt.index = p2->index;
    triangles.push_back(vt.index);

    vt.point = source[3 * i + 2];
    std::set<detail::myVertex, detail::ltVertexValue>::iterator p3 = vertices.find(vt);
    if (p3 == vertices.end())
    {
      vt.index = vertices.size();
      vertices.insert(vt);
    }
    else
      vt.index = p3->index;

    triangles.push_back(vt.index);
  }

  // sort our vertices
  std::vector<detail::myVertex> vt;
  vt.insert(vt.begin(), vertices.begin(), vertices.end());
  std::sort(vt.begin(), vt.end(), detail::ltVertexIndex());

  // copy the data to a mesh structure
  unsigned int nt = triangles.size() / 3;

  Mesh *mesh = new Mesh(vt.size(), nt);
  for (unsigned int i = 0 ; i < vt.size() ; ++i)
  {
    mesh->vertices[3 * i    ] = vt[i].point.x();
    mesh->vertices[3 * i + 1] = vt[i].point.y();
    mesh->vertices[3 * i + 2] = vt[i].point.z();
  }

  std::copy(triangles.begin(), triangles.end(), mesh->triangles);

  // compute normals
  for (unsigned int i = 0 ; i < nt ; ++i)
  {
    Eigen::Vector3d s1 = vt[triangles[i * 3    ]].point - vt[triangles[i * 3 + 1]].point;
    Eigen::Vector3d s2 = vt[triangles[i * 3 + 1]].point - vt[triangles[i * 3 + 2]].point;
    Eigen::Vector3d normal = s1.cross(s2);
    normal.normalize();
    mesh->normals[3 * i    ] = normal.x();
    mesh->normals[3 * i + 1] = normal.y();
    mesh->normals[3 * i + 2] = normal.z();
  }

  return mesh;
}

Mesh* createMeshFromFilename(const std::string& filename, const Eigen::Vector3d &scale)
{
  resource_retriever::Retriever retriever;
  resource_retriever::MemoryResource res;
  try {
    res = retriever.get(filename);
  } catch (resource_retriever::Exception& e) {
    ROS_ERROR("%s", e.what());
    return NULL;
  }

  if (res.size == 0) {
    ROS_WARN("Retrieved empty mesh for resource '%s'", filename.c_str());
    return NULL;
  }


  // Create an instance of the Importer class
  Assimp::Importer importer;

  // try to get a file extension
  std::string hint;
  std::size_t pos = filename.find_last_of(".");
  if (pos != std::string::npos)
  {
    hint = filename.substr(pos + 1);

    // temp hack until everything is stl not stlb
    if (hint.find("stl") != std::string::npos)
      hint = "stl";
  }

  // And have it read the given file with some postprocessing
  const aiScene* scene = importer.ReadFileFromMemory(reinterpret_cast<void*>(res.data.get()), res.size,
                                                     aiProcess_Triangulate            |
                                                     aiProcess_JoinIdenticalVertices  |
                                                     aiProcess_SortByPType, hint.c_str());
  if(!scene) {
    ROS_WARN_STREAM("Assimp reports no scene in " << filename);
    return NULL;
  }
  if(!scene->HasMeshes()) {
    ROS_WARN_STREAM("Assimp reports scene in " << filename << " has no meshes");
    return NULL;
  }
  if(scene->mNumMeshes > 1) {
    ROS_WARN_STREAM("Mesh loaded from " << filename << " has " << scene->mNumMeshes << " but only the first one will be used");
  }

  aiNode *node = scene->mRootNode;

  bool found = false;
  if(node->mNumMeshes > 0 && node->mMeshes != NULL)
  {
    ROS_DEBUG_STREAM("Root node has " << node->mMeshes << " meshes in " << filename);
    found = true;
  }
  else
  {
    for (uint32_t i = 0; i < node->mNumChildren; ++i)
    {
      if(node->mChildren[i]->mNumMeshes > 0 && node->mChildren[i]->mMeshes != NULL)
      {
        ROS_DEBUG_STREAM("Child " << i << " has meshes in " << filename);
        node = node->mChildren[i];
        found = true;
        break;
      }
    }
  }
  if(found == false) {
    ROS_WARN_STREAM("Can't find meshes in " << filename);
    return NULL;
  }
  return createMeshFromAsset(scene->mMeshes[node->mMeshes[0]], node->mTransformation, scale);
}

Mesh* createMeshFromAsset(const aiMesh* a, const aiMatrix4x4& transform, const Eigen::Vector3d& scale)
{
  if (!a->HasFaces())
  {
    ROS_ERROR("Mesh asset has no faces");
    return NULL;
  }

  if (!a->HasPositions())
  {
    ROS_ERROR("Mesh asset has no positions");
    return NULL;
  }

  for (unsigned int i = 0 ; i < a->mNumFaces ; ++i)
    if (a->mFaces[i].mNumIndices != 3)
    {
      ROS_ERROR("Asset is not a triangle mesh");
      return NULL;
    }

  Mesh *mesh = new Mesh(a->mNumVertices, a->mNumFaces);

  // copy vertices
  for (unsigned int i = 0 ; i < a->mNumVertices ; ++i)
  {
    aiVector3D p;
    p.x = a->mVertices[i].x;
    p.y = a->mVertices[i].y;
    p.z = a->mVertices[i].z;
    p *= transform;
    mesh->vertices[3 * i    ] = p.x*scale.x();
    mesh->vertices[3 * i + 1] = p.y*scale.y();
    mesh->vertices[3 * i + 2] = p.z*scale.z();
  }

  // copy triangles
  for (unsigned int i = 0 ; i < a->mNumFaces ; ++i)
  {
    mesh->triangles[3 * i    ] = a->mFaces[i].mIndices[0];
    mesh->triangles[3 * i + 1] = a->mFaces[i].mIndices[1];
    mesh->triangles[3 * i + 2] = a->mFaces[i].mIndices[2];
  }

  // compute normals
  for (unsigned int i = 0 ; i < a->mNumFaces ; ++i)
  {
    aiVector3D f1 = a->mVertices[a->mFaces[i].mIndices[0]];
    f1.x *= scale.x();
    f1.y *= scale.y();
    f1.z *= scale.z();
    aiVector3D f2 = a->mVertices[a->mFaces[i].mIndices[1]];
    f2.x *= scale.x();
    f2.y *= scale.y();
    f2.z *= scale.z();
    aiVector3D f3 = a->mVertices[a->mFaces[i].mIndices[2]];
    f3.x *= scale.x();
    f3.y *= scale.y();
    f3.z *= scale.z();
    aiVector3D as1 = f1-f2;
    aiVector3D as2 = f2-f3;
    Eigen::Vector3d s1(as1.x, as1.y, as1.z);
    Eigen::Vector3d s2(as2.x, as2.y, as2.z);
    Eigen::Vector3d normal = s1.cross(s2);
    normal.normalize();
    mesh->normals[3 * i    ] = normal.x();
    mesh->normals[3 * i + 1] = normal.y();
    mesh->normals[3 * i + 2] = normal.z();
  }

  return mesh;
}

Shape* constructShapeFromMsg(const shape_msgs::Shape &shape_msg)
{
  Shape *shape = NULL;
  if (shape_msg.type == shape_msgs::Shape::SPHERE)
  {
    if (shape_msg.dimensions.size() != 1)
      ROS_ERROR("Unexpected number of dimensions in sphere definition");
    else
      shape = new Sphere(shape_msg.dimensions[0]);
  }
  else
    if (shape_msg.type == shape_msgs::Shape::BOX)
    {
      if (shape_msg.dimensions.size() != 3)
        ROS_ERROR("Unexpected number of dimensions in box definition");
      else
        shape = new Box(shape_msg.dimensions[0], shape_msg.dimensions[1], shape_msg.dimensions[2]);
    }
    else
      if (shape_msg.type == shape_msgs::Shape::CYLINDER)
      {
        if (shape_msg.dimensions.size() != 2)
          ROS_ERROR("Unexpected number of dimensions in cylinder definition");
        else
          shape = new Cylinder(shape_msg.dimensions[0], shape_msg.dimensions[1]);
      }
      else
        if (shape_msg.type == shape_msgs::Shape::MESH)
        {
          if (shape_msg.dimensions.size() != 0)
            ROS_ERROR("Unexpected number of dimensions in mesh definition");
          else
          {
            if (shape_msg.triangles.size() % 3 != 0)
              ROS_ERROR("Number of triangle indices is not divisible by 3");
            else
            {
              if (shape_msg.triangles.empty() || shape_msg.vertices.empty())
                ROS_ERROR("Mesh definition is empty");
              else
              {
                std::vector<Eigen::Vector3d>    vertices(shape_msg.vertices.size());
                std::vector<unsigned int> triangles(shape_msg.triangles.size());
                for (unsigned int i = 0 ; i < shape_msg.vertices.size() ; ++i)
                  vertices[i] = Eigen::Vector3d(shape_msg.vertices[i].x, shape_msg.vertices[i].y, shape_msg.vertices[i].z);
                for (unsigned int i = 0 ; i < shape_msg.triangles.size() ; ++i)
                  triangles[i] = shape_msg.triangles[i];
                shape = createMeshFromVertices(vertices, triangles);
              }
            }
          }
        }

  if (shape == NULL)
    ROS_ERROR("Unable to construct shape corresponding to shape_msgect of type %d", (int)shape_msg.type);

  return shape;
}

bool constructMarkerFromShape(const Shape* shape, visualization_msgs::Marker &mk, bool use_mesh_triangle_list)
{
  shape_msgs::Shape shape_msg;
  if (constructMsgFromShape(shape, shape_msg))
    return constructMarkerFromShape(shape_msg, mk, use_mesh_triangle_list);
  else
    return false;
}

bool constructMarkerFromShape(const shape_msgs::Shape &shape_msg, visualization_msgs::Marker &mk, bool use_mesh_triangle_list)
{
  switch (shape_msg.type)
  {
  case shape_msgs::Shape::SPHERE:
    mk.type = visualization_msgs::Marker::SPHERE;
    if (shape_msg.dimensions.size() != 1)
    {
      ROS_ERROR("Unexpected number of dimensions in sphere definition");
      return false;
    }
    else
      mk.scale.x = mk.scale.y = mk.scale.z = shape_msg.dimensions[0] * 2.0;
    break;
  case shape_msgs::Shape::BOX:
    mk.type = visualization_msgs::Marker::CUBE;
    if (shape_msg.dimensions.size() != 3)
    {
      ROS_ERROR("Unexpected number of dimensions in box definition");
      return false;
    }
    else
    {
      mk.scale.x = shape_msg.dimensions[0];
      mk.scale.y = shape_msg.dimensions[1];
      mk.scale.z = shape_msg.dimensions[2];
    }
    break;
  case shape_msgs::Shape::CYLINDER:
    mk.type = visualization_msgs::Marker::CYLINDER;
    if (shape_msg.dimensions.size() != 2)
    {
      ROS_ERROR("Unexpected number of dimensions in cylinder definition");
      return false;
    }
    else
    {
      mk.scale.x = shape_msg.dimensions[0] * 2.0;
      mk.scale.y = shape_msg.dimensions[0] * 2.0;
      mk.scale.z = shape_msg.dimensions[1];
    }
    break;

  case shape_msgs::Shape::MESH:
    if (shape_msg.dimensions.size() != 0) {
      ROS_ERROR("Unexpected number of dimensions in mesh definition");
      return false;
    } 
    if (shape_msg.triangles.size() % 3 != 0) {
      ROS_ERROR("Number of triangle indices is not divisible by 3");
      return false;
    }
    if (shape_msg.triangles.empty() || shape_msg.vertices.empty()) {
      ROS_ERROR("Mesh definition is empty");
      return false;
    }
    if(!use_mesh_triangle_list) {
      mk.type = visualization_msgs::Marker::LINE_LIST;
      mk.scale.x = mk.scale.y = mk.scale.z = 0.01;
      std::size_t nt = shape_msg.triangles.size() / 3;
      for (std::size_t i = 0 ; i < nt ; ++i)
      {
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i+1]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i+2]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i+1]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[3*i+2]]);
      }
    } else {
      mk.type = visualization_msgs::Marker::TRIANGLE_LIST;
      mk.scale.x = mk.scale.y = mk.scale.z = 1.0;
      for (std::size_t i = 0 ; i < shape_msg.triangles.size(); i+=3)
      {
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[i]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[i+1]]);
        mk.points.push_back(shape_msg.vertices[shape_msg.triangles[i+2]]);
      }
    }
    return true;
  default:
    ROS_ERROR("Unknown shape type: %d", (int)shape_msg.type);
    return false;
  }
  return true;
}

bool constructMsgFromShape(const Shape* shape, shape_msgs::Shape &shape_msg)
{
  shape_msg.dimensions.clear();
  shape_msg.vertices.clear();
  shape_msg.triangles.clear();
  if (shape->type == SPHERE)
  {
    shape_msg.type = shape_msgs::Shape::SPHERE;
    shape_msg.dimensions.push_back(static_cast<const Sphere*>(shape)->radius);
  }
  else
    if (shape->type == BOX)
    {
      shape_msg.type = shape_msgs::Shape::BOX;
      const double* sz = static_cast<const Box*>(shape)->size;
      shape_msg.dimensions.push_back(sz[0]);
      shape_msg.dimensions.push_back(sz[1]);
      shape_msg.dimensions.push_back(sz[2]);
    }
    else
      if (shape->type == CYLINDER)
      {
        shape_msg.type = shape_msgs::Shape::CYLINDER;
        shape_msg.dimensions.push_back(static_cast<const Cylinder*>(shape)->radius);
        shape_msg.dimensions.push_back(static_cast<const Cylinder*>(shape)->length);
      }
      else
        if (shape->type == MESH)
        {
          shape_msg.type = shape_msgs::Shape::MESH;

          const Mesh *mesh = static_cast<const Mesh*>(shape);
          const unsigned int t3 = mesh->triangle_count * 3;

          shape_msg.vertices.resize(mesh->vertex_count);
          shape_msg.triangles.resize(t3);

          for (unsigned int i = 0 ; i < mesh->vertex_count ; ++i)
          {
            unsigned int i3 = i * 3;
            shape_msg.vertices[i].x = mesh->vertices[i3];
            shape_msg.vertices[i].y = mesh->vertices[i3 + 1];
            shape_msg.vertices[i].z = mesh->vertices[i3 + 2];
          }

          for (unsigned int i = 0 ; i < t3  ; ++i)
            shape_msg.triangles[i] = mesh->triangles[i];
        }
        else
        {
          ROS_ERROR("Unable to construct shape message for shape of type %d", (int)shape->type);
          return false;
        }

  return true;
}

StaticShape* constructShapeFromMsg(const shape_msgs::StaticShape &shape_msg)
{
  StaticShape *shape = NULL;
  if (shape_msg.type == shape_msgs::StaticShape::PLANE)
  {
    if (shape_msg.dimensions.size() == 4)
      shape = new Plane(shape_msg.dimensions[0], shape_msg.dimensions[1], shape_msg.dimensions[2], shape_msg.dimensions[3]);
    else
      ROS_ERROR("Unexpected number of dimensions in plane definition (got %d, expected 4)", (int)shape_msg.dimensions.size());
  }
  else
    ROS_ERROR("Unknown static shape type: %d", (int)shape_msg.type);
  return shape;
}

bool constructMsgFromShape(const StaticShape* shape, shape_msgs::StaticShape &shape_msg)
{
  shape_msg.dimensions.clear();
  if (shape->type == PLANE)
  {
    shape_msg.type = shape_msgs::StaticShape::PLANE;
    const Plane *p = static_cast<const Plane*>(shape);
    shape_msg.dimensions.push_back(p->a);
    shape_msg.dimensions.push_back(p->b);
    shape_msg.dimensions.push_back(p->c);
    shape_msg.dimensions.push_back(p->d);
  }
  else
  {
    ROS_ERROR("Unable to construct shape message for shape of type %d", (int)shape->type);
    return false;
  }
  return true;
}

bool getShapeExtents(const shape_msgs::Shape& shape_msg,
                     double& x_extent,
                     double& y_extent,
                     double& z_extent)
{
  if(shape_msg.type == shape_msgs::Shape::SPHERE) {
    if(shape_msg.dimensions.size() != 1) return false;
    x_extent = y_extent = z_extent = shape_msg.dimensions[0];
  } else if(shape_msg.type == shape_msgs::Shape::BOX) {
    if(shape_msg.dimensions.size() != 3) return false;
    x_extent = shape_msg.dimensions[0];
    y_extent = shape_msg.dimensions[1];
    z_extent = shape_msg.dimensions[2];
  } else if(shape_msg.type == shape_msgs::Shape::CYLINDER) {
    if(shape_msg.dimensions.size() != 2) return false;
    x_extent = shape_msg.dimensions[0];
    y_extent = shape_msg.dimensions[0];
    z_extent = shape_msg.dimensions[1];
  } else if(shape_msg.type == shape_msgs::Shape::MESH) {
    if(shape_msg.vertices.size() == 0) return false;
    double xmin = DBL_MAX, ymin = DBL_MAX, zmin = DBL_MAX;
    double xmax = -DBL_MAX, ymax = -DBL_MAX, zmax = -DBL_MAX;
    for(unsigned int i = 0; i < shape_msg.vertices.size(); i++) {
      if(shape_msg.vertices[i].x > xmax) {
        xmax = shape_msg.vertices[i].x;
      }
      if(shape_msg.vertices[i].x < xmin) {
        xmin = shape_msg.vertices[i].x;
      }
      if(shape_msg.vertices[i].y > ymax) {
        ymax = shape_msg.vertices[i].y;
      }
      if(shape_msg.vertices[i].y < ymin) {
        ymin = shape_msg.vertices[i].y;
      }
      if(shape_msg.vertices[i].z > zmax) {
        zmax = shape_msg.vertices[i].z;
      }
      if(shape_msg.vertices[i].z < zmin) {
        zmin = shape_msg.vertices[i].z;
      }
    }
    x_extent = xmax-xmin;
    y_extent = ymax-ymin;
    z_extent = zmax-zmin;
  } else {
    return false;
  }  
  return true;
}


}
