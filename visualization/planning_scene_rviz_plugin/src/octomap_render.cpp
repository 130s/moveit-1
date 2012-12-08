#include "moveit/planning_scene_rviz_plugin/octomap_render.h"

#include <octomap_msgs/Octomap.h>
#include <octomap_msgs/GetOctomap.h>
#include <octomap_msgs/BoundingBoxQuery.h>

//#include <octomap_ros/OctomapROS.h>
#include <octomap/octomap.h>
#include <octomap_ros/conversions.h>
#include <octomap/OcTreeKey.h>

#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSceneManager.h>

#include "rviz/ogre_helpers/point_cloud.h"

namespace moveit_rviz_plugin
{

typedef std::vector<rviz::PointCloud::Point> VPoint;
typedef std::vector<VPoint> VVPoint;

static Ogre::String movable_type ("OcTree");

OcTreeRender::OcTreeRender(const shapes::Shape *shape, Ogre::SceneManager* scene_manager, Ogre::SceneNode* parent_node) :
    Ogre::MovableObject(), shape_(shape), colorFactor_(0.8)
{
  std::size_t i;

  if (!parent_node)
  {
    parent_node = scene_manager_->getRootSceneNode();
  }

  scene_node_ = parent_node->createChildSceneNode();

  cloud_.resize(16);

  for (i = 0; i < 16; ++i)
  {
    std::stringstream sname;
    sname << "PointCloud Nr." << i;
    cloud_[i] = new rviz::PointCloud();
    cloud_[i]->setName(sname.str());
    cloud_[i]->setRenderMode(rviz::PointCloud::RM_BOXES);
    scene_node_->attachObject(cloud_[i]);
  }

  boost::shared_ptr<const octomap::OcTree> octree = static_cast<const shapes::OcTree*>(shape)->octree;

  octreeDecoding(octree);
}

OcTreeRender::~OcTreeRender()
{
  std::size_t i;

  scene_node_->detachAllObjects();

  for (i = 0; i < 16; ++i)
  {
    delete cloud_[i];
  }

}

// method taken from octomap_server package
void OcTreeRender::setColor( double z_pos, double min_z, double max_z, double color_factor, rviz::PointCloud::Point* point)
{
  int i;
  double m, n, f;

  double s = 1.0;
  double v = 1.0;

  double h = (1.0 - std::min(std::max((z_pos-min_z)/ (max_z - min_z), 0.0), 1.0)) *color_factor;

  h -= floor(h);
  h *= 6;
  i = floor(h);
  f = h - i;
  if (!(i & 1))
    f = 1 - f; // if i is even
  m = v * (1 - s);
  n = v * (1 - s * f);

  switch (i) {
    case 6:
    case 0:
      point->setColor(v, n, m);
      break;
    case 1:
      point->setColor(n, v, m);
      break;
    case 2:
      point->setColor(m, v, n);
      break;
    case 3:
      point->setColor(m, n, v);
      break;
    case 4:
      point->setColor(n, m, v);
      break;
    case 5:
      point->setColor(v, m, n);
      break;
    default:
      point->setColor(1, 0.5, 0.5);
      break;
  }
}

void OcTreeRender::_notifyCurrentCamera(Ogre::Camera* camera)
{
  MovableObject::_notifyCurrentCamera( camera );
}

void OcTreeRender::_updateRenderQueue(Ogre::RenderQueue* queue)
{
  std::vector<rviz::PointCloud*>::iterator it = cloud_.begin();
  std::vector<rviz::PointCloud*>::iterator end = cloud_.end();
  for (; it != end; ++it)
  {
    (*it)->_updateRenderQueue(queue);
  }
}

void OcTreeRender::_notifyAttached(Ogre::Node *parent, bool isTagPoint)
{
  MovableObject::_notifyAttached(parent, isTagPoint);
}

#if (OGRE_VERSION_MAJOR >= 1 && OGRE_VERSION_MINOR >= 6)
void OcTreeRender::visitRenderables(Ogre::Renderable::Visitor* visitor, bool debugRenderables)
{
}
#endif

void OcTreeRender::getWorldTransforms(Ogre::Matrix4* xform) const
{
   *xform = scene_node_->_getFullTransform();
}

const Ogre::AxisAlignedBox& OcTreeRender::getBoundingBox() const
{
  return bb_;
}

float OcTreeRender::getBoundingRadius() const
{
  Ogre::AxisAlignedBox bb = getBoundingBox();
  Ogre::Vector3 bb_hs = bb.getHalfSize();

  return bb_hs.length();
}

const Ogre::String& OcTreeRender::getMovableType() const
{
  return movable_type;
}

void OcTreeRender::octreeDecoding (boost::shared_ptr<const octomap::OcTree> octree )
{
  VVPoint pointBuf_;

  // get dimensions of octree
  double minX, minY, minZ, maxX, maxY, maxZ;
  octree->getMetricMin(minX, minY, minZ);
  octree->getMetricMax(maxX, maxY, maxZ);

  size_t pointCount = 0;
  {
    // traverse all leafs in the tree:
    unsigned int treeDepth = octree->getTreeDepth();
    for (octomap::OcTree::iterator it = octree->begin(treeDepth), end = octree->end(); it != end; ++it)
    {

      if (octree->isNodeOccupied(*it))
      {

        // check if current voxel has neighbors on all sides -> no need to be displayed
        bool allNeighborsFound = true;

        octomap::OcTreeKey key;
        octomap::OcTreeKey nKey = it.getKey();

        for (key[2] = nKey[2] - 1; allNeighborsFound && key[2] <= nKey[2] + 1; ++key[2])
        {
          for (key[1] = nKey[1] - 1; allNeighborsFound && key[1] <= nKey[1] + 1; ++key[1])
          {
            for (key[0] = nKey[0] - 1; allNeighborsFound && key[0] <= nKey[0] + 1; ++key[0])
            {
              if (key != nKey)
              {
                octomap::OcTreeNode* node = octree->search(key);
                if (!(node && octree->isNodeOccupied(node)))
                {
                  // we do not have a neighbor => break!
                  allNeighborsFound = false;
                }
              }
            }
          }

        }

        if (!allNeighborsFound)
        {
          rviz::PointCloud::Point newPoint;

          newPoint.position.x = it.getX();
          newPoint.position.y = it.getY();
          newPoint.position.z = it.getZ();

          // apply color
          setColor(newPoint.position.z, minZ, maxZ, colorFactor_, &newPoint);

          // push to point vectors
          unsigned int depth = it.getDepth();
          pointBuf_[depth-1].push_back(newPoint);

          ++pointCount;
        }
      }
    }
  }

  for (size_t i=0; i<16; ++i)
  {
    double size = octree->getNodeSize(i+1);

    cloud_[i]->clear();
    cloud_[i]->setDimensions( size, size, size );

    cloud_[i]->addPoints(&pointBuf_[i].front(), pointBuf_[i].size());
    pointBuf_[i].clear();

    bb_.merge(cloud_[i]->getBoundingBox());
  }

}


}
