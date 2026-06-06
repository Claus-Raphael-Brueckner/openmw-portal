#include "portalrttnode.hpp"

#include <osg/Camera>
#include <osg/ClipNode>
#include <osg/ClipPlane>
#include <osg/Group>

#include <components/sceneutil/depth.hpp>

#include "vismask.hpp"

namespace MWRender
{

PortalRTTNode::PortalRTTNode(osg::Group* portalScene, osg::Group* skyScene, uint32_t width, uint32_t height, bool addMSAAIntermediateTarget)
    : RTTNode(width, height, 0, false, -1, StereoAwareness::Unaware, addMSAAIntermediateTarget)
    , mPortalScene(portalScene)
    , mSkyScene(skyScene)
    , mViewMatrix(osg::Matrix::identity())
    , mProjMatrix(osg::Matrix::identity())
{
}

void PortalRTTNode::setClipPlaneBoundary(const osg::Vec3f& normal, const osg::Vec3f& point)
{
    mPlaneNormal = normal;
    mPlanePoint  = point;
}

void PortalRTTNode::setDefaults(osg::Camera* camera)
{
    camera->setName("PortalCamera");
    camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    camera->setClearColor(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    SceneUtil::setCameraClearDepth(camera);

    camera->setNodeMask(Mask_RenderToTexture);

    // Clip plane (GL_CLIP_PLANE0): kept on a ClipNode so it only affects the portal scene.
    // Equation is updated each frame in apply() in eye space.
    mClipPlane = new osg::ClipPlane(0, 0.0, 0.0, 1.0, 0.0); // default: always pass
    osg::ref_ptr<osg::ClipNode> clipNode = new osg::ClipNode;
    {
        osg::ref_ptr<osg::StateSet> clipSS = clipNode->getOrCreateStateSet();
        // Sky renders in RenderBin_Sky=-1 with depth write=false. OSG does not reset states that
        // the next StateSet leaves unspecified, so depth write=false leaks into terrain/statics
        // (bin 0) causing tree depth occlusion to break. Explicitly restore depth write=true here.
        osg::ref_ptr<osg::Depth> sceneDepth = new SceneUtil::AutoDepth;
        sceneDepth->setWriteMask(true);
        clipSS->setAttributeAndModes(sceneDepth, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        // Explicitly enable GL_CLIP_PLANE0 via the mode map as well as the attribute mechanism.
        // OSG tracks modes and attributes separately; if the sky StateSet previously set
        // GL_CLIP_PLANE0=OFF via mode, OSG's mode-map might not re-enable the clip plane even
        // though the ClipPlane attribute calls glEnable. Setting the mode to ON here ensures both
        // tracking paths agree and the clip plane is reliably activated.
        clipSS->setMode(GL_CLIP_PLANE0, osg::StateAttribute::ON);
        // Also reset GL_BLEND to OFF so sky's leaked GL_BLEND=ON cannot interact with opaque
        // terrain rendering. Alpha-blended objects (vegetation) re-enable GL_BLEND in their own
        // StateSets (child overrides parent), so transparent geometry is unaffected.
        clipSS->setMode(GL_BLEND, osg::StateAttribute::OFF);
    }
    clipNode->addClipPlane(mClipPlane.get());
    clipNode->addChild(mPortalScene);
    camera->addChild(clipNode);

    // Sky is camera-relative and must never be clipped by the portal plane.
    // Add it directly to the camera node, outside the ClipNode subtree.
    if (mSkyScene)
        camera->addChild(mSkyScene);
}

void PortalRTTNode::apply(osg::Camera* camera)
{
    camera->setClearColor(mClearColor);
    camera->setViewMatrix(mViewMatrix);
    camera->setProjectionMatrix(mProjMatrix);
    // Mask_Sky included for exterior portals (atmosphere/clouds/moons via mSkyNode wrapper).
    // Mask_Sun intentionally excluded: Sun uses occlusion queries that crash with a second camera.
    camera->setCullMask(Mask_Scene | Mask_Sky | Mask_Terrain | Mask_Static | Mask_Object | Mask_Lighting | Mask_ParticleSystem);
    // Mask_PortalQuad intentionally excluded: portal quads must not appear inside RTT views.

    // Update the clip plane in WORLD space. glClipPlane transforms by the current model-view
    // (= camera view matrix for ABSOLUTE_RF children) to produce the correct eye-space equation.
    // Passing eye-space coordinates would double-transform and produce an incorrect clip.
    //
    // Normal = forward (inward, into destination). This keeps geometry on the same side as the
    // RTT camera (cave interior) and clips the opposite side (outdoor world). Identical to the
    // OpenGL near-plane convention: near-plane normal also points away from the camera into the scene.
    if (mClipEnabled && mClipPlane && mPlaneNormal.length2() > 0.f)
    {
        const double d = -(static_cast<double>(mPlaneNormal.x()) * mPlanePoint.x()
                         + static_cast<double>(mPlaneNormal.y()) * mPlanePoint.y()
                         + static_cast<double>(mPlaneNormal.z()) * mPlanePoint.z());
        mClipPlane->setClipPlane(
            static_cast<double>(mPlaneNormal.x()),
            static_cast<double>(mPlaneNormal.y()),
            static_cast<double>(mPlaneNormal.z()),
            d);
    }
    else if (mClipPlane)
    {
        // Disabled: set a trivially-passing equation so the stale ClipNode never clips anything.
        mClipPlane->setClipPlane(0.0, 0.0, 0.0, 1.0);
    }
}

} // namespace MWRender
