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
    explicit PortalRTTNode(osg::Group* portalScene, uint32_t width, uint32_t height);

    void setViewMatrix(const osg::Matrix& v) { mViewMatrix = v; }
    void setProjectionMatrix(const osg::Matrix& p) { mProjMatrix = p; }
    void setClipEnabled(bool enabled) { mClipEnabled = enabled; }
    void setClearColor(const osg::Vec4f& color) { mClearColor = color; }
    /// Clip the RTT scene to geometry on the "forward" side of the portal plane.
    void setClipPlaneBoundary(const osg::Vec3f& normal, const osg::Vec3f& point);

    void setDefaults(osg::Camera* camera) override;
    void apply(osg::Camera* camera) override;

private:
    osg::ref_ptr<osg::Group>     mPortalScene;
    osg::ref_ptr<osg::ClipPlane> mClipPlane;
    osg::Matrix mViewMatrix;
    osg::Matrix mProjMatrix;
    bool        mClipEnabled  = true;
    osg::Vec4f  mClearColor   = osg::Vec4f(0.f, 0.f, 0.f, 1.f);
    osg::Vec3f  mPlaneNormal;   ///< world-space clip plane normal (points into destination)
    osg::Vec3f  mPlanePoint;    ///< world-space point on the clip plane
};

} // namespace MWRender

#endif
