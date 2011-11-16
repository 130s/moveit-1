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

#ifndef PLANNING_MODELS_KINEMATIC_MODEL_
#define PLANNING_MODELS_KINEMATIC_MODEL_

#include <urdf/model.h>
#include <srdf/model.h>
#include <LinearMath/btTransform.h>
#include <geometric_shapes/shapes.h>
#include <random_numbers/random_numbers.h>

#include <boost/shared_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>


/** \brief Main namespace */
namespace planning_models
{

    /** \brief Definition of a kinematic model. This class is not thread
        safe, however multiple instances can be created */
    class KinematicModel
    {
    public:
        /** \brief Forward definition of a joint */
        class JointModel;

        /** \brief Forward definition of a link */
        class LinkModel;

        /** \brief A joint from the robot. Models the transform that
            this joint applies in the kinematic chain. A joint
            consists of multiple variables. In the simplest case, when
            the joint is a single DOF, there is only one variable and
            its name is the same as the joint's name. For multi-DOF
            joints, each variable has a local name (e.g., \e x, \e y)
            but the full variable name as seen from the outside of
            this class is a concatenation of the "joint name"."local
            name" (e.g., a joint named 'base' with local variables 'x'
            and 'y' will store its full variable names as 'base.x' and
            'base.y'). Local names are never used to reference
            variables directly. */
        class JointModel
        {
            friend class KinematicModel;
        public:

            JointModel(const std::string& name);
            virtual ~JointModel(void);

            /** \brief Get the name of the joint */
            const std::string& getName(void) const
            {
                return name_;
            }

            /** \brief The index of this joint when traversing the kinematic tree in depth first fashion */
            int getTreeIndex(void) const
            {
                return tree_index_;
            }

            /** \brief Get the link that this joint connects to. The
                robot is assumed to start with a joint, so the root
                joint will return a NULL pointer here. */
            const LinkModel* getParentLinkModel(void) const
            {
                return parent_link_model_;
            }

            /** \brief Get the link that this joint connects to. There will always be such a link */
            const LinkModel* getChildLinkModel(void) const
            {
                return child_link_model_;
            }

            /** \brief Gets the lower and upper bounds for a variable. Return false if variable was not found */
            bool getVariableBounds(const std::string& variable, std::pair<double, double>& bounds) const;

            /** \brief Provides a default value for the joint given the joint bounds.
                Most joints will use the default, but the quaternion for floating
                point values needs something else. The map is NOT cleared; elements are only added  (or overwritten). */
            void getDefaultValues(std::map<std::string, double> &values) const;

            /** \brief Provides a random values for the joint given the joint bounds. The map is NOT cleared; elements are only added (or overwritten). */
            void getRandomValues(random_numbers::RNG &rng, std::map<std::string, double> &values) const;

            /** \brief Provides a default value for the joint given the joint bounds.
                Most joints will use the default, but the quaternion for floating
                point values needs something else. The vector is NOT cleared; elements are only added with push_back */
            virtual void getDefaultValues(std::vector<double> &values) const;

            /** \brief Provides a random values for the joint given the joint bounds. The vector is NOT cleared; elements are only added with push_back */
            virtual void getRandomValues(random_numbers::RNG &rng, std::vector<double> &values) const;

            /** \brief Check if a particular variable satisfies the specified bounds */
            virtual bool isVariableWithinBounds(const std::string& variable, double value) const;

            /** \brief Get the names of the variables that make up this joint, in the order they appear in corresponding states.
                For single DOF joints, this will be just the joint name. */
            const std::vector<std::string>& getVariableNames(void) const
            {
                return variable_names_;
            }

            /** \brief Get the names of the variable suffixes that are attached to joint names to construct the variable names. For single DOF joints, this will be empty. */
            const std::vector<std::string>& getLocalVariableNames(void) const
            {
                return local_names_;
            }

            /** \brief Check if a particular variable is known to this joint */
            bool hasVariable(const std::string &variable) const
            {
                return variable_index_.find(variable) != variable_index_.end();
            }

            /** \brief Get the number of variables that describe this joint */
            unsigned int getVariableCount(void) const
            {
                return variable_names_.size();
            }

            const std::map<std::string, unsigned int>& getVariableIndexMap(void) const
            {
                return variable_index_;
            }

            /** \brief Given the joint values for a joint, compute the corresponding transform */
            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const = 0;

            /** \brief Given the transform generated by joint, compute the corresponding joint values */
            virtual void computeJointStateValues(const btTransform& transform, std::vector<double> &joint_values) const = 0;

            /** \brief Update a transform so that it corresponds to
                the new joint values assuming that \e transf was
                previously set to identity and that only calls to updateTransform() were issued afterwards */
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const = 0;

        protected:

            /** \brief Name of the joint */
            std::string                                       name_;

            /** \brief The local names to use for the variables that make up this joint */
            std::vector<std::string>                          local_names_;

            /** \brief The full names to use for the variables that make up this joint */
            std::vector<std::string>                          variable_names_;

            /** \brief The bounds for each variable (low, high) in the same order as variable_names_ */
            std::vector<std::pair<double, double> >           variable_bounds_;

            /** \brief Map from variable names to the corresponding index in variable_names_ */
            std::map<std::string, unsigned int>               variable_index_;

            /** \brief The link before this joint */
            LinkModel                                        *parent_link_model_;

            /** \brief The link after this joint */
            LinkModel                                        *child_link_model_;

            /** \brief The joint this one mimics (NULL for joints that do not mimic) */
            JointModel                                       *mimic_;

            /** \brief The offset to the mimic joint */
            double                                            mimic_factor_;

            /** \brief The multiplier to the mimic joint */
            double                                            mimic_offset_;

            /** \brief The set of joints that should get a value copied to them when this joint changes */
            std::vector<JointModel*>                          mimic_requests_;

            /** \brief The index assigned to this joint when traversing the kinematic tree in depth first fashion */
            int                                               tree_index_;
        };

        /** \brief A fixed joint */
        class FixedJointModel : public JointModel
        {
            friend class KinematicModel;
        public:

            FixedJointModel(const std::string &name) : JointModel(name)
            {
            }

            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void computeJointStateValues(const btTransform& trans, std::vector<double>& joint_values) const;
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const;
        };

        /** \brief A planar joint */
        class PlanarJointModel : public JointModel
        {
            friend class KinematicModel;
        public:

            PlanarJointModel(const std::string& name);

            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void computeJointStateValues(const btTransform& transf, std::vector<double>& joint_values) const;
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const;
        };

        /** \brief A floating joint */
        class FloatingJointModel : public JointModel
        {
            friend class KinematicModel;
        public:

            FloatingJointModel(const std::string& name);

            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void computeJointStateValues(const btTransform& transf, std::vector<double>& joint_values) const;
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void getRandomValues(random_numbers::RNG &rng, std::vector<double> &values) const;
            virtual void getDefaultValues(std::vector<double>& values) const;
        };

        /** \brief A prismatic joint */
        class PrismaticJointModel : public JointModel
        {
            friend class KinematicModel;
        public:

            PrismaticJointModel(const std::string& name);

            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void computeJointStateValues(const btTransform& transf, std::vector<double> &joint_values) const;
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const;

            const btVector3& getAxis(void) const
            {
                return axis_;
            }

        protected:
            /** \brief The axis of the joint */
            btVector3 axis_;
        };

        /** \brief A revolute joint */
        class RevoluteJointModel : public JointModel
        {
            friend class KinematicModel;
        public:

            RevoluteJointModel(const std::string& name);

            virtual void computeTransform(const std::vector<double>& joint_values, btTransform &transf) const;
            virtual void computeJointStateValues(const btTransform& transf, std::vector<double> &joint_values) const;
            virtual void updateTransform(const std::vector<double>& joint_values, btTransform &transf) const;

            /** \brief Check if this joint wraps around */
            bool isContinuous(void) const
            {
                return continuous_;
            }

            const btVector3& getAxis(void) const
            {
                return axis_;
            }

        protected:
            /** \brief The axis of the joint */
            btVector3 axis_;

            /** \brief Flag indicating whether this joint wraps around */
            bool      continuous_;
        };

        /** \brief A link from the robot. Contains the constant transform applied to the link and its geometry */
        class LinkModel
        {
            friend class KinematicModel;
        public:

            LinkModel(void);
            ~LinkModel(void);

            /** \brief The name of this link */
            const std::string& getName(void) const
            {
                return name_;
            }

            /** \brief The index of this joint when traversing the kinematic tree in depth first fashion */
            int getTreeIndex(void) const
            {
                return tree_index_;
            }

            const JointModel* getParentJointModel(void) const
            {
                return parent_joint_model_;
            }

            const std::vector<JointModel*>& getChildJointModels(void) const
            {
                return child_joint_models_;
            }

            const btTransform& getJointOriginTransform(void) const
            {
                return joint_origin_transform_;
            }

            const btTransform& getCollisionOriginTransform(void) const
            {
                return collision_origin_transform_;
            }

            const boost::shared_ptr<shapes::Shape>& getShape(void) const
            {
                return shape_;
            }

        private:

            /** \brief Name of the link */
            std::string                      name_;

            /** \brief JointModel that connects this link to the parent link */
            JointModel                      *parent_joint_model_;

            /** \brief List of descending joints (each connects to a child link) */
            std::vector<JointModel*>         child_joint_models_;

            /** \brief The constant transform applied to the link (local) */
            btTransform                      joint_origin_transform_;

            /** \brief The constant transform applied to the collision geometry of the link (local) */
            btTransform                      collision_origin_transform_;

            /** \brief The collision geometry of the link */
            boost::shared_ptr<shapes::Shape> shape_;

            /** \brief The index assigned to this link when traversing the kinematic tree in depth first fashion */
            int                              tree_index_;
        };

        class JointModelGroup
        {
            friend class KinematicModel;
        public:

            JointModelGroup(const std::string& name, const std::vector<const JointModel*>& joint_vector, const KinematicModel *parent_model);

            virtual ~JointModelGroup(void);

            const KinematicModel* getParentModel(void) const
            {
                return parent_model_;
            }

            const std::string& getName(void) const
            {
                return name_;
            }

            /** \brief Check if a joint is part of this group */
            bool hasJointModel(const std::string &joint) const;

            /** \brief Check if a link is part of this group */
            bool hasLinkModel(const std::string &link) const;

            /** \brief Get a joint by its name */
            const JointModel* getJointModel(const std::string &joint) const;

            const std::vector<const JointModel*>& getJointModels(void) const
            {
                return joint_model_vector_;
            }

            const std::vector<const JointModel*>& getFixedJointModels(void) const
            {
                return fixed_joints_;
            }

            const std::vector<std::string>& getJointModelNames(void) const
            {
                return joint_model_name_vector_;
            }

            const std::vector<const JointModel*>& getJointRoots(void) const
            {
                return joint_roots_;
            }

            const std::vector<const LinkModel*>& getLinkModels(void) const
            {
                return group_link_model_vector_;
            }

            const std::vector<std::string>& getLinkModelNames(void) const
            {
                return link_model_name_vector_;
            }

            const std::vector<const LinkModel*>& getUpdatedLinkModels(void) const
            {
                return updated_link_model_vector_;
            }

            const std::vector<std::string>& getUpdatedLinkModelNames(void) const
            {
                return updated_link_model_name_vector_;
            }

            const std::map<std::string, unsigned int>& getJointVariablesIndexMap(void) const
            {
                return joint_variables_index_map_;
            }

            void getRandomValues(random_numbers::RNG &rng, std::vector<double> &values) const;

            /** \brief Get the number of variables that describe this joint group */
            unsigned int getVariableCount(void) const
            {
                return variable_count_;
            }

        protected:

             /** \brief Owner model */
            const KinematicModel                    *parent_model_;

            /** \brief Name of group */
            std::string                              name_;

            /** \brief Names of joints in the order they appear in the group state */
            std::vector<std::string>                 joint_model_name_vector_;

            /** \brief Joint instances in the order they appear in the group state */
            std::vector<const JointModel*>           joint_model_vector_;

            /** \brief A map from joint names to their instances */
            std::map<std::string, const JointModel*> joint_model_map_;

            /** \brief The list of joint models that are roots in this group */
             std::vector<const JointModel*>           joint_roots_;

            /** \brief The group includes all the joint variables that make up the joints the group consists of.
                This map gives the position in the state vector of the group for each of these variables.
                Additionaly, it includes the names of the joints and the index for the first variable of that joint. */
            std::map<std::string, unsigned int>      joint_variables_index_map_;

            /** \brief The joints that have no DOF (fixed) */
            std::vector<const JointModel*>           fixed_joints_;

            /** \brief The links that are on the direct lineage between joints
                and joint_roots_, as well as the children of the joint leafs.
                May not be in any particular order */
            std::vector<const LinkModel*>            group_link_model_vector_;

            /** \brief The names of the links in this group */
            std::vector<std::string>                 link_model_name_vector_;

            /** \brief The list of downstream link models in the order they should be updated (may include links that are not in this group) */
            std::vector<const LinkModel*>            updated_link_model_vector_;

            /** \brief The list of downstream link names in the order they should be updated (may include links that are not in this group) */
            std::vector<std::string>                 updated_link_model_name_vector_;

            /** \brief The number of variables necessary to describe this group of joints */
            unsigned int                             variable_count_;
        };

        /** \brief Construct a kinematic model from a parsed description and a list of planning groups */
        KinematicModel(const urdf::Model &model, const srdf::Model &smodel);

        /** \brief Destructor. Clear all memory. */
        virtual ~KinematicModel(void);

        /** \brief General the model name **/
        const std::string& getName(void) const;

        /** \brief Get a link by its name */
        const LinkModel* getLinkModel(const std::string &link) const;

        /** \brief Check if a link exists */
        bool hasLinkModel(const std::string &name) const;

        /** \brief Get a joint by its name */
        const JointModel* getJointModel(const std::string &joint) const;

        /** \brief Check if a joint exists */
        bool hasJointModel(const std::string &name) const;

        /** \brief Get the set of link models that follow a parent link in the kinematic chain */
        void getChildLinkModels(const LinkModel* parent, std::vector<const LinkModel*> &links) const;

        /** \brief Get the set of link models that follow a parent joint in the kinematic chain */
        void getChildLinkModels(const JointModel* parent, std::vector<const LinkModel*> &links) const;

        /** \brief Get the set of joint models that follow a parent link in the kinematic chain */
        void getChildJointModels(const LinkModel* parent, std::vector<const JointModel*> &links) const;

        /** \brief Get the set of joint models that follow a parent joint in the kinematic chain */
        void getChildJointModels(const JointModel* parent, std::vector<const JointModel*> &links) const;

        /** \brief Get the set of link names that follow a parent link in the kinematic chain */
        std::vector<std::string> getChildLinkModelNames(const LinkModel* parent) const;

        /** \brief Get the set of joint names that follow a parent link in the kinematic chain */
        std::vector<std::string> getChildJointModelNames(const LinkModel* parent) const;

        /** \brief Get the set of joint names that follow a parent joint in the kinematic chain */
        std::vector<std::string> getChildJointModelNames(const JointModel* parent) const;

        /** \brief Get the set of link names that follow a parent link in the kinematic chain */
        std::vector<std::string> getChildLinkModelNames(const JointModel* parent) const;

        /** \brief Get the array of joints, in the order they appear
            in the robot state. */
        const std::vector<JointModel*>& getJointModels(void) const
        {
            return joint_model_vector_;
        }

        /** \brief Get the array of joint names, in the order they appear in the robot state. */
        const std::vector<std::string>& getJointModelNames(void) const
        {
            return joint_model_names_vector_;
        }

        /** \brief Get the array of joints, in the order they should be
            updated*/
        const std::vector<LinkModel*>& getLinkModels(void) const
        {
            return link_model_vector_;
        }

        const std::vector<LinkModel*>& getLinkModelsWithCollisionGeometry(void) const
        {
            return link_models_with_collision_geometry_vector_;
        }

        const std::vector<std::string>& getLinkModelNamesWithCollisionGeometry(void) const
        {
            return link_model_names_with_collision_geometry_vector_;
        }

        /** \brief Get the link names */
        const std::vector<std::string>& getLinkModelNames(void) const
        {
            return link_model_names_vector_;
        }

        /** \brief Get the root joint */
        const JointModel* getRoot(void) const;

        const std::string& getModelFrame(void) const
        {
            return model_frame_;
        }

        void getRandomValues(random_numbers::RNG &rng, std::vector<double> &values) const;

        /** \brief Print information about the constructed model */
        void printModelInfo(std::ostream &out = std::cout) const;

        bool hasJointModelGroup(const std::string& group) const;

        bool addJointModelGroup(const srdf::Model::Group& group);

        void removeJointModelGroup(const std::string& group);

        const JointModelGroup* getJointModelGroup(const std::string& name) const;

        const std::map<std::string, JointModelGroup*>& getJointModelGroupMap(void) const
        {
            return joint_model_group_map_;
        }

        const std::map<std::string, srdf::Model::Group>& getJointModelGroupConfigMap(void) const
        {
            return joint_model_group_config_map_;
        }

        const std::vector<std::string>& getJointModelGroupNames(void) const;

        /** \brief Get the number of variables that describe this model */
        unsigned int getVariableCount(void) const
        {
            return variable_count_;
        }

        const std::map<std::string, unsigned int>& getJointVariablesIndexMap(void) const
        {
            return joint_variables_index_map_;
        }

    protected:

        /** \brief The name of the model */
        std::string                             model_name_;

        std::string                             model_frame_;

        /** \brief A map from link names to their instances */
        std::map<std::string, LinkModel*>       link_model_map_;

        /** \brief The vector of links that are updated when computeTransforms() is called, in the order they are updated */
        std::vector<LinkModel*>                 link_model_vector_;

        /** \brief The vector of link names that corresponds to link_model_vector_ */
        std::vector<std::string>                link_model_names_vector_;

        /** \brief Only links that have collision geometry specified */
        std::vector<LinkModel*>                 link_models_with_collision_geometry_vector_;

        /** \brief The vector of link names that corresponds to link_models_with_collision_geometry_vector_ */
        std::vector<std::string>                link_model_names_with_collision_geometry_vector_;

        /** \brief A map from joint names to their instances */
        std::map<std::string, JointModel*>      joint_model_map_;

        /** \brief The vector of joints in the model, in the order they appear in the state vector */
        std::vector<JointModel*>                joint_model_vector_;

        /** \brief The vector of joint names that corresponds to joint_model_vector_ */
        std::vector<std::string>                joint_model_names_vector_;

        /** \brief Get the number of variables necessary to describe this model */
        unsigned int                            variable_count_;

        /** \brief The state includes all the joint variables that make up the joints the state consists of.
            This map gives the position in the state vector of the group for each of these variables.
            Additionaly, it includes the names of the joints and the index for the first variable of that joint. */
        std::map<std::string, unsigned int>     joint_variables_index_map_;

        /** \brief The root joint */
        JointModel                             *root_;

        std::map<std::string, JointModelGroup*>   joint_model_group_map_;
        std::map<std::string, srdf::Model::Group> joint_model_group_config_map_;
        std::vector<std::string>                  joint_model_group_names_;
        std::vector<srdf::Model::GroupState>      default_states_;

        void buildGroups(const std::vector<srdf::Model::Group> &group_config);

        JointModel* buildRecursive(LinkModel *parent, const urdf::Link *link, const std::vector<srdf::Model::VirtualJoint> &vjoints);
        JointModel* constructJointModel(const urdf::Joint *urdfJointModel, const urdf::Link *child_link, const std::vector<srdf::Model::VirtualJoint> &vjoints);
        LinkModel* constructLinkModel(const urdf::Link *urdfLink);
        boost::shared_ptr<shapes::Shape> constructShape(const urdf::Geometry *geom);
    };

    typedef boost::shared_ptr<KinematicModel> KinematicModelPtr;
}

#endif
