#include "portalrttnode.hpp"

#include <osg/Camera>
#include <osg/ClipNode>
#include <osg/ClipPlane>
#include <osg/Group>

#include <components/sceneutil/depth.hpp>

#include "vismask.hpp"

namespace MWRender
{

PortalRTTNode::PortalRTTNode(osg::Group* portalScene, uint32_t width, uint32_t height)
    : RTTNode(width, height, 0, false, -1, StereoAwareness::Unaware, false)
    , mPortalScene(portalScene)
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
    clipNode->addClipPlane(mClipPlane.get());
    clipNode->addChild(mPortalScene);
    camera->addChild(clipNode);
}

void PortalRTTNode::apply(osg::Camera* camera)
{
    camera->setViewMatrix(mViewMatrix);
    camera->setProjectionMatrix(mProjMatrix);
    camera->setCullMask(Mask_Scene | Mask_Terrain | Mask_Static | Mask_Object | Mask_Lighting | Mask_ParticleSystem);
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
