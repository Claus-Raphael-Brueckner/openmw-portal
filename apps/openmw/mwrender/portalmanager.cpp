#include "portalmanager.hpp"
#include "sky.hpp"
#include "skyutil.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <components/debug/debuglog.hpp>

#include <osg/ComputeBoundsVisitor>
#include <osg/ClipNode>
#include <osg/ClipPlane>
#include <osg/Depth>
#include <osg/Fog>
#include <osg/FrontFace>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Image>
#include <osg/Light>
#include <osg/LightModel>
#include <osg/LightSource>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/Texture2D>

#include <osg/NodeCallback>
#include <osgUtil/CullVisitor>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <components/sceneutil/lightcontroller.hpp>

#include <components/esm3/loadacti.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loaddoor.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/vfs/manager.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/lightcommon.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/util.hpp>
#include <components/sceneutil/lightutil.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/statesetupdater.hpp>
#include <components/settings/values.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/fallback/fallback.hpp>
#include <components/misc/constants.hpp>
#include <components/nifosg/controller.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/sceneutil/controller.hpp>
#include <components/sceneutil/statesetupdater.hpp>
#include <components/stereo/stereomanager.hpp>
#include <components/sceneutil/waterutil.hpp>
#include "renderbin.hpp"
#include <components/vfs/pathutil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "portalrttnode.hpp"
#include "ripples.hpp"
#include "vismask.hpp"

#include <components/sceneutil/rtt.hpp>

namespace MWRender
{
    namespace
    {
        static const std::initializer_list<std::string_view> sDefaultPortalModels = {
            "ex_cave_door_01.nif",
            "in_cave_door_01.nif",
            "ex_nord_door_01.nif",
            "hlaalu_loaddoor_ 02.nif",
            "in_hlaalu_loaddoor_01.nif",
            "in_hlaalu_door.nif",
            "hlaalu_loaddoor_ 01.nif",
            "ex_velothi_loaddoor_01.nif",
            "in_velothismall_ndoor_01.nif",
            "ex_common_door_01.nif",
            "ex_common_door_balcony.nif",
            "in_c_door_wood_square.nif",
            "ex_ashl_door_01.nif",
            "ex_ashl_door_02.nif",
            "in_ashl_door_01.nif",
            "in_ashl_door_02.nif",
            "ex_imp_loaddoor_03.nif",
            "in_impsmall_loaddoor_01.nif",
            "ex_imp_loaddoor_02.nif",
            "in_ar_door_01.nif",
            "ex_redoran_barracks_door.nif",
            "in_redoran_barracks_door.nif",
            "in_r_s_door_01.nif",
            "in_redoran_hut_door_01.nif",
            "ex_redoran_hut_01_a.nif",
            "in_de_shack_door.nif",
            "ex_de_shack_door.nif",
            "ex_t_door_01.nif",
            "ex_t_door_02.nif",
            "in_t_housepod_door_exit.nif",
            "in_t_s_plain_door.nif",
            "ex_colony_door01.nif",
            "ex_colony_door01_1b.nif",
            "ex_colony_door02.nif",
            "ex_colony_door02_1.nif",
            "ex_colony_door02_2.nif",
            "ex_colony_door02b_2.nif",
            "ex_colony_door03.nif",
            "ex_colony_door03 int.nif",
            "ex_colony_door03_1.nif",
            "ex_colony_door03_1_uryn.nif",
            "ex_colony_door03_2.nif",
            "ex_colony_door03_4.nif",
            "ex_colony_door03_4a.nif",
            "ex_colony_door03_int.nif",
            "ex_colony_door04.nif",
            "ex_colony_door04_1.nif",
            "ex_colony_door04_2.nif",
            "ex_colony_door04_2b.nif",
            "ex_colony_door04_3.nif",
            "ex_colony_door04b_3.nif",
            "ex_colony_door04c_3.nif",
            "ex_colony_door05.nif",
            "ex_colony_door05_2.nif",
            "ex_colony_door05_3.nif",
            "ex_colony_door05_int.nif",
            "ex_colony_door05_int_a.nif",
            "ex_colony_door05_int_b.nif",
            "ex_colony_door05_int_c.nif",
            "ex_colony_door05a_4.nif",
            "ex_colony_door05b_4.nif",
            "ex_colony_door05c_4.nif",
            "ex_colony_door06.nif",
            "ex_colony_door07.nif",
            "ex_colony_door08.nif",
            "ex_colony_bardoor.nif",
            "ex_colony_minedoor.nif",
            "bm_ic_door_01.nif",
            "bm_ic_door_pelt.nif",
            "bm_ic_door_pelt_dark.nif",
            "bm_ic_door_pelt_wolf.nif",
            "ex_bm_tomb_door_01.nif",
            "ex_bm_tomb_door_02.nif",
            "ex_bm_tomb_door_03.nif",
            "ex_bm_tomb_door_skaalara.nif",
            "ex_s_door.nif",
            "ex_s_door_double.nif",
            "ex_s_door_double_gh.nif",
            "ex_s_door_double_fixed.nif",
            "ex_s_door_double_fixing.nif",
            "ex_s_door_rigmor.nif",
            "ex_s_door_rigmor2.nif",
            "ex_s_door_rounded.nif",
            "in_s_door.nif",
            "in_s_doorjam.nif",
            "in_s_doorjam_rounded.nif",
            "in_s_doorway.nif",
            "in_m_sewer_door_01.nif",
            "ex_mh_palace_gate.nif",
            "ex_mh_temple_door_01.nif",
            "in_mh_door_01.nif",
            "in_mh_door_01_nerev.nif",
            "in_mh_door_01_velas.nif",
            "in_mh_door_02.nif",
            "in_mh_door_02_bar1_uni.nif",
            "in_mh_door_02_bar2_uni.nif",
            "in_mh_door_02_bar3_uni.nif",
            "in_mh_door_02_bar4_uni.nif",
            "in_mh_door_02_chapel.nif",
            "in_mh_door_02_hels_uni.nif",
            "in_mh_door_02_play.nif",
            "in_mh_door_02_throne1.nif",
            "in_mh_door_02_throne2.nif",
        };


        // 1×1 RGBA8 texture filled with a constant color.
        osg::ref_ptr<osg::Texture2D> makeSolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
        {
            osg::ref_ptr<osg::Image> img = new osg::Image;
            img->allocateImage(1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            unsigned char* d = img->data();
            d[0] = r; d[1] = g; d[2] = b; d[3] = a;
            osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(img);
            tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
            tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
            return tex;
        }

        // Magenta wireframe group visualising the portal collision shapes:
        // floor/ramp box and two edge cylinders (Mask_Debug → main view only).
        [[maybe_unused]] osg::ref_ptr<osg::Group> buildPortalCollisionDebug(
            bool hasFloor, const osg::Vec3f& floorCenter, const osg::Quat& floorRot,
            float floorHalfX, float floorHalfY,
            const osg::Vec3f& cylLeft, const osg::Vec3f& cylRight,
            float cylRadius, float cylHalfH)
        {
            osg::ref_ptr<osg::Group> grp = new osg::Group;
            grp->setNodeMask(Mask_Debug);

            osg::ref_ptr<osg::StateSet> ss = grp->getOrCreateStateSet();
            ss->setAttributeAndModes(
                new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
            ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

            const osg::Vec4f magenta(1.f, 0.f, 1.f, 1.f);
            auto colorShape = [&](osg::Shape* shape) -> osg::ref_ptr<osg::Geode>
            {
                osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(shape);
                sd->setColor(magenta);
                osg::ref_ptr<osg::Geode> g = new osg::Geode;
                g->addDrawable(sd);
                return g;
            };
            auto addBox = [&](osg::Group* parent, const osg::Vec3f& center, const osg::Quat& rot,
                               float hx, float hy, float hz)
            {
                osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
                osg::Matrix m; m.setRotate(rot); m.setTrans(center);
                mt->setMatrix(m);
                mt->addChild(colorShape(new osg::Box(osg::Vec3f(), hx*2.f, hy*2.f, hz*2.f)));
                parent->addChild(mt);
            };

            if (hasFloor)
                addBox(grp.get(), floorCenter, floorRot, floorHalfX, floorHalfY, 10.f);

            grp->addChild(colorShape(new osg::Cylinder(cylLeft,  cylRadius, 2.f * cylHalfH)));
            grp->addChild(colorShape(new osg::Cylinder(cylRight, cylRadius, 2.f * cylHalfH)));

            return grp;
        }

        // CullCallback on the portal water PAT: binds reflection (unit 1) and optionally
        // refraction color (unit 2) + depth (unit 3) from portal-specific RTT cameras.
        // Uses getFirstColorTexture()/getFirstDepthTexture() because apply() is invoked from
        // the portal RTT's own cull visitor, which has no entry in the reflection/refraction
        // RTTNode's ViewDependentDataMap (those RTTs live in the main scene, not the portal scene).
        class PortalWaterRTTUpdater : public SceneUtil::StateSetUpdater
        {
        public:
            PortalWaterRTTUpdater(SceneUtil::RTTNode* reflRTT, SceneUtil::RTTNode* refrRTT, osg::Texture2D* fallback)
                : mReflectionRTT(reflRTT), mRefractionRTT(refrRTT), mFallback(fallback) {}

            void setDefaults(osg::StateSet* ss) override
            {
                ss->setTextureAttributeAndModes(1, mFallback, osg::StateAttribute::ON);
            }

            void apply(osg::StateSet* ss, osg::NodeVisitor*) override
            {
                osg::Texture* reflTex = mReflectionRTT ? mReflectionRTT->getFirstColorTexture() : nullptr;
                ss->setTextureAttributeAndModes(1, reflTex ? reflTex : mFallback.get(), osg::StateAttribute::ON);

                if (mRefractionRTT)
                {
                    osg::Texture* refrColor = mRefractionRTT->getFirstColorTexture();
                    osg::Texture* refrDepth = mRefractionRTT->getFirstDepthTexture();
                    if (refrColor)
                        ss->setTextureAttributeAndModes(2, refrColor, osg::StateAttribute::ON);
                    if (refrDepth)
                        ss->setTextureAttributeAndModes(3, refrDepth, osg::StateAttribute::ON);
                }
            }

        private:
            osg::ref_ptr<SceneUtil::RTTNode> mReflectionRTT;
            osg::ref_ptr<SceneUtil::RTTNode> mRefractionRTT;
            osg::ref_ptr<osg::Texture2D>     mFallback;
        };

        // RTTNode for portal water reflection: renders the portal scene from the portal camera's
        // viewpoint reflected over the water plane. Must render BEFORE the portal RTT (order -3).
        class PortalReflectionRTTNode : public SceneUtil::RTTNode
        {
        public:
            PortalReflectionRTTNode(osg::Group* scene, float waterHeight, uint32_t w, uint32_t h, bool msaa)
                : RTTNode(w, h, 0, false, -3, StereoAwareness::Unaware, msaa)
                , mScene(scene), mWaterHeight(waterHeight)
            {
                setDepthBufferInternalFormat(GL_DEPTH24_STENCIL8);
            }

            void setDefaults(osg::Camera* camera) override
            {
                camera->setName("PortalReflectionCamera");
                camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
                camera->setClearColor(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
                camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
                camera->setNodeMask(Mask_RenderToTexture);
                SceneUtil::setCameraClearDepth(camera);

                osg::ref_ptr<osg::StateSet> ss = camera->getOrCreateStateSet();
                // Reflection flips winding order
                ss->setAttributeAndModes(new osg::FrontFace(osg::FrontFace::CLOCKWISE), osg::StateAttribute::ON);
                ss->addUniform(new osg::Uniform("isReflection", true));
                SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*ss);

                // Clip plane: keep world z >= waterHeight (discard below-water geometry).
                // World-space coefficients are passed here; OpenGL multiplies by (MV)^{-T} to
                // get the correct eye-space equation, preserving world-z semantics for any MV.
                osg::ref_ptr<osg::ClipPlane> clipPlane = new osg::ClipPlane(
                    0, 0.0, 0.0, 1.0, -static_cast<double>(mWaterHeight));
                osg::ref_ptr<osg::ClipNode> clipNode = new osg::ClipNode;
                clipNode->addClipPlane(clipPlane);
                clipNode->setStateSetModes(*clipNode->getOrCreateStateSet(), osg::StateAttribute::ON);
                clipNode->setCullingActive(false);
                clipNode->addChild(mScene);
                camera->addChild(clipNode);
            }

            void apply(osg::Camera* camera) override
            {
                camera->setViewMatrix(mViewMatrix);
                camera->setProjectionMatrix(mProjMatrix);
                // No Mask_Water — no recursive reflection
                camera->setCullMask(Mask_Scene | Mask_Sky | Mask_Terrain | Mask_Static | Mask_Object
                                  | Mask_Lighting | Mask_ParticleSystem);
            }

            void setMatrices(const osg::Matrixd& portalView, const osg::Matrixd& proj)
            {
                // Reflect portal camera over the water plane (matches Water::Reflection's approach
                // for RELATIVE_RF: finalMV = scale(1,1,-1) * translate(0,0,2h) * parentView)
                mViewMatrix = osg::Matrix::scale(1.0, 1.0, -1.0)
                            * osg::Matrix::translate(0.0, 0.0, 2.0 * mWaterHeight)
                            * portalView;
                mProjMatrix = proj;
            }

        private:
            osg::ref_ptr<osg::Group> mScene;
            float       mWaterHeight;
            osg::Matrixd mViewMatrix;
            osg::Matrixd mProjMatrix;
        };

        // RTTNode for portal water refraction: renders the portal scene from the portal camera's
        // viewpoint with a slight vertical compression and clipped above the water plane.
        // Provides refractionMap (unit 2) and refractionDepthMap (unit 3). Order: -2.
        class PortalRefractionRTTNode : public SceneUtil::RTTNode
        {
        public:
            PortalRefractionRTTNode(osg::Group* scene, float waterHeight, uint32_t w, uint32_t h, bool msaa)
                : RTTNode(w, h, 0, false, -2, StereoAwareness::Unaware, msaa)
                , mScene(scene), mWaterHeight(waterHeight)
            {
                setDepthBufferInternalFormat(GL_DEPTH24_STENCIL8);
            }

            void setDefaults(osg::Camera* camera) override
            {
                camera->setName("PortalRefractionCamera");
                camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
                camera->setClearColor(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
                camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
                camera->setNodeMask(Mask_RenderToTexture);
                SceneUtil::setCameraClearDepth(camera);

                osg::ref_ptr<osg::StateSet> ss = camera->getOrCreateStateSet();
                SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*ss);

                // Clip plane: keep world z <= waterHeight (discard above-water geometry).
                osg::ref_ptr<osg::ClipPlane> clipPlane = new osg::ClipPlane(
                    0, 0.0, 0.0, -1.0, static_cast<double>(mWaterHeight));
                osg::ref_ptr<osg::ClipNode> clipNode = new osg::ClipNode;
                clipNode->addClipPlane(clipPlane);
                clipNode->setStateSetModes(*clipNode->getOrCreateStateSet(), osg::StateAttribute::ON);
                clipNode->setCullingActive(false);
                clipNode->addChild(mScene);
                camera->addChild(clipNode);
            }

            void apply(osg::Camera* camera) override
            {
                camera->setViewMatrix(mViewMatrix);
                camera->setProjectionMatrix(mProjMatrix);
                // No sky in refraction (underwater view)
                camera->setCullMask(Mask_Scene | Mask_Terrain | Mask_Static | Mask_Object
                                  | Mask_Lighting | Mask_ParticleSystem);
            }

            void setMatrices(const osg::Matrixd& portalView, const osg::Matrixd& proj)
            {
                // Vertical compression matches Water::Refraction for consistent depth appearance
                const float s = Settings::water().mRefractionScale;
                mViewMatrix = osg::Matrix::scale(1.0, 1.0, static_cast<double>(s))
                            * osg::Matrix::translate(0.0, 0.0, (1.0 - s) * mWaterHeight)
                            * portalView;
                mProjMatrix = proj;
            }

        private:
            osg::ref_ptr<osg::Group> mScene;
            float        mWaterHeight;
            osg::Matrixd mViewMatrix;
            osg::Matrixd mProjMatrix;
        };

        // Build a water plane for exterior portal scenes.
        // Uses dedicated portal reflection/refraction RTTs for correct water appearance.
        osg::ref_ptr<osg::PositionAttitudeTransform> createPortalWaterNode(
            float waterHeight,
            Resource::ResourceSystem* resourceSystem,
            Shader::ShaderManager& shaderMgr,
            const osg::Vec4f& skyColor,
            float nearClip,
            osg::ref_ptr<osg::Texture2D>& outSkyTex,
            SceneUtil::RTTNode* reflectionRTT,
            SceneUtil::RTTNode* refractionRTT)
        {
            osg::ref_ptr<osg::Geometry> waterGeom
                = SceneUtil::createWaterGeometry(Constants::CellSizeInUnits * 150, 40, 900);
            waterGeom->setCullingActive(false);
            waterGeom->setNodeMask(Mask_Water);

            osg::ref_ptr<osg::PositionAttitudeTransform> waterNode = new osg::PositionAttitudeTransform;
            waterNode->setPosition(osg::Vec3f(0.f, 0.f, waterHeight));
            waterNode->setNodeMask(Mask_Water);
            // Disable frustum culling on the PAT: the water geometry has an empty bounding box
            // (via WaterBoundCallback) to suppress the "huge triangle" cull warning, which makes
            // the PAT's derived bounding sphere empty too — causing it to be frustum-culled away.
            waterNode->setCullingActive(false);
            waterNode->addChild(waterGeom);

            const bool hasRefraction = (refractionRTT != nullptr);
            bool shaderOk = false;
            try
            {
                Shader::ShaderManager::DefineMap defines;
                defines["waterRefraction"]     = hasRefraction ? "1" : "0";
                defines["rainRippleDetail"]    = "0";
                defines["rippleMapWorldScale"] = std::to_string(MWRender::RipplesSurface::sWorldScaleFactor);
                defines["rippleMapSize"]       = std::to_string(MWRender::RipplesSurface::sRTTSize) + ".0";
                defines["sunlightScattering"]  = Settings::water().mSunlightScattering ? "1" : "0";
                defines["wobblyShores"]        = "0";
                Stereo::shaderStereoDefines(defines);

                osg::ref_ptr<osg::Program> program = shaderMgr.getProgram("water", defines);

                constexpr VFS::Path::NormalizedView waterNM("textures/omw/water_nm.png");
                osg::ref_ptr<osg::Texture2D> normalMap(
                    new osg::Texture2D(resourceSystem->getImageManager()->getImage(waterNM)));
                normalMap->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
                normalMap->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
                resourceSystem->getSceneManager()->applyFilterSettings(normalMap);

                // Water reflects overhead sky, which is ~40% of the horizon clear-color.
                // Using the raw sky color directly produces water that is far too bright.
                constexpr float kReflectionScale = 0.4f;
                osg::ref_ptr<osg::Texture2D> skyTex = makeSolidTexture(
                    static_cast<unsigned char>(std::clamp(skyColor.r() * kReflectionScale, 0.f, 1.f) * 255.f),
                    static_cast<unsigned char>(std::clamp(skyColor.g() * kReflectionScale, 0.f, 1.f) * 255.f),
                    static_cast<unsigned char>(std::clamp(skyColor.b() * kReflectionScale, 0.f, 1.f) * 255.f), 255);

                osg::ref_ptr<osg::Texture2D> rippleDummy = makeSolidTexture(128, 128, 0, 0);

                // Set the water shader StateSet directly on the geometry node — same pattern as
                // simple water (setStateSet on mWaterGeom) and confirmed to reach the render leaf.
                // Using setStateSet avoids StateSetUpdater push/pop ordering issues and bounding-sphere
                // culling of the PAT when the geometry reports an empty bounding box.
                osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;
                ss->setAttributeAndModes(program, osg::StateAttribute::ON);
                ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
                // Water at sea level (z≈-1) is below the destination door (z≈268).
                // The portal clip plane passes through the door and clips the water plane,
                // removing exactly the visible portion. Water is always in the exterior so
                // clipping it is wrong — disable GL_CLIP_PLANE0 for this geometry.
                ss->setMode(GL_CLIP_PLANE0, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
                // Render opaque: portal.frag forces alpha=1 anyway, and the shader outputs
                // near-zero alpha at steep viewing angles (low Fresnel), hiding the surface.
                ss->setMode(GL_BLEND, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
                osg::ref_ptr<osg::Depth> depth = new SceneUtil::AutoDepth;
                depth->setWriteMask(true);
                ss->setAttributeAndModes(depth, osg::StateAttribute::ON);
                ss->setRenderBinDetails(RenderBin_Default, "RenderBin");

                ss->addUniform(new osg::Uniform("normalMap",     0));
                ss->setTextureAttributeAndModes(0, normalMap,    osg::StateAttribute::ON);
                ss->addUniform(new osg::Uniform("reflectionMap", 1));
                // Units 1/2/3 are NOT bound here — PortalWaterRTTUpdater on the PAT provides
                // them each frame from the portal-specific reflection/refraction RTTs.
                if (hasRefraction)
                {
                    ss->addUniform(new osg::Uniform("refractionMap",      2));
                    ss->addUniform(new osg::Uniform("refractionDepthMap", 3));
                }
                ss->addUniform(new osg::Uniform("rippleMap",     4));
                ss->setTextureAttributeAndModes(4, rippleDummy,  osg::StateAttribute::ON);

                ss->addUniform(new osg::Uniform("near",          nearClip));
                ss->addUniform(new osg::Uniform("nodePosition",  osg::Vec3f(0.f, 0.f, waterHeight)));
                ss->addUniform(new osg::Uniform("rainIntensity", 0.f));

                waterGeom->setStateSet(ss);
                waterNode->addCullCallback(new PortalWaterRTTUpdater(reflectionRTT, refractionRTT, skyTex.get()));
                outSkyTex = skyTex;
                shaderOk = true;
            }
            catch (...) {}

            if (!shaderOk)
            {
                // Fallback: simple animated water.
                waterGeom->setNodeMask(Mask_SimpleWater);
                waterNode->setNodeMask(Mask_SimpleWater);
                const float alpha = Fallback::Map::getFloat("Water_World_Alpha");
                osg::ref_ptr<osg::StateSet> ss = SceneUtil::createSimpleWaterStateSet(alpha, RenderBin_Water);
                waterGeom->setStateSet(ss);

                std::vector<osg::ref_ptr<osg::Texture2D>> textures;
                const int frameCount = std::clamp(Fallback::Map::getInt("Water_SurfaceFrameCount"), 0, 320);
                const std::string_view textureName = Fallback::Map::getString("Water_SurfaceTexture");
                for (int i = 0; i < frameCount; ++i)
                {
                    std::ostringstream texname;
                    texname << "textures/water/" << textureName
                            << std::setw(2) << std::setfill('0') << i << ".dds";
                    try
                    {
                        osg::ref_ptr<osg::Texture2D> tex(
                            new osg::Texture2D(resourceSystem->getImageManager()->getImage(
                                VFS::Path::Normalized(texname.str()))));
                        tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
                        tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
                        resourceSystem->getSceneManager()->applyFilterSettings(tex);
                        textures.push_back(tex);
                    }
                    catch (...) {}
                }
                if (!textures.empty())
                {
                    const float fps = Fallback::Map::getFloat("Water_SurfaceFPS");
                    osg::ref_ptr<NifOsg::FlipController> ctrl(
                        new NifOsg::FlipController(0, 1.f / fps, textures));
                    ctrl->setSource(std::make_shared<SceneUtil::FrameTimeSource>());
                    waterGeom->setUpdateCallback(ctrl);
                    ss->setTextureAttributeAndModes(0, textures[0], osg::StateAttribute::ON);
                    resourceSystem->getSceneManager()->recreateShaders(waterGeom);
                }
            }

            return waterNode;
        }

        // Returns the rotation of the NIF root node (identity if root is a plain Group).
        osg::Quat getNifRootQuat(const osg::Node* node)
        {
            const auto* mt = dynamic_cast<const osg::MatrixTransform*>(node);
            if (!mt)
                return osg::Quat();
            osg::Quat q, so;
            osg::Vec3d t, s;
            mt->getMatrix().decompose(t, q, s, so);
            return q;
        }

        // CullCallback on the portal quad node: binds the RTT texture and portal shader each frame.
        class PortalStateSetUpdater : public SceneUtil::StateSetUpdater
        {
        public:
            PortalStateSetUpdater(PortalRTTNode* rtt, osg::Program* program)
                : mRTT(rtt)
                , mProgram(program)
            {
            }

            void setDefaults(osg::StateSet* stateset) override
            {
                stateset->addUniform(new osg::Uniform("portalTex", 0));
                stateset->setAttributeAndModes(mProgram, osg::StateAttribute::ON);
            }

            void apply(osg::StateSet* stateset, osg::NodeVisitor* nv) override
            {
                auto* cv = static_cast<osgUtil::CullVisitor*>(nv);
                stateset->setTextureAttributeAndModes(
                    0, mRTT->getColorTexture(cv), osg::StateAttribute::ON);
            }

        private:
            osg::ref_ptr<PortalRTTNode> mRTT;
            osg::ref_ptr<osg::Program>  mProgram;
        };

        // Cull callback on the LightManager: re-populates mLights and clears the view-space
        // cache every frame. Needed because RTTNode is an osg::Node — the OSG update traversal
        // never reaches the internal camera subtree, so LightManagerUpdateCallback never fires
        // and mLightsInViewSpace accumulates stale view-space positions causing flickering.
        class PortalLightRefresher : public osg::NodeCallback
        {
        public:
            struct Entry
            {
                osg::ref_ptr<SceneUtil::LightSource>    ls;
                osg::Vec3f                              pos;
                // Direct pointer into the callback chain (owned by ls via addUpdateCallback).
                // Used to drive flicker/pulse without going through the full OSG update traversal,
                // which crashes because CollectLightCallback modifies the LightManager and
                // osgUtil::UpdateVisitor propagates dirty-counts up parent chains outside update pass.
                SceneUtil::LightController*             ctrl = nullptr;
            };

            PortalLightRefresher(SceneUtil::LightManager* lm) : mLightManager(lm) {}

            void add(osg::ref_ptr<SceneUtil::LightSource> ls, const osg::Vec3f& pos)
            {
                // createLightSource chains: CollectLightCallback -> LightController
                // getNestedCallback() skips CollectLightCallback to reach LightController directly.
                auto* nested = ls->getUpdateCallback() ? ls->getUpdateCallback()->getNestedCallback() : nullptr;
                mEntries.push_back({ ls, pos, dynamic_cast<SceneUtil::LightController*>(nested) });
            }

            void setStatics(osg::Group* statics) { mStatics = statics; }

            void operator()(osg::Node* node, osg::NodeVisitor* nv) override
            {
                const size_t traversalNum = nv->getTraversalNumber();

                // The portal scene is shared by up to 3 cameras (reflection, refraction, portal).
                // update() + addLight() must run exactly once per frame — on the first cull pass.
                // Subsequent passes for the same frame skip the reset so mLights stays consistent
                // for every camera's LightListCallback. traverse() still runs for each camera so
                // LightManagerCullCallback can push per-camera sunlight state and cull statics.
                if (traversalNum != mLastUpdateTraversalNum)
                {
                    mLastUpdateTraversalNum = traversalNum;

                    // Drive LightController (flicker/pulse) directly, bypassing CollectLightCallback.
                    // TRAVERSE_NONE prevents any scene-graph modification from this path.
                    osg::NodeVisitor fakeNv(osg::NodeVisitor::UPDATE_VISITOR,
                                            osg::NodeVisitor::TRAVERSE_NONE);
                    fakeNv.setFrameStamp(const_cast<osg::FrameStamp*>(nv->getFrameStamp()));
                    fakeNv.setTraversalNumber(traversalNum);
                    for (const auto& e : mEntries)
                        if (e.ctrl) (*e.ctrl)(e.ls.get(), &fakeNv);

                    // Drive osgParticle emitters and ParticleSystemUpdater in the statics subtree
                    // (torch flames, candles, etc.) using a plain NodeVisitor — NOT osgUtil::UpdateVisitor,
                    // which propagates dirty-counts up parent chains and crashes inside a cull callback.
                    if (mStatics)
                    {
                        osg::NodeVisitor staticsNv(osg::NodeVisitor::UPDATE_VISITOR,
                                                   osg::NodeVisitor::TRAVERSE_ALL_CHILDREN);
                        staticsNv.setFrameStamp(const_cast<osg::FrameStamp*>(nv->getFrameStamp()));
                        staticsNv.setTraversalNumber(traversalNum);
                        mStatics->accept(staticsNv);
                    }

                    // Clears mLights and mLightsInViewSpace, then re-adds lights with fresh
                    // world matrices so getLightsInViewSpace rebuilds view-space positions
                    // against the current RTT camera view matrix this frame.
                    mLightManager->update(traversalNum);
                    for (const auto& e : mEntries)
                    {
                        osg::Matrixf worldMat;
                        worldMat.setTrans(e.pos);
                        mLightManager->addLight(e.ls.get(), worldMat, traversalNum);
                    }
                }

                traverse(node, nv);  // chains to LightManagerCullCallback
            }

        private:
            SceneUtil::LightManager*        mLightManager;
            osg::ref_ptr<osg::Group>        mStatics;
            std::vector<Entry>              mEntries;
            size_t                          mLastUpdateTraversalNum = std::numeric_limits<size_t>::max();
        };

        // Load static geometry from the destination cell into a plain group.
        // Only objects within maxDist (XY) of destCenter are included — important for exterior
        // cells that can contain thousands of statics spread across 8192×8192 units.
        // Also loads light meshes (torches, candles) so they are visually present in the portal.
        osg::ref_ptr<osg::Group> loadCellStatics(
            MWWorld::CellStore* cellStore,
            Resource::ResourceSystem* resourceSystem,
            const osg::Vec3f& destCenter,
            const std::unordered_set<std::string>& portalModels,
            float maxDist = 5000.f)
        {
            osg::ref_ptr<osg::Group> group = new osg::Group;

            if (!cellStore)
                return group;

            const float maxDistSq = maxDist * maxDist;

            auto addRefs = [&](auto& refList)
            {
                for (const auto& ref : refList)
                {
                    if (!ref.mBase || ref.mBase->mModel.empty())
                        continue;
                    {
                        std::string m = ref.mBase->mModel;
                        Misc::StringUtils::lowerCaseInPlace(m);
                        if (m.find("marker") != std::string::npos)
                            continue;
                    }
                    if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef))
                        continue;
                    if (!ref.mData.isEnabled())
                        continue;

                    const ESM::Position& pos = ref.mRef.getPosition();
                    const float dx = pos.pos[0] - destCenter.x();
                    const float dy = pos.pos[1] - destCenter.y();
                    if (dx * dx + dy * dy > maxDistSq)
                        continue;

                    try
                    {
                        VFS::Path::Normalized modelPath = Misc::ResourceHelpers::correctMeshPath(
                            VFS::Path::Normalized(ref.mBase->mModel));
                        osg::ref_ptr<osg::Node> node = resourceSystem->getSceneManager()->getInstance(modelPath);

                        const float scale = ref.mRef.getScale();

                        osg::ref_ptr<osg::PositionAttitudeTransform> pat = new osg::PositionAttitudeTransform;
                        pat->setPosition(pos.asVec3());
                        pat->setAttitude(Misc::Convert::makeOsgQuat(pos));
                        pat->setScale(osg::Vec3f(scale, scale, scale));
                        pat->addChild(node);
                        group->addChild(pat);
                    }
                    catch (...) {}
                }
            };

            addRefs(cellStore->getReadOnlyStatics().mList);
            for (const auto& ref : cellStore->getReadOnlyDoors().mList)
            {
                if (!ref.mBase || ref.mBase->mModel.empty()) continue;
                if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef)) continue;
                if (!ref.mData.isEnabled()) continue;
                if (ref.mRef.getTeleport())
                {
                    // Interior destination: skip portal doors (portal quads handle those).
                    // Exterior destination: show the original mesh so the door frame is visible.
                    if (!cellStore->getCell()->isExterior()) continue;
                    std::string m = ref.mBase->mModel;
                    Misc::StringUtils::lowerCaseInPlace(m);
                    bool known = false;
                    for (const auto& p : portalModels)
                        if (m.find(p) != std::string::npos) { known = true; break; }
                    if (!known) continue;
                }
                const ESM::Position& pos = ref.mRef.getPosition();
                const float dx = pos.pos[0] - destCenter.x();
                const float dy = pos.pos[1] - destCenter.y();
                if (dx * dx + dy * dy > maxDistSq) continue;
                // Skip the destination door itself — it sits right at the camera and would block the view.
                if (ref.mRef.getTeleport() && dx * dx + dy * dy < 150.f * 150.f) continue;
                try
                {
                    VFS::Path::Normalized modelPath = Misc::ResourceHelpers::correctMeshPath(
                        VFS::Path::Normalized(ref.mBase->mModel));
                    if (!resourceSystem->getVFS()->exists(modelPath)) continue;
                    osg::ref_ptr<osg::Node> node = resourceSystem->getSceneManager()->getInstance(modelPath);
                    const float scale = ref.mRef.getScale();
                    osg::ref_ptr<osg::PositionAttitudeTransform> pat = new osg::PositionAttitudeTransform;
                    pat->setPosition(pos.asVec3());
                    pat->setAttitude(Misc::Convert::makeOsgQuat(pos));
                    pat->setScale(osg::Vec3f(scale, scale, scale));
                    pat->addChild(node);
                    group->addChild(pat);
                }
                catch (...) {}
            }
            addRefs(cellStore->getReadOnlyContainers().mList);
            addRefs(cellStore->getReadOnlyActivators().mList);

            addRefs(cellStore->getReadOnlyLights().mList);

            return group;
        }

        struct PortalSceneResult
        {
            osg::ref_ptr<osg::Group>      scene;
            osg::ref_ptr<osg::Light>      sunLight;
            osg::ref_ptr<osg::LightModel> lightModelAttr;
            osg::ref_ptr<osg::Fog>        sceneFog;  ///< non-null for quasi-exterior; updated per-frame by caller
            float                         waterHeight = 0.f;
        };

        // Wrap the geometry in a LightManager with a directional light, cell point lights, and
        // all uniforms that OpenMW's object shaders expect. Does NOT add water — caller does that
        // after creating the reflection/refraction RTTs.
        PortalSceneResult buildPortalScene(
            MWWorld::CellStore* cellStore,
            const osg::Vec3f& destCenter,
            Resource::ResourceSystem* resourceSystem,
            osg::Group* exteriorTerrainNode,
            SkyManager* skyManager,
            const osg::Vec2f& screenRes,
            const osg::Vec4f& ambient,
            const osg::Vec4f& diffuse,
            const osg::Vec3f& sunDir,
            float maxDist,
            const std::unordered_set<std::string>& portalModels,
            const osg::Vec4f& skyColor = osg::Vec4f(0.4f, 0.65f, 1.f, 1.f))
        {
            osg::ref_ptr<osg::Group> statics = loadCellStatics(cellStore, resourceSystem, destCenter, portalModels, maxDist);
            // One LightListCallback on the whole group with an explicit large bound so it always
            // intersects with any light in the scene. Individual PAT bounds may be invalid before
            // the first render, causing per-PAT callbacks to silently skip all lights.
            statics->setInitialBound(osg::BoundingSphere(osg::Vec3f(destCenter), maxDist));
            statics->addCullCallback(new SceneUtil::LightListCallback);

            osg::ref_ptr<SceneUtil::LightManager> lightManager
                = new SceneUtil::LightManager(SceneUtil::LightSettings{
                    .mLightingMethod = resourceSystem->getSceneManager()->getLightingMethod(),
                    .mMaxLights = Settings::shaders().mMaxLights,
                });
            lightManager->setStartLight(1);
            // Disable distance-based fade: getLightsInViewSpace permanently multiplies
            // light->getDiffuse() by the fade factor when mPointLightFadeEnd != 0. Called
            // every frame by PortalLightRefresher, this accumulates into black. mPointLightFadeEnd=0
            // short-circuits the fade branch entirely.
            lightManager->processChangedSettings(1.f, 0.f, 0.f);

            osg::ref_ptr<osg::StateSet> ss = lightManager->getOrCreateStateSet();
            // FORCE_OPAQUE is intentionally NOT set here.
            // portal.frag already forces gl_FragData[0].a=1.0 on the portal quad output,
            // so FORCE_OPAQUE in the RTT scene is redundant. With alphaToCoverage enabled
            // (MSAA+AntialiasAlphaTest), FORCE_OPAQUE would override coverageAlpha≈0 back to 1.0
            // before GL_SAMPLE_ALPHA_TO_COVERAGE reads it, defeating alpha-tested transparency
            // and rendering tree leaves / thatch as solid black in the portal.
            ss->setMode(GL_NORMALIZE, osg::StateAttribute::ON);

            // Uniforms the object shaders depend on.
            ss->addUniform(new osg::Uniform("far", 100000.0f));
            ss->addUniform(new osg::Uniform("skyBlendingStart", 90000.0f));
            ss->addUniform(new osg::Uniform("screenRes", screenRes));
            ss->addUniform(new osg::Uniform("emissiveMult", 1.f));
            ss->addUniform(new osg::Uniform("specStrength", 1.f));

            // Fog: quasi-exterior cells (Mournhold-style) carry interior fog data — apply it so
            // the view through the portal matches what the player sees after crossing.
            // All other portal scenes keep fog disabled (exterior uses weather fog, regular
            // interiors are typically close enough that the difference isn't noticeable).
            osg::ref_ptr<osg::Fog> fog = new osg::Fog;
            fog->setMode(osg::Fog::LINEAR);
            osg::ref_ptr<osg::Fog> sceneFogOut;
            const bool needsWeatherFog = cellStore
                && (cellStore->getCell()->isExterior() || cellStore->getCell()->isQuasiExterior());
            if (needsWeatherFog)
            {
                // Initial values; caller updates this fog object every frame via portal.sceneFog.
                fog->setStart(1e8f);
                fog->setEnd(1e8f);
                ss->setAttributeAndModes(fog, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
                sceneFogOut = fog;
            }
            else
            {
                fog->setStart(1e8f);
                fog->setEnd(1e8f);
                ss->setAttributeAndModes(fog, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
            }

            SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*ss);

            // Dummy shadow depth texture on unit 7 (prevents GL errors from shadow samplers).
            osg::ref_ptr<osg::Texture2D> dummyTex = new osg::Texture2D;
            dummyTex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            dummyTex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            dummyTex->setInternalFormat(GL_DEPTH_COMPONENT);
            dummyTex->setTextureSize(1, 1);
            dummyTex->setShadowComparison(true);
            dummyTex->setShadowCompareFunc(osg::Texture::ShadowCompareFunc::ALWAYS);
            ss->setTextureAttributeAndModes(7, dummyTex, osg::StateAttribute::ON);

            // Directional light: for interiors use cell mood; for exteriors (and quasi-exteriors
            // like Mournhold) use the caller-supplied values updated each frame from RenderingManager.
            osg::Vec4f effectiveAmbient = ambient;
            osg::Vec4f effectiveDiffuse = diffuse;
            osg::Vec4f effectiveSunPos  = osg::Vec4f(sunDir, 0.f);
            const bool isExterior = cellStore && cellStore->getCell()->isExterior();
            const bool isQuasiExterior = cellStore && !isExterior && cellStore->getCell()->isQuasiExterior();
            if (cellStore && !isExterior && !isQuasiExterior)
            {
                const MWWorld::Cell* cell = cellStore->getCell();
                effectiveAmbient = SceneUtil::colourFromRGB(cell->getMood().mAmbiantColor);
                effectiveDiffuse = SceneUtil::colourFromRGB(cell->getMood().mDirectionalColor);
                // Interior sun convention (Morrowind style)
                effectiveSunPos = osg::Vec4f(-1.f, osg::DegreesToRadians(45.f), osg::DegreesToRadians(45.f), 0.f);
            }

            // Ambient on LightModel (matches CharacterPreview pattern and OpenMW shader expectations).
            osg::ref_ptr<osg::LightModel> lightModel = new osg::LightModel;
            lightModel->setAmbientIntensity(effectiveAmbient);
            ss->setAttributeAndModes(lightModel, osg::StateAttribute::ON);

            osg::ref_ptr<osg::Light> light = new osg::Light;
            light->setLightNum(0);
            light->setPosition(effectiveSunPos);
            light->setAmbient(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
            light->setDiffuse(effectiveDiffuse);
            light->setSpecular(osg::Vec4f(0.f, 0.f, 0.f, 0.f));
            light->setConstantAttenuation(1.f);
            light->setLinearAttenuation(0.f);
            light->setQuadraticAttenuation(0.f);
            lightManager->setSunlight(light);

            osg::ref_ptr<osg::LightSource> lightSource = new osg::LightSource;
            lightSource->setLight(light);
            lightSource->setStateSetModes(*ss, osg::StateAttribute::ON);
            lightManager->addChild(lightSource);

            // RTTNode is an osg::Node — update traversal never reaches the internal camera
            // subtree, so CollectLightCallback never fires. We bypass it with PortalLightRefresher,
            // a cull callback that re-populates mLights and clears the view-space cache every frame.
            osg::ref_ptr<PortalLightRefresher> refresher = new PortalLightRefresher(lightManager.get());
            if (cellStore)
            {
                const float maxDistSq = maxDist * maxDist;
                for (const auto& ref : cellStore->getReadOnlyLights().mList)
                {
                    if (!ref.mBase) continue;
                    if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef)) continue;
                    if (!ref.mData.isEnabled()) continue;
                    const ESM::Position& pos = ref.mRef.getPosition();
                    const float dx = pos.pos[0] - destCenter.x();
                    const float dy = pos.pos[1] - destCenter.y();
                    if (dx * dx + dy * dy > maxDistSq) continue;

                    osg::ref_ptr<SceneUtil::LightSource> ls
                        = SceneUtil::createLightSource(SceneUtil::LightCommon(*ref.mBase), Mask_Lighting, false);
                    osg::ref_ptr<osg::PositionAttitudeTransform> pat = new osg::PositionAttitudeTransform;
                    pat->setPosition(pos.asVec3());
                    pat->addChild(ls);
                    lightManager->addChild(pat);  // keeps LightSource alive via scene graph
                    refresher->add(ls, pos.asVec3());
                }
            }
            refresher->setStatics(statics.get());
            // addCullCallback prepends: refresher fires first, then chains to LightManagerCullCallback.
            lightManager->addCullCallback(refresher);

            // Allow alpha blending on transparent geometry (particles, glass, etc.) in the statics.
            // FORCE_OPAQUE=1 on the lightManager forces gl_FragData[0].a=1 which makes srcAlpha=1
            // in the blend formula, rendering particle quads as opaque rectangles. Statics override
            // it to 0 so NiAlphaProperty blend states work correctly. Opaque geometry is unaffected
            // since its native alpha is already 1.
            statics->getOrCreateStateSet()->setDefine("FORCE_OPAQUE", "0", osg::StateAttribute::ON);

            lightManager->addChild(statics);

            if (isExterior && exteriorTerrainNode)
                lightManager->addChild(exteriorTerrainNode);

            // Water is NOT added here — caller creates reflection/refraction RTTs first,
            // then calls createPortalWaterNode() and adds it to the returned lightManager.
            float waterHeight = 0.f;
            if (cellStore->getCell()->hasWater())
                waterHeight = cellStore->getCell()->getWaterHeight();

            return PortalSceneResult{ lightManager, light, lightModel, sceneFogOut, waterHeight };
        }

    }

    PortalManager::PortalManager(Resource::ResourceSystem* resourceSystem, osg::Group* rttParent)
        : mResourceSystem(resourceSystem)
        , mRttParent(rttParent)
    {
    }

    PortalManager::~PortalManager() = default;

    void PortalManager::loadModelList(const std::filesystem::path& userDataPath)
    {
        const std::filesystem::path filePath = userDataPath / "portal_models.txt";
        std::ifstream file(filePath);
        if (file.is_open())
        {
            Log(Debug::Info) << "Portal: loading model list from " << filePath;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                Misc::StringUtils::lowerCaseInPlace(line);
                if (!line.empty())
                    mPortalModels.insert(std::move(line));
            }
            Log(Debug::Info) << "Portal: loaded " << mPortalModels.size() << " model(s)";
        }
        else
        {
            mPortalModels = std::unordered_set<std::string>(
                sDefaultPortalModels.begin(), sDefaultPortalModels.end());
            std::ofstream out(filePath);
            if (out.is_open())
            {
                for (const auto& name : sDefaultPortalModels)
                    out << name << '\n';
                Log(Debug::Info) << "Portal: wrote default model list to " << filePath;
            }
            else
                Log(Debug::Warning) << "Portal: could not write default model list to " << filePath;
        }
    }

    void PortalManager::setExteriorLighting(const osg::Vec4f& ambient, const osg::Vec4f& diffuse, const osg::Vec3f& sunDir)
    {
        mExteriorAmbient = ambient;
        mExteriorDiffuse = diffuse;
        mExteriorSunDir  = sunDir;
    }

    // --------------------------------------------------------------------------

    bool PortalManager::isPortalDoor(const MWWorld::Ptr& door) const
    {
        if (!door.getCellRef().getTeleport())
            return false;
        if (door.getCellRef().isLocked())
            return false;
        const auto* base = door.get<ESM::Door>()->mBase;
        if (!base)
            return false;
        std::string model = base->mModel;
        Misc::StringUtils::lowerCaseInPlace(model);
        const auto slash = model.find_last_of("/\\");
        const std::string_view filename = (slash != std::string::npos)
            ? std::string_view(model).substr(slash + 1)
            : std::string_view(model);
        return mPortalModels.count(std::string(filename)) > 0;
    }

    osg::Vec2f PortalManager::computeHalfExtents(
        const MWWorld::Ptr& door, osg::Vec3f& outCenter, osg::Quat& inOutNifRot) const
    {
        const osg::Vec2f fallback(96.f, 128.f);
        outCenter = osg::Vec3f();

        VFS::Path::Normalized modelPath(door.getClass().getCorrectedModel(door));
        if (modelPath.empty())
            return fallback;

        try
        {
            osg::ref_ptr<const osg::Node> node = mResourceSystem->getSceneManager()->getTemplate(modelPath);
            if (!node)
                return fallback;

            osg::ComputeBoundsVisitor cv;
            cv.setTraversalMask(~(Mask_ParticleSystem | Mask_Effect));
            const_cast<osg::Node*>(node.get())->accept(cv);
            const osg::BoundingBox& bb = cv.getBoundingBox();
            if (!bb.valid())
                return fallback;

            const float xSpan = std::abs(bb.xMax() - bb.xMin());
            const float ySpan = std::abs(bb.yMax() - bb.yMin());
            const float zSpan = std::abs(bb.zMax() - bb.zMin());
            const osg::Vec3f c = bb.center();
            // Use the BBox center for height (Z) centering. Only use the X center if it is
            // large relative to the opening width — that indicates the model origin sits at a
            // corner rather than at the geometric centre (e.g. in_c_door_wood_square).
            const float halfWidth = std::max(xSpan, ySpan) * 0.5f;
            // Sub-node -90° Z rotation: geometry runs along Y in model space (ySpan >> xSpan).
            // Only correct when the root node reported no rotation of its own; if getNifRootQuat
            // already returned a non-identity quaternion, the root accounts for the orientation
            // and applying a second -90° Z would double-correct to -180°.
            if (ySpan > xSpan && std::abs(inOutNifRot.w()) > 0.999f)
                inOutNifRot = inOutNifRot * osg::Quat(osg::DegreesToRadians(-90.f), osg::Vec3f(0.f, 0.f, 1.f));
            // buildQuadNode uses rotate-then-translate (OSG row-vector convention), so localOffset
            // becomes the quad center directly in the parent's local space.  Use all three BBox
            // center components, filtering out values that are small relative to the opening width
            // to avoid shifting the quad in directions that should be near-zero.
            const float cx = (std::abs(c.x()) > halfWidth * 0.3f) ? c.x() : 0.f;
            const float cy = (std::abs(c.y()) > halfWidth * 0.3f) ? c.y() : 0.f;
            outCenter = osg::Vec3f(cx, cy, c.z());
            const float colonySizeMult = (modelPath.value().find("ex_colony") != std::string::npos) ? 1.1f : 1.0f;
            return osg::Vec2f(halfWidth * colonySizeMult, zSpan * 0.5f * colonySizeMult);
        }
        catch (...)
        {
            return fallback;
        }
    }

    osg::ref_ptr<osg::MatrixTransform> PortalManager::buildQuadNode(
        const osg::Vec2f& halfExtents, const osg::Quat& nifRootQuat, const osg::Vec3f& localOffset) const
    {
        const float w = halfExtents.x();
        const float h = halfExtents.y();

        // Quad lies in the XZ plane in NIF-root-local space; normal faces -Y (toward the outside).
        // The PAT parent will apply the full CellRef rotation — we only bake in the NIF root orientation.
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(4);
        (*vertices)[0] = osg::Vec3f(-w, 0.f, -h);
        (*vertices)[1] = osg::Vec3f( w, 0.f, -h);
        (*vertices)[2] = osg::Vec3f( w, 0.f,  h);
        (*vertices)[3] = osg::Vec3f(-w, 0.f,  h);

        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(1);
        (*normals)[0] = osg::Vec3f(0.f, -1.f, 0.f);

        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array(1);
        (*colors)[0] = osg::Vec4f(1.f, 1.f, 1.f, 1.f);

        // UVs: (0,0) bottom-left to (1,1) top-right, matching vertex order.
        osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array(4);
        (*texcoords)[0] = osg::Vec2f(0.f, 0.f);
        (*texcoords)[1] = osg::Vec2f(1.f, 0.f);
        (*texcoords)[2] = osg::Vec2f(1.f, 1.f);
        (*texcoords)[3] = osg::Vec2f(0.f, 1.f);

        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setVertexArray(vertices);
        geom->setNormalArray(normals, osg::Array::BIND_OVERALL);
        geom->setColorArray(colors, osg::Array::BIND_OVERALL);
        geom->setTexCoordArray(0, texcoords);
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);

        osg::ref_ptr<osg::StateSet> ss = geom->getOrCreateStateSet();
        ss->setMode(GL_CULL_FACE, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
        // Reverse-Z aware depth write.
        ss->setAttributeAndModes(new SceneUtil::AutoDepth(osg::Depth::LEQUAL, 0.0, 1.0, true));
        // Bias the portal quad slightly toward the camera so it wins depth tests against
        // coplanar wall geometry (e.g. in_c_wall_plain) placed at the portal opening.
        // Sign convention mirrors nifloader.cpp handleDecal: positive in reverse-Z, negative in standard.
        {
            const float sign = SceneUtil::AutoDepth::isReversed() ? 1.f : -1.f;
            osg::ref_ptr<osg::PolygonOffset> po = new osg::PolygonOffset(sign * 0.65f, sign * 1.f);
            ss->setAttributeAndModes(po, osg::StateAttribute::ON);
        }
        ss->setRenderBinToInherit();

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(geom);

        osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
        mt->setMatrix(osg::Matrix::rotate(nifRootQuat) * osg::Matrix::translate(localOffset));
        mt->addChild(geode);
        mt->setNodeMask(Mask_PortalQuad);

        // Default mask: full quad visible. Override per model in tryCreatePortal.
        osg::ref_ptr<osg::StateSet> mss = mt->getOrCreateStateSet();
        mss->addUniform(new osg::Uniform("portalMaskType", 0));
        mss->addUniform(new osg::Uniform("portalAspect",   1.0f));

        return mt;
    }

    // --------------------------------------------------------------------------

    bool PortalManager::tryCreatePortal(const MWWorld::Ptr& door)
    {
        if (!isPortalDoor(door))
            return false;

        if (door.getCellRef().isLocked())
            return false;

        {
            const ESM::RefId& destId = door.getCellRef().getDestCell();
            const MWWorld::CellStore* destStore
                = MWBase::Environment::get().getWorld()->findCellStore(destId);
            if (!destStore)
                return false;
        }

        osg::Group* baseNode = door.getRefData().getBaseNode();
        if (!baseNode)
            return false;

        // Load the NIF to get the accumulated rotation from root to first geometry.
        osg::Quat nifRootQuat;
        VFS::Path::Normalized modelPath(door.getClass().getCorrectedModel(door));
        if (!modelPath.empty())
        {
            try
            {
                osg::ref_ptr<const osg::Node> nifNode
                    = mResourceSystem->getSceneManager()->getTemplate(modelPath);
                if (nifNode)
                    nifRootQuat = getNifRootQuat(nifNode.get());
            }
            catch (...) {}
        }

        osg::Vec3f modelCenter;
        const osg::Quat nifRootQuatBeforeHeuristic = nifRootQuat;
        osg::Vec2f halfExtents = computeHalfExtents(door, modelCenter, nifRootQuat);

        {
            osg::Quat::value_type angle0; osg::Vec3d axis0;
            nifRootQuatBeforeHeuristic.getRotate(angle0, axis0);
            osg::Quat::value_type angle; osg::Vec3d axis;
            nifRootQuat.getRotate(angle, axis);
            const auto& pos = door.getCellRef().getPosition();
            Log(Debug::Info) << "Portal door model=" << modelPath
                << " cell=" << (door.getCell() ? door.getCell()->getCell()->getDisplayName() : "?")
                << " → dest=" << door.getCellRef().getDestCell().toDebugString()
                << " cellRefRotZ=" << osg::RadiansToDegrees(pos.rot[2])
                << " halfExt=(" << halfExtents.x() << "," << halfExtents.y() << ")"
                << " center=(" << modelCenter.x() << "," << modelCenter.z() << ")"
                << " nifRootBefore=(" << axis0 << " " << osg::RadiansToDegrees(angle0) << "deg)"
                << " nifRootAfter=(" << axis << " " << osg::RadiansToDegrees(angle) << "deg)";
        }

        // modelCenter is already the filtered (cx, 0, cz) offset from computeHalfExtents.
        const osg::Quat cellRefRot = Misc::Convert::makeOsgQuat(door.getCellRef().getPosition());
        osg::Vec3f localOffset = modelCenter;
        {
            std::string model = door.get<ESM::Door>()->mBase->mModel;
            Misc::StringUtils::lowerCaseInPlace(model);
            if (model.find("ex_nord_door_01") != std::string::npos)
                localOffset = osg::Vec3f(10.f, 13.f, -15.f);  // manually tuned hinge offset
            else if (model.find("ex_common_door_01") != std::string::npos)
            {
                // Exterior cells: snap portal depth to nearby ex_common_entrance_02 frame origin.
                // Door placement depth varies between level designers; the frame is the stable
                // reference. Project frame→door vector onto the door's depth axis (Y in cellRef space).
                if (door.getCell() && door.getCell()->isExterior())
                {
                    const ESM::Position& cpos = door.getCellRef().getPosition();
                    const osg::Vec3f doorWorldPos(cpos.pos[0], cpos.pos[1], cpos.pos[2]);
                    door.getCell()->forEachType<ESM::Static>([&](MWWorld::Ptr ref) -> bool {
                        std::string sm = ref.get<ESM::Static>()->mBase->mModel;
                        Misc::StringUtils::lowerCaseInPlace(sm);
                        if (sm.find("ex_common_entrance_02") == std::string::npos)
                            return true;
                        const ESM::Position& fp = ref.getCellRef().getPosition();
                        const osg::Vec3f frameWorldPos(fp.pos[0], fp.pos[1], fp.pos[2]);
                        if ((frameWorldPos - doorWorldPos).length2() > 200.f * 200.f)
                            return true;
                        localOffset.y() = (cellRefRot.inverse() * (frameWorldPos - doorWorldPos)).y() + 10.f;
                        return false;
                    });
                }

            }
        }
        osg::ref_ptr<osg::MatrixTransform> quadNode = buildQuadNode(halfExtents, nifRootQuat, localOffset);
        // Clear any previously loaded door mesh children (happens when a locked door is unlocked
        // at runtime: Objects loaded the normal mesh, now we replace it with the portal quad).
        baseNode->removeChildren(0, baseNode->getNumChildren());
        baseNode->addChild(quadNode);

        // World-space portal plane normal = CellRef rotation * NIF root rotation * (-Y).
        // setNodeRotation() will apply CellRef rotation to the PAT after this call returns.
        const osg::Quat fullRot = cellRefRot * nifRootQuat;
        const osg::Vec3f normal = fullRot * osg::Vec3f(0.f, -1.f, 0.f);
        const osg::Vec3f pos = door.getRefData().getPosition().asVec3();

        const osg::Vec3f playerPos
            = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3();
        const float initialDist = (playerPos - pos) * normal;

        Portal portal;
        portal.door        = door;
        portal.quadNode    = quadNode;
        portal.quadCenter  = pos + cellRefRot * localOffset;
        portal.planePoint  = portal.quadCenter;
        portal.planeNormal = normal;
        portal.invRot      = fullRot.inverse();
        portal.halfExtents = halfExtents;
        portal.lastSide    = (initialDist >= 0.f);
        // tryCreatePortal runs during loadCell, before changePlayerCell sets the arrival position.
        // lastSide is therefore initialized from the old cell's player position and may be wrong.
        // The cooldown lets the player arrive so eyePos-based lastSide can stabilize before the
        // trigger is armed. 30 frames ≈ 0.5 s at 60 fps is enough for the cell transition to settle.
        portal.cooldown    = 2;

        // Destination door lookup (cheap: in-memory iteration, no RTT construction).
        // destIsExterior and destDoorPos/Rot are needed by the streaming system and update().
        // reverseArrival: where the player arrives in THIS cell when walking back through the
        // dest portal. Same coordinate frame as pos — used below to test planeNormal direction.
        osg::Vec3f reverseArrival = pos; // fallback: no flip if dest door not found
        {
            const ESM::RefId destCellId = door.getCellRef().getDestCell();
            portal.destCellId = destCellId;
            const ESM::Position destPos = door.getCellRef().getDoorDest();
            portal.destPos   = destPos;
            portal.destPoint = destPos.asVec3();
            portal.destRot   = Misc::Convert::makeOsgQuat(destPos);
            portal.destDoorPos = portal.destPoint;
            portal.destDoorRot = portal.destRot;
            try
            {
                MWWorld::CellStore* destCellStore
                    = MWBase::Environment::get().getWorld()->findCellStore(destCellId);
                portal.destIsExterior      = destCellStore && destCellStore->getCell()->isExterior();
                portal.destIsQuasiExterior = destCellStore && !portal.destIsExterior
                                             && destCellStore->getCell()->isQuasiExterior();
                if (destCellStore)
                {
                    const ESM::RefId sourceCellId = door.getCell()->getCell()->getId();
                    float bestDistSq = std::numeric_limits<float>::max();
                    for (const auto& ref : destCellStore->getReadOnlyDoors().mList)
                    {
                        if (!ref.mBase || !ref.mRef.getTeleport())
                            continue;
                        if (ref.mRef.getDestCell() != sourceCellId)
                            continue;
                        const ESM::Position& dPos = ref.mRef.getPosition();
                        const float distSq = (dPos.asVec3() - portal.destPoint).length2();
                        if (distSq >= bestDistSq)
                            continue;
                        bestDistSq = distSq;
                        reverseArrival = ref.mRef.getDoorDest().asVec3();
                        osg::Quat destCellRefRot = Misc::Convert::makeOsgQuat(dPos);
                        osg::Quat destNifRootQuat;
                        osg::Vec3f destModelOffset;
                        if (!ref.mBase->mModel.empty())
                        {
                            try
                            {
                                VFS::Path::Normalized destModelPath = Misc::ResourceHelpers::correctMeshPath(
                                    VFS::Path::Normalized(ref.mBase->mModel));
                                osg::ref_ptr<const osg::Node> destNode
                                    = mResourceSystem->getSceneManager()->getTemplate(destModelPath);
                                if (destNode)
                                {
                                    destNifRootQuat = getNifRootQuat(destNode.get());
                                    osg::ComputeBoundsVisitor cv;
                                    cv.setTraversalMask(~(Mask_ParticleSystem | Mask_Effect));
                                    const_cast<osg::Node*>(destNode.get())->accept(cv);
                                    const osg::BoundingBox& bb = cv.getBoundingBox();
                                    if (bb.valid())
                                    {
                                        const float xSpan = std::abs(bb.xMax() - bb.xMin());
                                        const float ySpan = std::abs(bb.yMax() - bb.yMin());
                                        const float halfWidth = std::max(xSpan, ySpan) * 0.5f;
                                        const osg::Vec3f c = bb.center();
                                        if (ySpan > xSpan && std::abs(destNifRootQuat.w()) > 0.999f)
                                            destNifRootQuat = destNifRootQuat
                                                * osg::Quat(osg::DegreesToRadians(-90.f), osg::Vec3f(0.f, 0.f, 1.f));
                                        const float cx = (std::abs(c.x()) > halfWidth * 0.3f) ? c.x() : 0.f;
                                        const float cy = (std::abs(c.y()) > halfWidth * 0.3f) ? c.y() : 0.f;
                                        destModelOffset = osg::Vec3f(cx, cy, c.z());
                                    }
                                }
                            }
                            catch (...) {}
                        }
                        portal.destDoorPos = dPos.asVec3() + destCellRefRot * destModelOffset;
                        portal.destDoorRot = destCellRefRot * destNifRootQuat;
                        // If destFwd points AWAY from the teleport arrival point, the clip plane
                        // would cut all destination content — flip by R_z(+180°) to correct.
                        // No |w| guard here: flat-door NIFs (identity root, no heuristic) also
                        // need this correction when cellRefRotZ places destFwd antiparallel to arrival.
                        {
                            const osg::Vec3f destFwd = portal.destDoorRot * osg::Vec3f(0.f, -1.f, 0.f);
                            // Use dPos (raw door spawn, no model-center offset) so the direction
                            // into the destination cell is reliable regardless of NIF geometry.
                            const osg::Vec3f toArrival = portal.destPoint - dPos.asVec3();
                            if (toArrival.length2() > 1.f && destFwd * toArrival < 0.f)
                            {
                                destNifRootQuat = destNifRootQuat * osg::Quat(osg::PI, osg::Vec3f(0.f, 0.f, 1.f));
                                portal.destDoorRot = destCellRefRot * destNifRootQuat;
                                Log(Debug::Info) << "Portal dest door flip applied: destFwd was antiparallel to arrival vector";
                            }
                        }
                        {
                            osg::Quat::value_type ang; osg::Vec3d ax;
                            portal.destDoorRot.getRotate(ang, ax);
                            const osg::Vec3f fwd = portal.destDoorRot * osg::Vec3f(0.f,-1.f,0.f);
                            Log(Debug::Info) << "Portal dest door model=" << ref.mBase->mModel
                                << " destCellRefRotZ=" << osg::RadiansToDegrees(dPos.rot[2])
                                << " destDoorRot=(" << ax << " " << osg::RadiansToDegrees(ang) << "deg)"
                                << " destFwd=(" << fwd << ")"
                                << " destDoorPos=(" << portal.destDoorPos << ")";
                        }
                    }
                }
            }
            catch (...) {}
        }

        // planeNormal must face toward the approaching player in the source cell.
        // reverseArrival is where the player appears in this cell when walking back through the
        // dest portal — same coordinate frame as pos — giving a reliable approach direction.
        // Flip when planeNormal points AWAY from the player (dot with toSourcePlayer < 0).
        // This replaces the old destFwd-based condition, which could not distinguish buildings
        // where both doors happen to face the same world direction (e.g. Council Club vs Aurelia).
        {
            const osg::Vec3f toSourcePlayer = reverseArrival - pos;
            if (toSourcePlayer.length2() > 1.f && portal.planeNormal * toSourcePlayer < 0.f)
            {
                nifRootQuat = nifRootQuat * osg::Quat(osg::PI, osg::Vec3f(0.f, 0.f, 1.f));
                const osg::Quat fullRotFixed = cellRefRot * nifRootQuat;
                portal.planeNormal = fullRotFixed * osg::Vec3f(0.f, -1.f, 0.f);
                portal.invRot      = fullRotFixed.inverse();
                baseNode->removeChild(quadNode);
                quadNode = buildQuadNode(halfExtents, nifRootQuat, localOffset);
                baseNode->addChild(quadNode);
                portal.quadNode = quadNode;
                Log(Debug::Info) << "Portal plane flip applied for cellRefRotZ="
                    << osg::RadiansToDegrees(door.getCellRef().getPosition().rot[2])
                    << " new planeNormal=(" << portal.planeNormal << ")";
            }
        }

        // Interior→Exterior: source cell may contain flat wall geometry (e.g. in_c_wall_plain)
        // placed a few units in front of the portal door. PolygonOffset handles the coplanar
        // case but cannot overcome a geometric depth difference; shift the quad toward the
        // interior player so it wins the depth test against such walls.
        // Exterior→Interior is handled by the RTT clip-plane bias in setupPortalRTT.
        if (portal.destIsExterior)
        {
            constexpr float kWallClearance = 10.f;
            // In NIF-root-local space the portal normal is always -Y, so displacing by
            // nifRootQuat*(0,-k,0) == k units toward the player after cellRefRot is applied.
            const osg::Vec3f pushLocal = nifRootQuat * osg::Vec3f(0.f, -kWallClearance, 0.f);
            osg::Matrix m = quadNode->getMatrix();
            m.setTrans(m.getTrans() + osg::Vec3d(pushLocal));
            quadNode->setMatrix(m);
            // Keep quadCenter and planePoint in sync with the pushed quad position.
            // The RTT camera uses quadCenter as reference; without this update it is 10 units
            // off, which shifts the virtual camera into the destination and causes the clip
            // plane to appear as a gap at the portal edge when viewed from inside.
            const osg::Vec3f pushWorld = portal.planeNormal * kWallClearance;
            portal.quadCenter += pushWorld;
            portal.planePoint += pushWorld;
        }

        // Per-model overrides: shape mask and collision behaviour.
        {
            std::string model = door.get<ESM::Door>()->mBase->mModel;
            Misc::StringUtils::lowerCaseInPlace(model);

            // Telvanni organic doors: no approach-zone ghost mode (no flat floor/wall geometry
            // to stand on, and the opening shape doesn't match the rectangular physics box).
            if (model.find("ex_t_") != std::string::npos || model.find("in_t_") != std::string::npos)
                portal.noCollision = true;


            // Per-door RTT clip plane bias: pushes the clip plane into the destination cell
            // to clear flush wall/frame geometry that would otherwise block the portal view.
            if (model.find("in_hlaalu_loaddoor_01") != std::string::npos
                  || model.find("in_hlaalu_door") != std::string::npos
                  || model.find("hlaalu_loaddoor") != std::string::npos)
                portal.clipBias = -10.0f;


            if (model.find("ex_imp_loaddoor_02") != std::string::npos
             || model.find("in_impsmall_loaddoor_01") != std::string::npos
             || model.find("ex_redoran_hut_01_a") != std::string::npos
             || model.find("ex_s_door_rounded") != std::string::npos
             || model.find("in_s_doorjam_rounded") != std::string::npos
             || model.find("ex_mh_door_02") != std::string::npos
             || model.find("ex_mh_door_01") != std::string::npos
			 || model.find("ex_vivec_grate_01") != std::string::npos)	
            {
                osg::ref_ptr<osg::StateSet> mss = quadNode->getOrCreateStateSet();
                mss->getUniform("portalMaskType")->set(1);
                mss->getUniform("portalAspect")->set(halfExtents.y() / halfExtents.x());
            }
            else if (model.find("ex_t_door_01") != std::string::npos
                  || model.find("ex_t_door_02") != std::string::npos
                  || model.find("in_t_housepod_door_exit") != std::string::npos
                  || model.find("in_t_s_plain_door") != std::string::npos)
            {
                osg::ref_ptr<osg::StateSet> mss = quadNode->getOrCreateStateSet();
                mss->getUniform("portalMaskType")->set(2);
                mss->getUniform("portalAspect")->set(halfExtents.y() / halfExtents.x());
            }
            else if (model.find("ex_v_cantondoor_01") != std::string::npos)
            {
                osg::ref_ptr<osg::StateSet> mss = quadNode->getOrCreateStateSet();
                mss->getUniform("portalMaskType")->set(3);
                mss->getUniform("portalAspect")->set(halfExtents.y() / halfExtents.x());
            }
			else if (model.find("in_velothismall_ndoor_01") != std::string::npos)
			{
                osg::ref_ptr<osg::StateSet> mss = quadNode->getOrCreateStateSet();
                mss->getUniform("portalMaskType")->set(4);
                mss->getUniform("portalAspect")->set(halfExtents.y() / halfExtents.x());

			}
        }

        // Decorative meshes placed alongside specific portal quads.
        {
            std::string model = door.get<ESM::Door>()->mBase->mModel;
            Misc::StringUtils::lowerCaseInPlace(model);
            const char* decorNif = nullptr;
            bool decorUsesLocalOffset = false;
            if (model.find("in_hlaalu_loaddoor_01") != std::string::npos)
            {
                decorNif = "i/portal_in_hlaalu_loaddoor_01.nif";
                decorUsesLocalOffset = true;  // symmetric arch, localOffset ≈ 0 but correct to include
            }
            else if (model.find("in_hlaalu_door") != std::string::npos)
            {
                decorNif = "i/portal_in_hlaalu_door.nif";
                decorUsesLocalOffset = false; // NIF origin matches door hinge, geometry already offset
            }
			else if (model.find("in_t_housepod_door_exit.nif") != std::string::npos)
			{
				decorNif = "x/ex_t_doorway_02.nif";
				decorUsesLocalOffset = true;
			}
			else if (model.find("ex_mh_temple_door_01") != std::string::npos)
			{
				decorNif = "x/portal_ex_mh_temple_door_01.nif";
				decorUsesLocalOffset = true; 
			}
            if (decorNif)
            {
                const VFS::Path::Normalized decorPath = Misc::ResourceHelpers::correctMeshPath(
                    VFS::Path::Normalized(decorNif));
                if (mResourceSystem->getVFS()->exists(decorPath))
                {
                    try
                    {
                        osg::ref_ptr<osg::Node> decorNode
                            = mResourceSystem->getSceneManager()->getInstance(decorPath);
                        const osg::Vec3f decorShift = nifRootQuat * osg::Vec3f(0.f, -1.f, 0.f) * 6.5f;
                        const osg::Vec3f decorPos = (decorUsesLocalOffset ? localOffset : osg::Vec3f()) + decorShift;
                        osg::ref_ptr<osg::MatrixTransform> decorXform = new osg::MatrixTransform(
                            osg::Matrix::translate(osg::Vec3d(decorPos)));
                        decorXform->addChild(decorNode);
                        baseNode->addChild(decorXform);
                        portal.decorMesh = decorXform;
                    }
                    catch (const std::exception& e)
                    {
                        Log(Debug::Warning) << "Portal: failed to load decor mesh " << decorPath << ": " << e.what();
                    }
                }
            }
        }

        // screenRes is always needed (portal shader reads it even before RTT is active).
        {
            const auto screenW = static_cast<uint32_t>(Settings::video().mResolutionX);
            const auto screenH = static_cast<uint32_t>(Settings::video().mResolutionY);
            quadNode->getOrCreateStateSet()->addUniform(
                new osg::Uniform("screenRes", osg::Vec2f(float(screenW), float(screenH))));
        }
        // Quad is invisible until the streaming system activates the RTT (setupPortalRTT restores the mask).
        quadNode->setNodeMask(0);

        mPortals.push_back(std::move(portal));

        // Activate RTT immediately if the player is already within streaming range.
        if ((playerPos - mPortals.back().planePoint).length2() <= kStreamRange * kStreamRange)
            setupPortalRTT(mPortals.back());

        return true;
    }

    bool PortalManager::destroyPortal(const MWWorld::Ptr& door)
    {
        auto it = std::find_if(mPortals.begin(), mPortals.end(),
            [&door](const Portal& p) { return p.door.mRef == door.mRef; });
        if (it != mPortals.end())
        {
            // Do NOT call physics cleanup here — destroyPortal runs during cell unloading
            // while physics state is in flux; calling setCollisionFilterMask then crashes.
            // mGhostModeActive stays true so update() cleans up on the next stable frame.
            teardownPortalRTT(*it);
            // Explicitly detach quadNode from the scene graph. Copy the parent list first —
            // removeChild modifies getParents() in-place, invalidating iteration.
            if (it->quadNode)
            {
                osg::Node::ParentList parents = it->quadNode->getParents();
                for (auto* parent : parents)
                    parent->removeChild(it->quadNode);
            }
            if (it->decorMesh)
            {
                osg::Node::ParentList parents = it->decorMesh->getParents();
                for (auto* parent : parents)
                    parent->removeChild(it->decorMesh);
            }
            mPortals.erase(it);
            return true;
        }
        return false;
    }

    void PortalManager::setupPortalRTT(Portal& portal)
    {
        try
        {
            MWWorld::CellStore* destCellStore
                = MWBase::Environment::get().getWorld()->findCellStore(portal.destCellId);

            const auto screenW = static_cast<uint32_t>(Settings::video().mResolutionX);
            const auto screenH = static_cast<uint32_t>(Settings::video().mResolutionY);

            // Quasi-exterior cells (Mournhold/Tribunal) can have large architecture meshes
            // whose ESM spawn position lies far from the portal door — use a wider search radius.
            const float sceneMaxDist = portal.destIsQuasiExterior ? 20000.f : 5000.f;
            PortalSceneResult sceneResult = buildPortalScene(
                destCellStore, portal.destDoorPos, mResourceSystem, mExteriorTerrainNode,
                mSkyManager, osg::Vec2f(float(screenW), float(screenH)),
                mExteriorAmbient, mExteriorDiffuse, mExteriorSunDir, sceneMaxDist, mPortalModels, mExteriorSkyColor);


            const bool hasWater = destCellStore && destCellStore->getCell()->hasWater();

            if (hasWater && mRttParent)
            {
                auto* scene = sceneResult.scene.get();
                portal.reflectionRTTNode = new PortalReflectionRTTNode(
                    scene, sceneResult.waterHeight, screenW, screenH, shouldAddMSAAIntermediateTarget());
                portal.reflectionRTTNode->setNodeMask(Mask_RenderToTexture);
                mRttParent->addChild(portal.reflectionRTTNode);

                portal.refractionRTTNode = new PortalRefractionRTTNode(
                    scene, sceneResult.waterHeight, screenW, screenH, shouldAddMSAAIntermediateTarget());
                portal.refractionRTTNode->setNodeMask(Mask_RenderToTexture);
                mRttParent->addChild(portal.refractionRTTNode);
            }

            if (hasWater)
            {
                osg::ref_ptr<osg::Texture2D> waterSkyTex;
                Shader::ShaderManager& waterShaderMgr = mResourceSystem->getSceneManager()->getShaderManager();
                osg::ref_ptr<osg::PositionAttitudeTransform> wn = createPortalWaterNode(
                    sceneResult.waterHeight, mResourceSystem, waterShaderMgr,
                    mExteriorSkyColor, mNearClip, waterSkyTex,
                    portal.reflectionRTTNode.get(), portal.refractionRTTNode.get());
                portal.waterSkyTex = waterSkyTex;
                portal.waterHeight = sceneResult.waterHeight;
                sceneResult.scene->addChild(wn);
            }

            portal.sunLight       = sceneResult.sunLight;
            portal.lightModelAttr = sceneResult.lightModelAttr;
            portal.sceneFog       = sceneResult.sceneFog;

            osg::ref_ptr<osg::Group> skyScene;
            if ((portal.destIsExterior || portal.destIsQuasiExterior) && mSkyManager)
            {
                osg::ref_ptr<CameraRelativeTransform> skyWrapper = new CameraRelativeTransform;
                skyWrapper->setNodeMask(Mask_Sky);
                mSkyManager->populatePortalSkyGroup(skyWrapper.get());
                if (skyWrapper->getNumChildren() > 0)
                    skyScene = skyWrapper;
            }

            portal.rttNode = new PortalRTTNode(sceneResult.scene.get(), skyScene.get(), screenW, screenH, shouldAddMSAAIntermediateTarget());
            if (portal.destIsExterior || portal.destIsQuasiExterior)
                portal.rttNode->setClearColor(mExteriorSkyColor);
            {
                const osg::Vec3f destFwd = portal.destDoorRot * osg::Vec3f(0.f, -1.f, 0.f);

                // Auto-detect blocking geometry near the portal opening. Cast a centre ray
                // from just inside the destination door into the room; if it hits something
                // within kSearchDist units the portal entrance is obstructed and the clip
                // plane is pushed past the obstacle. Hits within kIgnoreNear are assumed to
                // be the door frame mesh itself and are skipped.
                if (!portal.destIsQuasiExterior)
                {
                    constexpr float kEpsilon    = 1.f;   // avoid fp issues at the clip plane
                    constexpr float kSearchDist = 100.f;
                    constexpr float kMargin     = 5.f;
                    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector
                        = new osgUtil::LineSegmentIntersector(
                            osg::Vec3d(portal.destDoorPos + destFwd * kEpsilon),
                            osg::Vec3d(portal.destDoorPos + destFwd * (kEpsilon + kSearchDist)));
                    osgUtil::IntersectionVisitor iv(intersector);
                    sceneResult.scene->accept(iv);
                    if (intersector->containsIntersections())
                    {
                        const osg::Vec3f hit(intersector->getFirstIntersection().getWorldIntersectPoint());
                        const float dist = (hit - portal.destDoorPos) * destFwd;
                        portal.clipBias = std::max(portal.clipBias, dist + kMargin);
                    }
                }

                portal.rttNode->setClipPlaneBoundary(destFwd, portal.destDoorPos + destFwd * portal.clipBias);
            }
            // Quasi-exterior cells (Mournhold) are self-contained — only their own statics are
            // loaded, so there is no outside geometry to accidentally reveal. The clip plane would
            // cut through interior geometry because destDoorPos carries a model-center offset that
            // can push the plane well into the cell, clipping floors and distant walls.
            portal.rttNode->setClipEnabled(!portal.destIsQuasiExterior);
            portal.rttNode->setNodeMask(Mask_RenderToTexture);
            if (mRttParent)
                mRttParent->addChild(portal.rttNode);

            Shader::ShaderManager& shaderMgr = mResourceSystem->getSceneManager()->getShaderManager();
            osg::ref_ptr<osg::Program> portalProgram = shaderMgr.getProgram("portal");
            osg::ref_ptr<PortalStateSetUpdater> ssUpdater
                = new PortalStateSetUpdater(portal.rttNode.get(), portalProgram.get());
            portal.quadNode->setCullCallback(ssUpdater);
            portal.quadNode->setNodeMask(Mask_PortalQuad);

            portal.portalScene = sceneResult.scene;
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "PortalManager: RTT setup failed (" << e.what() << "); portal inactive";
            if (portal.rttNode && mRttParent)
                mRttParent->removeChild(portal.rttNode);
            if (portal.reflectionRTTNode && mRttParent)
                mRttParent->removeChild(portal.reflectionRTTNode);
            if (portal.refractionRTTNode && mRttParent)
                mRttParent->removeChild(portal.refractionRTTNode);
            portal.rttNode           = nullptr;
            portal.reflectionRTTNode = nullptr;
            portal.refractionRTTNode = nullptr;
            portal.portalScene       = nullptr;
        }
    }

    void PortalManager::teardownPortalRTT(Portal& portal)
    {
        if (portal.rttNode && mRttParent)
            mRttParent->removeChild(portal.rttNode);
        if (portal.reflectionRTTNode && mRttParent)
            mRttParent->removeChild(portal.reflectionRTTNode);
        if (portal.refractionRTTNode && mRttParent)
            mRttParent->removeChild(portal.refractionRTTNode);

        // Detach the shared terrain node so the portal's LightManager releases it.
        if (portal.destIsExterior && mExteriorTerrainNode && portal.portalScene)
        {
            portal.portalScene->removeChild(mExteriorTerrainNode);
            for (unsigned int c = 0; c < portal.portalScene->getNumChildren(); ++c)
                if (auto* g = dynamic_cast<osg::Group*>(portal.portalScene->getChild(c)))
                    g->removeChild(mExteriorTerrainNode);
        }

        portal.quadNode->setCullCallback(nullptr);
        portal.quadNode->setNodeMask(0);

        portal.rttNode           = nullptr;
        portal.reflectionRTTNode = nullptr;
        portal.refractionRTTNode = nullptr;
        portal.portalScene       = nullptr;
        portal.sunLight          = nullptr;
        portal.lightModelAttr    = nullptr;
        portal.sceneFog          = nullptr;
        portal.waterSkyTex       = nullptr;

        // If ghost mode was active for this portal, clear the flag so the watchdog
        // in update() sees no active portal and disables ghost mode + removes physics objects.
        portal.approachActive = false;
    }

    // --------------------------------------------------------------------------

    bool PortalManager::isWithinBounds(const osg::Vec3f& playerPos, const Portal& portal) const
    {
        osg::Vec3f diff = playerPos - portal.planePoint;
        osg::Vec3f local = portal.invRot * diff;
        // local.x = right, local.y = depth along normal, local.z = up
        // Small margin so the player doesn't need to hit the exact edge of the opening.
        const float kMargin = 20.f;
        return std::abs(local.x()) < portal.halfExtents.x() + kMargin
            && std::abs(local.z()) < portal.halfExtents.y() + kMargin;
    }

    void PortalManager::update(const osg::Vec3f& playerPos, const osg::Matrixd& viewMatrix, const osg::Matrixd& projMatrix, bool paused)
    {
        // Watchdog: if ghost mode is active but no portal claims it (e.g. the portal was
        // destroyed during cell unloading), clean up here in a stable physics state.
        if (mGhostModeActive)
        {
            const bool anyActive = std::any_of(mPortals.begin(), mPortals.end(),
                [](const Portal& p) { return p.approachActive; });
            if (!anyActive)
            {
                mGhostModeActive = false;
                MWBase::World* world = MWBase::Environment::get().getWorld();
                world->setPlayerGhostMode(false);
                world->removePortalFloor();
                world->removePortalGuideWalls();
            }
        }

        if (paused || mPortals.empty())
            return;

        for (auto& portal : mPortals)
        {
            if ((!portal.destIsExterior && !portal.destIsQuasiExterior) || !portal.rttNode)
                continue;
            portal.rttNode->setClearColor(mExteriorSkyColor);

            // Update sky-color fallback texture for water reflection (used when RTT not ready yet).
            if (portal.waterSkyTex)
            {
                osg::Image* img = portal.waterSkyTex->getImage();
                if (img)
                {
                    constexpr float kReflectionScale = 0.4f;
                    unsigned char* d = img->data();
                    d[0] = static_cast<unsigned char>(std::clamp(mExteriorSkyColor.r() * kReflectionScale, 0.f, 1.f) * 255.f);
                    d[1] = static_cast<unsigned char>(std::clamp(mExteriorSkyColor.g() * kReflectionScale, 0.f, 1.f) * 255.f);
                    d[2] = static_cast<unsigned char>(std::clamp(mExteriorSkyColor.b() * kReflectionScale, 0.f, 1.f) * 255.f);
                    d[3] = 255;
                    img->dirty();
                }
            }

            // Update sun direction / color live from current weather.
            if (portal.sunLight)
            {
                portal.sunLight->setDiffuse(mExteriorDiffuse);
                portal.sunLight->setPosition(osg::Vec4f(mExteriorSunDir, 0.f));
            }
            if (portal.lightModelAttr)
                portal.lightModelAttr->setAmbientIntensity(mExteriorAmbient);
            if (portal.sceneFog)
            {
                portal.sceneFog->setStart(mExteriorFogStart);
                portal.sceneFog->setEnd(mExteriorFogEnd);
                portal.sceneFog->setColor(mExteriorFogColor);
            }
        }

        // Extract eye position, look, and up from the view matrix.
        // Eye position: view matrix M transforms world→eye. Camera origin = -R^T * t
        // where t = (M(3,0), M(3,1), M(3,2)) is the translation row.
        const osg::Vec3f eyePos(
            -static_cast<float>(viewMatrix(0,0)*viewMatrix(3,0) + viewMatrix(0,1)*viewMatrix(3,1) + viewMatrix(0,2)*viewMatrix(3,2)),
            -static_cast<float>(viewMatrix(1,0)*viewMatrix(3,0) + viewMatrix(1,1)*viewMatrix(3,1) + viewMatrix(1,2)*viewMatrix(3,2)),
            -static_cast<float>(viewMatrix(2,0)*viewMatrix(3,0) + viewMatrix(2,1)*viewMatrix(3,1) + viewMatrix(2,2)*viewMatrix(3,2)));
        const osg::Vec3f playerLook(
            static_cast<float>(-viewMatrix(0, 2)),
            static_cast<float>(-viewMatrix(1, 2)),
            static_cast<float>(-viewMatrix(2, 2)));
        const osg::Vec3f playerUp(
            static_cast<float>(viewMatrix(0, 1)),
            static_cast<float>(viewMatrix(1, 1)),
            static_cast<float>(viewMatrix(2, 1)));

        for (std::size_t i = 0; i < mPortals.size(); ++i)
        {
            Portal& portal = mPortals[i];

            // Streaming: activate within kStreamRange unconditionally,
            // or within kStreamRangeFar when the portal center is visible in the viewport.
            {
                const float dist2 = (playerPos - portal.planePoint).length2();
                bool shouldBeActive = dist2 <= kStreamRange * kStreamRange;
                if (!shouldBeActive && dist2 <= kStreamRangeFar * kStreamRangeFar)
                {
                    const osg::Vec4d clip = osg::Vec4d(portal.quadCenter, 1.0) * (viewMatrix * projMatrix);
                    if (clip.w() > 0.0)
                        shouldBeActive = std::abs(clip.x() / clip.w()) <= 1.0
                                      && std::abs(clip.y() / clip.w()) <= 1.0;
                }
                if (shouldBeActive && !portal.rttNode)
                    setupPortalRTT(portal);
                else if (!shouldBeActive && portal.rttNode)
                    teardownPortalRTT(portal);
            }

            // Stage 3: update RTT camera to track the player's position and look direction.
            // Maps the player's transform relative to the source portal into destination space.
            if (portal.rttNode)
            {
                // Full portal transformation quaternion: maps source portal local frame to
                // destination frame for any relative orientation (tilted, horizontal, etc.).
                // Quat(PI, Z) accounts for source and destination facing opposite directions —
                // validated to match the old per-component formula for the standard upright case.
                const osg::Quat portalRot = portal.destDoorRot
                    * osg::Quat(osg::PI, osg::Vec3f(0.f, 0.f, 1.f))
                    * portal.invRot;

                const osg::Vec3f diff      = eyePos - portal.quadCenter;
                const osg::Vec3f rawOffset = portalRot * diff;

                // Clamp depth along destFwd to prevent the virtual camera clipping through
                // the portal. Works for any portal orientation including tilted/horizontal.
                const osg::Vec3f destFwd = portal.destDoorRot * osg::Vec3f(0.f, -1.f, 0.f);
                const float rawDepth     = rawOffset * destFwd;
                const float clampedDepth = (rawDepth < 0.f)
                    ? std::min(rawDepth, -10.f)
                    : std::max(rawDepth,  10.f);
                const osg::Vec3f camPos = portal.destDoorPos + rawOffset
                    + destFwd * (clampedDepth - rawDepth + portal.clipBias);

                const osg::Vec3f rttLook = portalRot * playerLook;
                // Map player up through the portal so camera roll is physically consistent.
                // playerUp is always perpendicular to playerLook so lookAt is never degenerate.
                const osg::Vec3f rttUp   = portalRot * playerUp;
                const osg::Matrixd portalView = osg::Matrix::lookAt(
                    osg::Vec3d(camPos),
                    osg::Vec3d(camPos + rttLook * 400.f),
                    osg::Vec3d(rttUp));

                portal.rttNode->setViewMatrix(portalView);

                // Debug: log camera placement once per portal activation (cooldown starts at 2).
                if (portal.cooldown == 2)
                {
                    Log(Debug::Info) << "Portal RTT cam"
                        << " idx=" << i
                        << " dest=\"" << portal.destCellId.toDebugString() << "\""
                        << " planeNormal=(" << portal.planeNormal << ")"
                        << " destFwd=(" << destFwd << ")"
                        << " rawOffset=(" << rawOffset << ")"
                        << " camPos=(" << camPos << ")"
                        << " rttLook=(" << rttLook << ")";
                }

                // Use a fixed exterior-friendly projection: same FOV/aspect as the main camera,
                // near=1 unit (smaller than interior near to avoid clipping nearby exterior geometry),
                // infinite far (reversed-z, no far clip).
                osg::Matrixd rttProj = projMatrix;
                {
                    double fovY = 0.0, aspect = 0.0, nearP = 0.0, farP = 0.0;
                    if (projMatrix.getPerspective(fovY, aspect, nearP, farP) && fovY > 0.0 && aspect > 0.0)
                        rttProj = SceneUtil::getReversedZProjectionMatrixAsPerspectiveInf(fovY, aspect, 1.0);
                    portal.rttNode->setProjectionMatrix(rttProj);
                }

                // Propagate the same view/proj to the water reflection and refraction cameras.
                if (portal.reflectionRTTNode)
                    static_cast<PortalReflectionRTTNode*>(portal.reflectionRTTNode.get())
                        ->setMatrices(portalView, rttProj);
                if (portal.refractionRTTNode)
                    static_cast<PortalRefractionRTTNode*>(portal.refractionRTTNode.get())
                        ->setMatrices(portalView, rttProj);
            }

            if (portal.cooldown > 0)
            {
                --portal.cooldown;
                const float d = (playerPos - portal.planePoint) * portal.planeNormal;
                portal.lastSide = (d >= 0.f);
                continue;
            }

            // Only trigger crossing while the portal is visually active (RTT running).
            // An inactive portal (NodeMask = 0, no RTT) must not teleport the player silently.
            if (!portal.rttNode)
            {
                const float d = (playerPos - portal.planePoint) * portal.planeNormal;
                portal.lastSide = (d >= 0.f);
                continue;
            }

            // Use the player's body position (not the camera) for crossing detection so that
            // traversal works correctly in both first-person and third-person view.
            const float dist = (playerPos - portal.planePoint) * portal.planeNormal;
            const bool side = (dist >= 0.f);

            // --- Approach ghost mode ---
            // Strips CollisionType_World so rocks/walls don't block the path through the portal.
            // Needed for exterior→interior portals (cave entrance rocks) and for interior→exterior
            // portals where the indoor cell has no physical opening behind the door frame.
            constexpr float kApproachDist = 80.f;
            const bool inApproachZone = !portal.noCollision
                && portal.lastSide && dist >= 0.f && dist < kApproachDist
                && isWithinBounds(playerPos, portal);

            // Check whether any OTHER portal already owns the ghost-mode physics objects.
            // The floor/walls are a single shared physics resource; only one portal at a time
            // may manage them. When multiple portals are nearby (e.g. two Hlaalu buildings
            // side by side) we skip physics setup for subsequent portals so we never add/remove
            // the same objects twice in the same frame, which would crash the task scheduler.
            const bool anyOtherActive = std::any_of(mPortals.begin(), mPortals.end(),
                [&portal](const Portal& p) { return p.approachActive && (&p != &portal); });

            if (inApproachZone && !portal.approachActive)
            {
                portal.approachActive = true;
                mGhostModeActive = true;
                if (!anyOtherActive)
                {
                    MWBase::World* world = MWBase::Environment::get().getWorld();
                    world->setPlayerGhostMode(true);

                    bool hasRamp = false;
                    osg::Vec3f rampCenter;
                    osg::Quat  rampRot;
                    float halfRampWidth = 0.f, halfRampLen = 0.f;

                    // Portal rotation: maps portal-local axes to world space.
                    // Local X = width direction, local -Y = toward player, local Z = up.
                    const osg::Quat portalRot = portal.invRot.inverse();

                    const float portalSillZ = portal.planePoint.z() - portal.halfExtents.y();

                    // Flat platform at the portal sill, aligned with the portal plane.
                    // Center in portal-local space: (0, -halfLen, -(halfExtents.y + 5))
                    //   → halfLen units toward the player, 5 units below the sill.
                    {
                        hasRamp       = true;
                        halfRampWidth = portal.halfExtents.x() + 30.f;
                        halfRampLen   = 120.f;
                        rampRot       = portalRot;
                        rampCenter    = portal.planePoint
                            + portalRot * osg::Vec3f(0.f, 0.f,
                                                     -(portal.halfExtents.y() + 5.f));
                        world->addPortalFloor(rampCenter, halfRampWidth, halfRampLen, rampRot);
                    }

                    // If the portal sill is more than ~1 m above the player's feet, also add a ramp.
                    // The ramp top surface passes through playerPos, portal-bottom-left, and
                    // portal-bottom-right — the box is tilted to match that plane exactly.
                    if (portalSillZ - playerPos.z() > 50.f)
                    {
                        const osg::Vec3f sillMid(portal.planePoint.x(), portal.planePoint.y(), portalSillZ);
                        const osg::Vec3f rampVec = sillMid - playerPos;
                        const float rampLen = rampVec.length();
                        if (rampLen > 1.f)
                        {
                            const osg::Vec3f rampSlopeDir = rampVec / rampLen;

                            // Portal width axis in world space, orthogonalized against slope
                            osg::Vec3f widthDir = portal.invRot.inverse() * osg::Vec3f(1.f, 0.f, 0.f);
                            widthDir = widthDir - rampSlopeDir * (widthDir * rampSlopeDir);
                            widthDir.normalize();

                            // Ramp surface normal: rampSlopeDir × widthDir (points away from walkable side)
                            const osg::Vec3f rampUp = rampSlopeDir ^ widthDir;

                            // Box local axes: X→widthDir, Y→(-rampSlopeDir), Z→rampUp
                            // OSG row-vector convention: each row is the world target of that local axis
                            const osg::Matrix rotMat(
                                 widthDir.x(),       widthDir.y(),       widthDir.z(),      0.0,
                                -rampSlopeDir.x(),  -rampSlopeDir.y(),  -rampSlopeDir.z(),  0.0,
                                 rampUp.x(),          rampUp.y(),          rampUp.z(),       0.0,
                                 0.0,                 0.0,                 0.0,               1.0
                            );
                            rampRot.set(rotMat);
                            halfRampWidth = portal.halfExtents.x() + 80.f;
                            halfRampLen   = rampLen * 0.5f;
                            // Center: midpoint of playerPos→sillMid, offset inward by half-thickness (10)
                            rampCenter = (playerPos + sillMid) * 0.5f - rampUp * 10.f;
                            world->addPortalFloor(rampCenter, halfRampWidth, halfRampLen, rampRot);
                            hasRamp = true;
                        }
                    }

                    // Two angled guide walls funnelling the player toward the portal opening.
                    world->addPortalGuideWalls(portal.planePoint, portalRot,
                        portal.halfExtents.x(), portal.halfExtents.y());

                    // Debug: show collision shapes as magenta wireframe (settings.cfg: [Portal] debug geometry).
                    if (Settings::portal().mDebugGeometry)
                    {
                        constexpr float wallRadius = 45.f;
                        auto atFace = [&](float localX) -> osg::Vec3f {
                            return portal.planePoint + portalRot * osg::Vec3f(localX, 0.f, 0.f);
                        };
                        mDebugShapesNode = buildPortalCollisionDebug(
                            hasRamp, rampCenter, rampRot, halfRampWidth, halfRampLen,
                            atFace(-(portal.halfExtents.x() + wallRadius)),
                            atFace( (portal.halfExtents.x() + wallRadius)),
                            wallRadius, portal.halfExtents.y());
                        mRttParent->addChild(mDebugShapesNode);
                    }
                }
            }
            else if (!inApproachZone && portal.approachActive)
            {
                portal.approachActive = false;
                if (!anyOtherActive)
                {
                    mGhostModeActive = false;
                    MWBase::World* world = MWBase::Environment::get().getWorld();
                    world->setPlayerGhostMode(false);
                    world->removePortalFloor();
                    world->removePortalGuideWalls();
                    if (mDebugShapesNode)
                    {
                        mRttParent->removeChild(mDebugShapesNode);
                        mDebugShapesNode = nullptr;
                    }
                }
            }

            // Trigger only when crossing from outside (lastSide=true) to inside.
            if (portal.lastSide && !side && isWithinBounds(playerPos, portal))
            {
                // --- Rapid-crossing diagnostics ---
                const double nowMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                const double dtMs = nowMs - mLastCrossingMs;
                if (dtMs < 2000.0)
                {
                    Log(Debug::Warning) << "[Portal] Rapid crossing dt=" << (int)dtMs << "ms"
                        << "  dest=" << portal.destCellId.toDebugString()
                        << "  portal.approachActive=" << portal.approachActive
                        << "  mGhostModeActive=" << mGhostModeActive
                        << "  portal.cooldown=" << portal.cooldown
                        << "  dist=" << dist;
                    for (size_t j = 0; j < mPortals.size(); ++j)
                    {
                        const auto& p = mPortals[j];
                        const float pd = (playerPos - p.planePoint) * p.planeNormal;
                        const bool pInZone = !p.noCollision && p.lastSide
                            && pd >= 0.f && pd < kApproachDist && isWithinBounds(playerPos, p);
                        Log(Debug::Warning) << "  [p" << j << "]"
                            << " dest=" << p.destCellId.toDebugString()
                            << " approachActive=" << p.approachActive
                            << " cooldown=" << p.cooldown
                            << " dist=" << pd
                            << " inZone=" << pInZone
                            << " noCollision=" << p.noCollision;
                    }
                }
                mLastCrossingMs = nowMs;

                // Clean up ghost mode before the cell change destroys all portals.
                // Use mGhostModeActive rather than portal.approachActive: when two portals'
                // cooldowns expire in the same frame, portal A may activate ghost mode and
                // portal B may cross — portal B's approachActive is false (went from cooldown
                // directly to crossing) so the old guard left ghost mode active after the cell
                // change, causing the player to fall through the floor in the new cell.
                if (mGhostModeActive)
                {
                    Log(Debug::Warning) << "[Portal] ghost mode cleanup at crossing"
                        << " (mGhostModeActive=true, portal.approachActive=" << portal.approachActive << ")";
                    for (auto& p : mPortals)
                        p.approachActive = false;
                    mGhostModeActive = false;
                    MWBase::World* world = MWBase::Environment::get().getWorld();
                    world->setPlayerGhostMode(false);
                    world->removePortalFloor();
                    world->removePortalGuideWalls();
                    if (mDebugShapesNode)
                    {
                        mRttParent->removeChild(mDebugShapesNode);
                        mDebugShapesNode = nullptr;
                    }
                }
                // Use cached values — never re-dereference portal.door here; its CellRef
                // may be stale if a lock/unlock cycle occurred since the portal was created.
                const ESM::RefId&   destCell = portal.destCellId;

                // Preserve the player's facing angle relative to the source portal.
                // Compute yaw delta from the "straight-on" approach direction (-planeNormal)
                // and apply the same offset to the ESM arrival yaw so e.g. a player walking
                // backwards through the portal still faces the portal after teleporting.
                const float playerYaw = MWBase::Environment::get().getWorld()
                    ->getPlayerPtr().getRefData().getPosition().rot[2];
                const float srcNormalYaw = std::atan2(portal.planeNormal.x(), portal.planeNormal.y());
                const float yawDelta = playerYaw - (srcNormalYaw + static_cast<float>(osg::PI));
                ESM::Position destPos = portal.destPos;
                destPos.rot[2] += yawDelta;

                // changeToCell is synchronous; may call destroyPortal invalidating mPortals.
                MWBase::Environment::get().getWorld()->changeToCell(destCell, destPos, true, true, true);
                return;
            }

            portal.lastSide = side;
        }
    }

}
