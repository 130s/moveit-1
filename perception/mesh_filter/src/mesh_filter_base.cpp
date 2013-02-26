/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2013, Willow Garage, Inc.
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

/* Author: Suat Gedikli */

#include <moveit/mesh_filter/mesh_filter_base.h>
#include <moveit/mesh_filter/gl_mesh.h>

#include <geometric_shapes/shapes.h>
#include <geometric_shapes/shape_operations.h>
#include <Eigen/Eigen>
#include <stdexcept>
#include <sensor_msgs/image_encodings.h>

// include SSE headers
#ifdef HAVE_SSE_EXTENSIONS
#include <xmmintrin.h>
#endif

using namespace std;
using namespace Eigen;
using shapes::Mesh;

mesh_filter::MeshFilterBase::MeshFilterBase (const TransformCallback& transform_callback,
              const SensorModel::Parameters& sensor_parameters,
              const string& render_vertex_shader, const string& render_fragment_shader,
              const string& filter_vertex_shader, const string& filter_fragment_shader)
: sensor_parameters_ (sensor_parameters.clone ())
, mesh_renderer_ (sensor_parameters.getWidth(), sensor_parameters.getHeight(),
                  sensor_parameters.getNearClippingPlaneDistance (), 
                  sensor_parameters.getFarClippingPlaneDistance ()) // some default values for buffer sizes and clipping planes
, depth_filter_ (sensor_parameters.getWidth(), sensor_parameters.getHeight(),
                  sensor_parameters.getNearClippingPlaneDistance (), 
                  sensor_parameters.getFarClippingPlaneDistance ())
, next_handle_ (FirstLabel) // 0 and 1 are reserved!
, transform_callback_ (transform_callback)
, shadow_threshold_ (0.5)
, padding_scale_ (1.0)
, padding_offset_ (0.01)
{
  mesh_renderer_.setShadersFromString (render_vertex_shader, render_fragment_shader);
  depth_filter_.setShadersFromString (filter_vertex_shader, filter_fragment_shader);

  depth_filter_.begin ();

  glGenTextures (1, &sensor_depth_texture_);

  glUniform1i (glGetUniformLocation (depth_filter_.getProgramID (), "sensor"), 0);
  glUniform1i (glGetUniformLocation (depth_filter_.getProgramID (), "depth"), 2);
  glUniform1i (glGetUniformLocation (depth_filter_.getProgramID (), "label"), 4);

  shadow_threshold_location_ = glGetUniformLocation (depth_filter_.getProgramID (), "shadow_threshold");

  depth_filter_.end ();

  canvas_ = glGenLists (1);
  glNewList (canvas_, GL_COMPILE);
  glBegin (GL_QUADS);

  glColor3f (1, 1, 1);
  glTexCoord2f (0, 0);
  glVertex3f (-1, -1, 0);

  glTexCoord2f (1, 0);
  glVertex3f (1, -1, 0);

  glTexCoord2f ( 1, 1);
  glVertex3f (1, 1, 0);

  glTexCoord2f ( 0, 1);
  glVertex3f (-1, 1, 0);

  glEnd ();
  glEndList ();
}

mesh_filter::MeshFilterBase::~MeshFilterBase ()
{
  glDeleteLists (1, canvas_);
  glDeleteTextures (1, &sensor_depth_texture_);

  for (map<unsigned int, GLMesh*>::iterator meshIt = meshes_.begin (); meshIt != meshes_.end (); ++meshIt)
    delete (meshIt->second);
  meshes_.clear ();
}

void mesh_filter::MeshFilterBase::setSize (unsigned width, unsigned height)
{
  mesh_renderer_.setBufferSize (width, height);
  mesh_renderer_.setCameraParameters (width, width, width >> 1, height >> 1);

  depth_filter_.setBufferSize (width, height);
  depth_filter_.setCameraParameters (width, width, width >> 1, height >> 1);
}

void mesh_filter::MeshFilterBase::setTransformCallback (const boost::function<bool(mesh_filter::MeshHandle, Affine3d&)>& transform_callback)
{
  transform_callback_ = transform_callback;
}

mesh_filter::MeshHandle mesh_filter::MeshFilterBase::addMesh (const Mesh& mesh)
{
  Mesh collapsedMesh (1, 1);
  mergeVertices (mesh, collapsedMesh);
  meshes_[next_handle_] = new GLMesh (collapsedMesh, next_handle_);
  return next_handle_++;
}

void mesh_filter::MeshFilterBase::removeMesh (MeshHandle handle)
{
  long unsigned int erased = meshes_.erase (handle);
  if (erased == 0)
    throw runtime_error ("Could not remove mesh. Mesh not found!");
}

void mesh_filter::MeshFilterBase::setShadowThreshold (float threshold)
{
  shadow_threshold_ = threshold;
}

void mesh_filter::MeshFilterBase::getModelLabels (unsigned* labels) const
{
  mesh_renderer_.getColorBuffer ((unsigned char*) labels);
}

void mesh_filter::MeshFilterBase::getModelDepth (float* depth) const
{
  mesh_renderer_.getDepthBuffer (depth);
  sensor_parameters_->transformModelDepthToMetricDepth (depth);
}

void mesh_filter::MeshFilterBase::getFilteredDepth (float* depth) const
{
  depth_filter_.getDepthBuffer (depth);
  sensor_parameters_->transformFilteredDepthToMetricDepth (depth);
}

void mesh_filter::MeshFilterBase::getFilteredLabels (unsigned* labels) const
{
  depth_filter_.getColorBuffer ((unsigned char*) labels);
}

void mesh_filter::MeshFilterBase::filter (const float* sensor_data) const
{
  mesh_renderer_.begin ();
  sensor_parameters_->setRenderParameters (mesh_renderer_);
  
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_CULL_FACE);
  glCullFace (GL_FRONT);
  glDisable (GL_ALPHA_TEST);

  GLuint padding_coefficients_id = glGetUniformLocation (mesh_renderer_.getProgramID (), "padding_coefficients");
  Eigen::Vector3f padding_coefficients = sensor_parameters_->getPaddingCoefficients () * padding_scale_ + Eigen::Vector3f (0, 0, padding_offset_);  
  glUniform3f (padding_coefficients_id, padding_coefficients [0], padding_coefficients [1], padding_coefficients [2]);

  Affine3d transform;
  for (map<unsigned int, GLMesh*>::const_iterator meshIt = meshes_.begin (); meshIt != meshes_.end (); ++meshIt)
    if (transform_callback_ (meshIt->first, transform))
      meshIt->second->render (transform);

  mesh_renderer_.end ();

  // now filter the depth_map with the second rendering stage
  //depth_filter_.setBufferSize (width, height);
  //depth_filter_.setCameraParameters (fx, fy, cx, cy);
  depth_filter_.begin ();
  sensor_parameters_->setRenderParameters (depth_filter_);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_CULL_FACE);
  glCullFace (GL_BACK);
  glDisable (GL_ALPHA_TEST);

//  glUniform1f (near_location_, depth_filter_.getNearClippingDistance ());
//  glUniform1f (far_location_, depth_filter_.getFarClippingDistance ());
  glUniform1f (shadow_threshold_location_, shadow_threshold_);

  GLuint depth_texture = mesh_renderer_.getDepthTexture ();
  GLuint color_texture = mesh_renderer_.getColorTexture ();

  // bind sensor depth
  glActiveTexture (GL_TEXTURE0);
  glBindTexture ( GL_TEXTURE_2D, sensor_depth_texture_ );

  float scale = 1.0 / (sensor_parameters_->getFarClippingPlaneDistance () - sensor_parameters_->getNearClippingPlaneDistance ());
  glPixelTransferf (GL_DEPTH_SCALE, scale);
  glPixelTransferf (GL_DEPTH_BIAS, -scale * sensor_parameters_->getNearClippingPlaneDistance ());
  glTexImage2D ( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, sensor_parameters_->getWidth (), sensor_parameters_->getHeight (), 0, GL_DEPTH_COMPONENT, GL_FLOAT, sensor_data);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // bind depth map
  glActiveTexture (GL_TEXTURE2);
  glBindTexture (GL_TEXTURE_2D, depth_texture);

  // bind labels
  glActiveTexture (GL_TEXTURE4);
  glBindTexture (GL_TEXTURE_2D, color_texture);

  glCallList (canvas_);
  glDisable ( GL_TEXTURE_2D );
  depth_filter_.end ();
}

void mesh_filter::MeshFilterBase::setPaddingOffset (float offset)
{
  padding_offset_ = offset;
}

void mesh_filter::MeshFilterBase::setPaddingScale (float scale)
{
  padding_scale_ = scale;
}

void mesh_filter::MeshFilterBase::mergeVertices (const Mesh& mesh, Mesh& compressed)
{
  // max allowed distance of vertices to be considered same! 1mm
  // to allow double-error from transformed vertices.
  static const double thresholdSQR = pow (0.00001, 2);

  vector<unsigned> vertex_map (mesh.vertex_count);
  vector<Vector3d> vertices (mesh.vertex_count);
  vector<Vector3d> compressed_vertices;
  vector<Vector3i> triangles (mesh.triangle_count);

  for (unsigned vIdx = 0; vIdx < mesh.vertex_count; ++vIdx)
  {
    vertices [vIdx][0] = mesh.vertices [3 * vIdx];
    vertices [vIdx][1] = mesh.vertices [3 * vIdx + 1];
    vertices [vIdx][2] = mesh.vertices [3 * vIdx + 2];
    vertex_map [vIdx] = vIdx;
  }

  for (unsigned tIdx = 0; tIdx < mesh.triangle_count; ++tIdx)
  {
    triangles [tIdx][0] = mesh.triangles [3 * tIdx];
    triangles [tIdx][1] = mesh.triangles [3 * tIdx + 1];
    triangles [tIdx][2] = mesh.triangles [3 * tIdx + 2];
  }

  for (unsigned vIdx1 = 0; vIdx1 < mesh.vertex_count; ++vIdx1)
  {
    if (vertex_map [vIdx1] != vIdx1)
      continue;

    vertex_map [vIdx1] = compressed_vertices.size ();
    compressed_vertices.push_back (vertices[vIdx1]);

    for (unsigned vIdx2 = vIdx1 + 1; vIdx2 < mesh.vertex_count; ++vIdx2)
    {
      double distanceSQR = (vertices[vIdx1] - vertices [vIdx2]).squaredNorm ();
      if (distanceSQR <= thresholdSQR)
        vertex_map [vIdx2] = vertex_map [vIdx1];
    }
  }

  // redirect triangles to new vertices!
  for (unsigned tIdx = 0; tIdx < mesh.triangle_count; ++tIdx)
  {
    triangles [tIdx][0] = vertex_map [triangles [tIdx][0]];
    triangles [tIdx][1] = vertex_map [triangles [tIdx][1]];
    triangles [tIdx][2] = vertex_map [triangles [tIdx][2]];
  }

  for (int vIdx = 0; vIdx < vertices.size (); ++vIdx)
  {
    if (vertices[vIdx][0] == vertices[vIdx][1] || vertices[vIdx][0] == vertices[vIdx][2] || vertices[vIdx][1] == vertices[vIdx][2])
    {
      vertices[vIdx] = vertices.back ();
      vertices.pop_back ();
      --vIdx;
    }
  }

  // create new Mesh structure
  if (compressed.vertex_count > 0 && compressed.vertices)
    delete [] compressed.vertices;
  if (compressed.triangle_count > 0 && compressed.triangles)
    delete [] compressed.triangles;
  if (compressed.triangle_count > 0 && compressed.normals)
    delete [] compressed.normals;

  compressed.vertex_count = compressed_vertices.size ();
  compressed.vertices = new double [compressed.vertex_count * 3];
  for (unsigned vIdx = 0; vIdx < compressed.vertex_count; ++vIdx)
  {
    compressed.vertices [3 * vIdx + 0] = compressed_vertices [vIdx][0];
    compressed.vertices [3 * vIdx + 1] = compressed_vertices [vIdx][1];
    compressed.vertices [3 * vIdx + 2] = compressed_vertices [vIdx][2];
  }

  compressed.triangle_count = triangles.size ();
  compressed.triangles = new unsigned int [compressed.triangle_count * 3];
  for (unsigned tIdx = 0; tIdx < compressed.triangle_count; ++tIdx)
  {
    compressed.triangles [3 * tIdx + 0] = triangles [tIdx][0];
    compressed.triangles [3 * tIdx + 1] = triangles [tIdx][1];
    compressed.triangles [3 * tIdx + 2] = triangles [tIdx][2];
  }

  compressed.normals = new double [compressed.triangle_count * 3];
  for (unsigned tIdx = 0; tIdx < compressed.triangle_count; ++tIdx)
  {
    Vector3d d1 = compressed_vertices [triangles[tIdx][1]] - compressed_vertices [triangles[tIdx][0]];
    Vector3d d2 = compressed_vertices [triangles[tIdx][2]] - compressed_vertices [triangles[tIdx][0]];
    Vector3d normal = d1.cross (d2);
    normal.normalize ();
    Vector3d normal_;
    normal_ [0] = mesh.normals [3 * tIdx];
    normal_ [1] = mesh.normals [3 * tIdx + 1];
    normal_ [2] = mesh.normals [3 * tIdx + 2];
    double cosAngle = normal.dot (normal_);

    if (cosAngle < 0)
    {
      swap (compressed.triangles [3 * tIdx + 1], compressed.triangles [3 * tIdx + 2]);
      normal *= -1;
    }

    compressed.normals [tIdx * 3 + 0] = normal [0];
    compressed.normals [tIdx * 3 + 1] = normal [1];
    compressed.normals [tIdx * 3 + 2] = normal [2];
  }
}
