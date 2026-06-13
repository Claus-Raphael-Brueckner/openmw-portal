#ifndef OPENMW_MWRENDER_PORTALMANAGER_H
#define OPENMW_MWRENDER_PORTALMANAGER_H

#include <vector>

#include <osg/Quat>
#include <osg/Vec2f>
#include <osg/Vec3f>
#include <osg/Vec4f>
#include <osg/ref_ptr>

#include "../mwworld/ptr.hpp"
#include <components/esm/position.hpp>
#include <components/esm/refid.hpp>

namespace osg
{
    class Group;
    class Light;
    class LightModel;
    class MatrixTransform;
    class Texture2D;
}

namespace SceneUtil
{
    class RTTNode;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    class PortalRTTNode;
    class SkyManager;

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
        /// Returns true if a portal was actually found and removed.
        bool destroyPortal(const MWWorld::Ptr& door);

        /// Called every frame. Checks whether the player has crossed any portal plane and
        /// triggers teleportation. Only runs when not paused.
        void update(const osg::Vec3f& playerPos, const osg::Matrixd& viewMatrix, const osg::Matrixd& projMatrix, bool paused);

        /// Provide the main terrain root so exterior portals can share it in their RTT scene.
        void setExteriorTerrainNode(osg::Group* terrain) { mExteriorTerrainNode = terrain; }

        /// Update the sky clear color for exterior portal RTTs (call each frame with current sky color).
        void setExteriorSkyColor(const osg::Vec4f& color) { mExteriorSkyColor = color; }

        /// Update sun/ambient lighting for exterior portals (call each frame from RenderingManager).
        void setExteriorLighting(const osg::Vec4f& ambient, const osg::Vec4f& diffuse, const osg::Vec3f& sunDir);

        /// Update the near clip distance used for the water refraction depth linearization.
        void setNearClip(float v) { mNearClip = v; }

        /// Provide the SkyManager so exterior portals can fetch mSkyNode lazily (after sky is created).
        void setSkyManager(SkyManager* sky) { mSkyManager = sky; }

        /// Provide the main Water reflection RTT so portal water uses real sky reflections.
        void setReflectionRTT(SceneUtil::RTTNode* rtt) { mReflectionRTT = rtt; }

        /// Returns true if ptr is a door whose model is handled as a portal surface.
        bool isPortalDoor(const MWWorld::Ptr& door) const;

    private:
        struct Portal
        {
            MWWorld::Ptr door;
            osg::ref_ptr<osg::MatrixTransform> quadNode; ///< child of door's base node
            osg::Vec3f planePoint;   ///< world-space point on the portal plane (at NIF root, used for crossing)
            osg::Vec3f quadCenter;   ///< world-space center of the visual quad (planePoint + worldOffset)
            osg::Vec3f planeNormal;  ///< world-space normal (faces "outside")
            osg::Quat  invRot;       ///< inverse of door rotation, for local-space projection
            osg::Vec2f halfExtents;  ///< half-width (X) and half-height (Z) of the opening
            bool lastSide = true;    ///< which side of the plane the player was on last frame
            int cooldown = 0;        ///< frames to wait; prevents immediate re-trigger
            ESM::RefId   destCellId; ///< cached dest cell id (avoid re-dereferencing door Ptr later)
            ESM::Position destPos;  ///< cached door destination (position + rotation) for cell change
            osg::Vec3f destPoint;   ///< world-space arrival position in the destination cell
            osg::Quat  destRot;     ///< orientation of the arrival point (forward = into dest)
            osg::Vec3f destDoorPos;  ///< world-space center of the actual destination door
            osg::Quat  destDoorRot;  ///< full rotation of the destination door (CellRef * NIF root)
            osg::ref_ptr<PortalRTTNode>      rttNode;         ///< RTT camera node, child of mRttParent
            osg::ref_ptr<SceneUtil::RTTNode> reflectionRTTNode; ///< water reflection camera (exterior portals)
            osg::ref_ptr<SceneUtil::RTTNode> refractionRTTNode; ///< water refraction camera (exterior portals)
            osg::ref_ptr<osg::Group>         portalScene;     ///< scene group rendered by the RTT camera
            osg::ref_ptr<osg::Texture2D>     waterSkyTex;     ///< 1×1 reflection stand-in for sky color
            osg::ref_ptr<osg::Light>         sunLight;        ///< updated each frame with current sun
            osg::ref_ptr<osg::LightModel>    lightModelAttr;  ///< updated each frame with current ambient
            float waterHeight = 0.f;
            bool destIsExterior  = false;
            bool approachActive  = false; ///< ghost mode currently active for this portal
            bool noCollision     = false; ///< skip approach-zone ghost mode (e.g. Telvanni organic doors)
            bool needsFlatFloor  = false; ///< always add flat floor box at portal sill (e.g. ex_cave_door_01)
        };

        osg::Vec2f computeHalfExtents(const MWWorld::Ptr& door, osg::Vec3f& outCenter, osg::Quat& inOutNifRot) const;
        osg::ref_ptr<osg::MatrixTransform> buildQuadNode(const osg::Vec2f& halfExtents, const osg::Quat& nifRootQuat, const osg::Vec3f& localOffset = {}) const;
        bool isWithinBounds(const osg::Vec3f& playerPos, const Portal& portal) const;

        /// Build and attach the RTT scene for a portal that is now within streaming range.
        void setupPortalRTT(Portal& portal);
        /// Tear down the RTT scene for a portal that has left streaming range.
        void teardownPortalRTT(Portal& portal);

        /// Portals whose RTT is active when the player is closer than this distance.
        static constexpr float kStreamRange    = 1600.f; // ~25 m, always active
        static constexpr float kStreamRangeFar = 3200.f; // ~50 m, active when portal is in viewport

        std::vector<Portal> mPortals;
        bool mGhostModeActive = false; ///< true while any portal has approachActive
        Resource::ResourceSystem* mResourceSystem;
        osg::Group* mRttParent;           ///< RTT nodes are added here (should outlive PortalManager)
        osg::Group* mExteriorTerrainNode = nullptr; ///< shared terrain root for exterior portals
        SkyManager* mSkyManager            = nullptr;
        SceneUtil::RTTNode* mReflectionRTT = nullptr; ///< kept for fallback; superseded by per-portal reflection RTTs
        osg::Vec4f  mExteriorSkyColor      = osg::Vec4f(0.4f, 0.65f, 1.f, 1.f);
        osg::Vec4f  mExteriorAmbient       = osg::Vec4f(0.35f, 0.35f, 0.35f, 1.f);
        osg::Vec4f  mExteriorDiffuse       = osg::Vec4f(0.85f, 0.80f, 0.70f, 1.f);
        osg::Vec3f  mExteriorSunDir        = osg::Vec3f(0.5f, -0.5f, 1.f);
        float       mNearClip              = 1.f;
        osg::ref_ptr<osg::Group> mDebugShapesNode; ///< Mask_Debug wireframe for active portal collision geometry
        double      mLastCrossingMs        = -1e9; ///< steady_clock ms at last portal crossing (for rapid-crossing log)
    };

}

#endif
