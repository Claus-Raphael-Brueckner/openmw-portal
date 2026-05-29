#ifndef OPENMW_MWRENDER_PORTALRTTNODE_H
#define OPENMW_MWRENDER_PORTALRTTNODE_H

#include <osg/Matrix>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <components/sceneutil/rtt.hpp>

namespace osg
{
    class ClipPlane;
    class Group;
}

namespace MWRender
{

class PortalRTTNode : public SceneUtil::RTTNode
{
public:
    static constexpr uint32_t sSize = 512;

    explicit PortalRTTNode(osg::Group* portalScene);

    void setViewMatrix(const osg::Matrix& v) { mViewMatrix = v; }
    void setAspect(float aspect) { mAspect = aspect; }
    void setClipEnabled(bool enabled) { mClipEnabled = enabled; }
    /// Set the near-plane distance used for the off-center frustum.
    void setNear(double n) { mFrustumNear = n; }
    /// Override projection with an off-center frustum (values at the near plane).
    void setProjectionFrustum(double l, double r, double b, double t)
        { mFrustumL = l; mFrustumR = r; mFrustumB = b; mFrustumT = t; mHasFrustum = true; }
    /// Clip the RTT scene to geometry on the "forward" side of the portal plane.
    void setClipPlaneBoundary(const osg::Vec3f& normal, const osg::Vec3f& point);

    void setDefaults(osg::Camera* camera) override;
    void apply(osg::Camera* camera) override;

private:
    osg::ref_ptr<osg::Group>    mPortalScene;
    osg::ref_ptr<osg::ClipPlane> mClipPlane;
    osg::Matrix mViewMatrix;
    float       mAspect       = 1.f;
    bool        mClipEnabled  = true;
    bool        mHasFrustum   = false;
    double      mFrustumNear  = 4.0;
    double      mFrustumL = 0, mFrustumR = 0, mFrustumB = 0, mFrustumT = 0;
    osg::Vec3f  mPlaneNormal;   ///< world-space clip plane normal (points into destination)
    osg::Vec3f  mPlanePoint;    ///< world-space point on the clip plane
};

} // namespace MWRender

#endif
