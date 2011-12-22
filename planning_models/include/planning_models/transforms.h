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

/* Author: Ioan Sucan */

#ifndef PLANNING_MODELS_TRANSFORMS_
#define PLANNING_MODELS_TRANSFORMS_

#include <LinearMath/btTransform.h>
#include <planning_models/kinematic_state.h>
#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Pose.h>
#include <string>
#include <map>

namespace planning_models
{
    bool quatFromMsg(const geometry_msgs::Quaternion &qmsg, btQuaternion &q);
    bool poseFromMsg(const geometry_msgs::Pose &tmsg, btTransform &t);
    void msgFromPose(const btTransform &t, geometry_msgs::Pose &tmsg);

/**
 * @class Transforms
 * @brief Provides an implementation of a snapshot of a transform tree that can be easily queried for 
 *        transforming different quantities
 */
    class Transforms
    {
    public:
      /**
       * @brief Construct a transform tree 
       */
        Transforms(const std::string &target_frame);

      /**
       * @brief Copy constructor
       */
        Transforms(const Transforms &other);

      /**
       * @brief Destructor
       */
        ~Transforms(void);

      /**
       * @brief Get the planning frame corresponding to this set of transforms
       * @return The planning frame
       */
        const std::string& getTargetFrame(void) const;

      /**
       * @brief Check whether a particular frame is a fixed frame
       * @return True if the frame is fixed, false otherwise
       */
        bool isFixedFrame(const std::string &frame) const;

      /**
       * @brief Return all the transforms
       * @return A map from string names of frames to corresponding btTransforms (w.r.t the planning frame)
       */
        const std::map<std::string, btTransform>& getAllTransforms(void) const;

      /**
       * @brief Get transform for from_frame (w.r.t target frame)
       * @param from_frame The string id of the frame for which the transform is being computed
       * @return The required transform
       */
        const btTransform& getTransform(const std::string &from_frame) const;

      /**
       * @brief Transform a vector in from_frame to the target_frame
       * @param from_frame The frame from which the transform is computed
       * @param v_in The input vector (in from_frame)
       * @param v_out The resultant (transformed) vector
       */
        void transformVector3(const std::string &from_frame, const btVector3 &v_in, btVector3 &v_out) const;

      /**
       * @brief Transform a quaternion in from_frame to the target_frame
       * @param from_frame The frame in which the input quaternion is specified
       * @param v_in The input quaternion (in from_frame)
       * @param v_out The resultant (transformed) quaternion
       */
        void transformQuaternion(const std::string &from_frame, const btQuaternion &q_in, btQuaternion &q_out) const;

      /**
       * @brief Transform a rotation matrix in from_frame to the target_frame
       * @param from_frame The frame in which the input rotation matrix is specified
       * @param m_in The input rotation matrix (in from_frame)
       * @param m_out The resultant (transformed) rotation matrix
       */
      void transformRotationMatrix(const std::string &from_frame, const btMatrix3x3 &m_in, btMatrix3x3 &m_out) const;

      /**
       * @brief Transform a pose in from_frame to the target_frame
       * @param from_frame The frame in which the input rotation matrix is specified
       * @param t_in The input pose (in from_frame)
       * @param t_out The resultant (transformed) pose
       */
      void transformPose(const std::string &from_frame, const btTransform &t_in, btTransform &t_out) const;

      /**
       * @brief Get transform for from_frame (w.r.t target frame) given a kinematic state
       * @param kinematic_state The input kinematic state
       * @param from_frame The string id of the frame for which the transform is being computed
       */
        const btTransform& getTransform(const planning_models::KinematicState &kinematic_state, const std::string &from_frame) const;

      /**
       * @brief Transform a vector in from_frame to the target_frame
       * @param kinematic_state The input kinematic state
       * @param from_frame The frame from which the transform is computed
       * @param v_in The input vector (in from_frame)
       * @param v_out The resultant (transformed) vector
       */
      void transformVector3(const planning_models::KinematicState &kinematic_state, const std::string &from_frame, const btVector3 &v_in, btVector3 &v_out) const;

      /**
       * @brief Transform a quaternion in from_frame to the target_frame
       * @param kinematic_state The input kinematic state
       * @param from_frame The frame in which the input quaternion is specified
       * @param v_in The input quaternion (in from_frame)
       * @param v_out The resultant (transformed) quaternion
       */
        void transformQuaternion(const planning_models::KinematicState &kinematic_state, const std::string &from_frame, const btQuaternion &q_in, btQuaternion &q_out) const;

      /**
       * @brief Transform a rotation matrix in from_frame to the target_frame
       * @param kinematic_state The input kinematic state
       * @param from_frame The frame in which the input rotation matrix is specified
       * @param m_in The input rotation matrix (in from_frame)
       * @param m_out The resultant (transformed) rotation matrix
       */
        void transformRotationMatrix(const planning_models::KinematicState &kinematic_state, const std::string &from_frame, const btMatrix3x3 &m_in, btMatrix3x3 &m_out) const;

      /**
       * @brief Transform a pose in from_frame to the target_frame
       * @param kinematic_state The input kinematic state
       * @param from_frame The frame in which the input rotation matrix is specified
       * @param t_in The input pose (in from_frame)
       * @param t_out The resultant (transformed) pose
       */
      void transformPose(const planning_models::KinematicState &kinematic_state, const std::string &from_frame, const btTransform &t_in, btTransform &t_out) const;

      /**
       * @brief Set a transform in the transform tree (adding it if necessary)
       * @param t The input transform (w.r.t the target frame)
       * @param from_frame The frame for which the input transform is specified
       */
        void setTransform(const btTransform &t, const std::string &from_frame);

      /**
       * @brief Set a transform in the transform tree (adding it if necessary)
       * @param transform The input transform (the frame_id must match the target frame)
       */
        void setTransform(const geometry_msgs::TransformStamped &transform);

      /**
       * @brief Set a transform in the transform tree (adding it if necessary)
       * @param transform The input transforms (the frame_id must match the target frame)
       */
        void setTransforms(const std::vector<geometry_msgs::TransformStamped> &transforms);

      /**
       * @brief Get a vector of all the transforms as ROS messages
       * @param transforms The output transforms
       */
        void getTransforms(std::vector<geometry_msgs::TransformStamped> &transforms) const;

    private:

        std::string                        target_frame_;
        std::map<std::string, btTransform> transforms_;
    };

    typedef boost::shared_ptr<Transforms> TransformsPtr;
    typedef boost::shared_ptr<const Transforms> TransformsConstPtr;
}

#endif
