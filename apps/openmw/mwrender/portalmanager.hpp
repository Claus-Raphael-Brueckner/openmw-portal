#ifndef OPENMW_MWRENDER_PORTALMANAGER_H
#define OPENMW_MWRENDER_PORTALMANAGER_H

#include <vector>

#include <osg/Quat>
#include <osg/Vec2f>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Group;
    class MatrixTransform;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    class PortalRTTNode;

    /// Manages portal surfaces that replace teleport door meshes.
    /// Stage 1: ex_cave_door_01 replaced by colored quad; walk-through triggers teleport.
    /// Stage 2: RTT camera renders destination cell statics into the quad.
    class PortalManager
    {
    public:
        PortalManager(Resource::ResourceSystem* resourceSystem, osg::Group* rttParent);
        ~PortalManager();

        /// Called from Objects::insertModel for door objects.
        /// Returns true if a portal was created (caller should skip loading the door mesh).
        bool tryCreatePortal(const MWWorld::Ptr& door);

        /// Called from Objects::removeObject. Removes portal data for the door if it exists.
        void destroyPortal(const MWWorld::Ptr& door);

        /// Called every frame. Checks whether the player has crossed any portal plane and
        /// triggers teleportation. Only runs when not paused.
        void update(const osg::Vec3f& playerPos, const osg::Matrixd& viewMatrix, const osg::Matrixd& projMatrix, bool paused);

        /// Provide the main terrain root so exterior portals can share it in their RTT scene.
        void setExteriorTerrainNode(osg::Group* terrain) { mExteriorTerrainNode = terrain; }

    private:
        struct Portal
        {
            MWWorld::Ptr door;
            osg::ref_ptr<osg::MatrixTransform> quadNode; ///< child of door's base node
            osg::Vec3f planePoint;   ///< world-space point on the portal plane
            osg::Vec3f planeNormal;  ///< world-space normal (faces "outside")
            osg::Quat  invRot;       ///< inverse of door rotation, for local-space projection
            osg::Vec2f halfExtents;  ///< half-width (X) and half-height (Z) of the opening
            bool lastSide = true;    ///< which side of the plane the player was on last frame
            int cooldown = 0;        ///< frames to wait; prevents immediate re-trigger
            osg::Vec3f destPoint;    ///< world-space arrival position in the destination cell
            osg::Quat  destRot;      ///< orientation of the arrival point (forward = into dest)
            osg::Vec3f destDoorPos;  ///< world-space center of the actual destination door
            osg::Quat  destDoorRot;  ///< full rotation of the destination door (CellRef * NIF root)
            osg::ref_ptr<PortalRTTNode> rttNode;    ///< RTT camera node, child of mRttParent
            osg::ref_ptr<osg::Group>   portalScene; ///< scene group rendered by the RTT camera
        };

        bool isPortalDoor(const MWWorld::Ptr& door) const;
        osg::Vec2f computeHalfExtents(const MWWorld::Ptr& door) const;
        osg::ref_ptr<osg::MatrixTransform> buildQuadNode(const osg::Vec2f& halfExtents, const osg::Quat& nifRootQuat) const;
        bool isWithinBounds(const osg::Vec3f& playerPos, const Portal& portal) const;

        std::vector<Portal> mPortals;
        Resource::ResourceSystem* mResourceSystem;
        osg::Group* mRttParent;           ///< RTT nodes are added here (should outlive PortalManager)
        osg::Group* mExteriorTerrainNode = nullptr; ///< shared terrain root for exterior portals
        int mDebugFrame = 0;
    };

}

#endif
