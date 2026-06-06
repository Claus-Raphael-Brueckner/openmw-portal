#include "portalmanager.hpp"
#include "sky.hpp"
#include "skyutil.hpp"
#include "util.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <components/debug/debuglog.hpp>

#include <osg/ComputeBoundsVisitor>
#include <osg/Depth>
#include <osg/Fog>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Light>
#include <osg/LightModel>
#include <osg/LightSource>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/StateSet>
#include <osg/Texture2D>

#include <osg/NodeCallback>
#include <osgUtil/CullVisitor>
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
#include <components/vfs/pathutil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "portalrttnode.hpp"
#include "vismask.hpp"

namespace MWRender
{
    namespace
    {
        // Extract the rotation quaternion from a NIF root node (if it carries a matrix transform).
        // Returns identity if the root is a plain osg::Group (identity transform in the NIF).
        osg::Quat getNifRootQuat(const osg::Node* node)
        {
            const auto* mt = dynamic_cast<const osg::MatrixTransform*>(node);
            if (!mt)
                return osg::Quat();
            osg::Quat q;
            osg::Vec3d t, s;
            osg::Quat so;
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
                // Drive LightController (flicker/pulse) directly, bypassing CollectLightCallback.
                // TRAVERSE_NONE prevents any scene-graph modification from this path.
                osg::NodeVisitor fakeNv(osg::NodeVisitor::UPDATE_VISITOR,
                                        osg::NodeVisitor::TRAVERSE_NONE);
                fakeNv.setFrameStamp(const_cast<osg::FrameStamp*>(nv->getFrameStamp()));
                fakeNv.setTraversalNumber(nv->getTraversalNumber());
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
                    staticsNv.setTraversalNumber(nv->getTraversalNumber());
                    mStatics->accept(staticsNv);
                }

                // Clears mLights and mLightsInViewSpace, then re-adds lights with fresh
                // world matrices so getLightsInViewSpace rebuilds view-space positions
                // against the current RTT camera view matrix this frame.
                mLightManager->update(nv->getTraversalNumber());
                for (const auto& e : mEntries)
                {
                    osg::Matrixf worldMat;
                    worldMat.setTrans(e.pos);
                    mLightManager->addLight(e.ls.get(), worldMat, nv->getTraversalNumber());
                }
                traverse(node, nv);  // chains to LightManagerCullCallback
            }

        private:
            SceneUtil::LightManager*        mLightManager;
            osg::ref_ptr<osg::Group>        mStatics;
            std::vector<Entry>              mEntries;
        };

        // Load static geometry from the destination cell into a plain group.
        // Only objects within maxDist (XY) of destCenter are included — important for exterior
        // cells that can contain thousands of statics spread across 8192×8192 units.
        // Also loads light meshes (torches, candles) so they are visually present in the portal.
        osg::ref_ptr<osg::Group> loadCellStatics(
            MWWorld::CellStore* cellStore,
            Resource::ResourceSystem* resourceSystem,
            const osg::Vec3f& destCenter,
            float maxDist = 5000.f)
        {
            osg::ref_ptr<osg::Group> group = new osg::Group;

            if (!cellStore)
                return group;

            const int nStatics    = static_cast<int>(cellStore->getReadOnlyStatics().mList.size());
            const int nDoors      = static_cast<int>(cellStore->getReadOnlyDoors().mList.size());
            const int nContainers = static_cast<int>(cellStore->getReadOnlyContainers().mList.size());
            const int nActivators = static_cast<int>(cellStore->getReadOnlyActivators().mList.size());
            const int nLights     = static_cast<int>(cellStore->getReadOnlyLights().mList.size());
            Log(Debug::Info) << "PortalScene: statics=" << nStatics << " doors=" << nDoors
                << " containers=" << nContainers << " activators=" << nActivators << " lights=" << nLights;

            const float maxDistSq = maxDist * maxDist;

            auto addRefs = [&](auto& refList)
            {
                for (const auto& ref : refList)
                {
                    if (!ref.mBase || ref.mBase->mModel.empty())
                        continue;
                    if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef))
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
            // Skip teleport doors: they are replaced by portal quads in the live scene and
            // would otherwise appear as physical door meshes blocking the RTT view.
            for (const auto& ref : cellStore->getReadOnlyDoors().mList)
            {
                if (!ref.mBase || ref.mBase->mModel.empty()) continue;
                if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef)) continue;
                if (ref.mRef.getTeleport()) continue;  // portal doors: skip
                const ESM::Position& pos = ref.mRef.getPosition();
                const float dx = pos.pos[0] - destCenter.x();
                const float dy = pos.pos[1] - destCenter.y();
                if (dx * dx + dy * dy > maxDistSq) continue;
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
            addRefs(cellStore->getReadOnlyContainers().mList);
            addRefs(cellStore->getReadOnlyActivators().mList);

            addRefs(cellStore->getReadOnlyLights().mList);

            return group;
        }

        // Wrap the geometry in a LightManager with a directional light, cell point lights, and
        // all uniforms that OpenMW's object shaders expect. Mirrors CharacterPreview's RTT setup.
        osg::ref_ptr<osg::Group> buildPortalScene(
            MWWorld::CellStore* cellStore,
            const osg::Vec3f& destCenter,
            Resource::ResourceSystem* resourceSystem,
            osg::Group* exteriorTerrainNode,
            SkyManager* skyManager)
        {
            constexpr float kMaxDist = 5000.f;
            osg::ref_ptr<osg::Group> statics = loadCellStatics(cellStore, resourceSystem, destCenter, kMaxDist);
            // One LightListCallback on the whole group with an explicit large bound so it always
            // intersects with any light in the scene. Individual PAT bounds may be invalid before
            // the first render, causing per-PAT callbacks to silently skip all lights.
            statics->setInitialBound(osg::BoundingSphere(osg::Vec3f(destCenter), kMaxDist));
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
            ss->addUniform(new osg::Uniform("screenRes", osg::Vec2f(1.f, 1.f)));
            ss->addUniform(new osg::Uniform("emissiveMult", 1.f));
            ss->addUniform(new osg::Uniform("specStrength", 1.f));

            // Disable fog (shaders don't respect glDisable(GL_FOG)).
            osg::ref_ptr<osg::Fog> fog = new osg::Fog;
            fog->setStart(100000000.f);
            fog->setEnd(100000000.f);
            ss->setAttributeAndModes(fog, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

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

            // Directional light: use cell mood for interiors; for exterior cells (cave→outside portal)
            // getMood() returns zeros — use a fixed daytime approximation instead.
            osg::Vec4f ambient(0.3f, 0.3f, 0.3f, 1.f);
            osg::Vec4f diffuse(0.7f, 0.7f, 0.7f, 1.f);
            const bool isExterior = cellStore && cellStore->getCell()->isExterior();
            if (cellStore && !isExterior)
            {
                const MWWorld::Cell* cell = cellStore->getCell();
                ambient = SceneUtil::colourFromRGB(cell->getMood().mAmbiantColor);
                diffuse = SceneUtil::colourFromRGB(cell->getMood().mDirectionalColor);
            }
            else if (isExterior)
            {
                ambient = osg::Vec4f(0.35f, 0.35f, 0.35f, 1.f);
                diffuse = osg::Vec4f(0.85f, 0.80f, 0.70f, 1.f);  // warm daylight
            }

            // Ambient goes on LightModel (matches CharacterPreview pattern and OpenMW shader expectations).
            osg::ref_ptr<osg::LightModel> lightModel = new osg::LightModel;
            lightModel->setAmbientIntensity(ambient);
            ss->setAttributeAndModes(lightModel, osg::StateAttribute::ON);

            // Sun direction: interior uses Morrowind's convention; exterior uses a midday approximation.
            const osg::Vec4f sunPos = isExterior
                ? osg::Vec4f(0.5f, -0.5f, 1.f, 0.f)
                : osg::Vec4f(-1.f, osg::DegreesToRadians(45.f), osg::DegreesToRadians(45.f), 0.f);
            osg::ref_ptr<osg::Light> light = new osg::Light;
            light->setLightNum(0);
            light->setPosition(sunPos);
            light->setAmbient(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
            light->setDiffuse(diffuse);
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
                const float maxDistSq = kMaxDist * kMaxDist;
                for (const auto& ref : cellStore->getReadOnlyLights().mList)
                {
                    if (!ref.mBase) continue;
                    if (!MWWorld::CellStore::isAccessible(ref.mData, ref.mRef)) continue;
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

            if (isExterior)
            {
                if (exteriorTerrainNode)
                    lightManager->addChild(exteriorTerrainNode);

            }

            return lightManager;
        }

    }

    PortalManager::PortalManager(Resource::ResourceSystem* resourceSystem, osg::Group* rttParent)
        : mResourceSystem(resourceSystem)
        , mRttParent(rttParent)
    {
    }

    PortalManager::~PortalManager() = default;

    // --------------------------------------------------------------------------

    bool PortalManager::isPortalDoor(const MWWorld::Ptr& door) const
    {
        if (!door.getCellRef().getTeleport())
            return false;
        const auto* base = door.get<ESM::Door>()->mBase;
        if (!base)
            return false;
        std::string model = base->mModel;
        Misc::StringUtils::lowerCaseInPlace(model);
        static const std::string_view sPortalModels[] = {
            "ex_cave_door_01.nif",
            "in_cave_door_01.nif",
            "ex_nord_door_01.nif",
            "hlaalu_loaddoor_ 02.nif",
            "in_hlaalu_door.nif",
            "ex_velothi_loaddoor_01.nif",
            "in_velothismall_ndoor_01.nif",
        };
        for (const auto& pattern : sPortalModels)
            if (model.find(pattern) != std::string::npos)
                return true;
        return false;
    }

    osg::Vec2f PortalManager::computeHalfExtents(const MWWorld::Ptr& door) const
    {
        const osg::Vec2f fallback(96.f, 128.f);

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

            return osg::Vec2f(std::abs(bb.xMax() - bb.xMin()) * 0.5f,
                std::abs(bb.zMax() - bb.zMin()) * 0.5f);
        }
        catch (...)
        {
            return fallback;
        }
    }

    osg::ref_ptr<osg::MatrixTransform> PortalManager::buildQuadNode(
        const osg::Vec2f& halfExtents, const osg::Quat& nifRootQuat) const
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
        ss->setRenderBinToInherit();

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(geom);

        // Only the NIF root rotation is applied here.
        // The PAT (door's base node) will add the CellRef rotation via setAttitude().
        osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
        mt->setMatrix(osg::Matrix::rotate(nifRootQuat));
        mt->addChild(geode);
        mt->setNodeMask(Mask_PortalQuad);

        return mt;
    }

    // --------------------------------------------------------------------------

    bool PortalManager::tryCreatePortal(const MWWorld::Ptr& door)
    {
        if (!isPortalDoor(door))
            return false;

        // Only create portals for genuine exterior↔interior transitions.
        // ex_cave_door_01 / in_cave_door_01 can also appear as interior→interior connectors
        // (deeper cave rooms). Those must not become portals — their destCell is the same
        // cell type as the source cell.
        {
            const ESM::RefId& destId = door.getCellRef().getDestCell();
            const MWWorld::CellStore* destStore
                = MWBase::Environment::get().getWorld()->findCellStore(destId);
            if (!destStore)
                return false;
            if (door.getCell()->getCell()->isExterior() == destStore->getCell()->isExterior())
                return false;
        }

        osg::Group* baseNode = door.getRefData().getBaseNode();
        if (!baseNode)
            return false;

        // Load the NIF to get the root node's rotation (may differ from identity).
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

        osg::Vec2f halfExtents = computeHalfExtents(door);
        osg::ref_ptr<osg::MatrixTransform> quadNode = buildQuadNode(halfExtents, nifRootQuat);
        baseNode->addChild(quadNode);

        // World-space portal plane normal = CellRef rotation * NIF root rotation * (-Y).
        // setNodeRotation() will apply CellRef rotation to the PAT after this call returns.
        const osg::Quat cellRefRot = Misc::Convert::makeOsgQuat(door.getCellRef().getPosition());
        const osg::Quat fullRot = cellRefRot * nifRootQuat;
        const osg::Vec3f normal = fullRot * osg::Vec3f(0.f, -1.f, 0.f);
        const osg::Vec3f pos = door.getRefData().getPosition().asVec3();

        const osg::Vec3f playerPos
            = MWBase::Environment::get().getWorld()->getPlayerPtr().getRefData().getPosition().asVec3();
        const float initialDist = (playerPos - pos) * normal;

        Portal portal;
        portal.door        = door;
        portal.quadNode    = quadNode;
        portal.planePoint  = pos;
        portal.planeNormal = normal;
        portal.invRot      = fullRot.inverse();
        portal.halfExtents = halfExtents;
        portal.lastSide    = (initialDist >= 0.f);
        // tryCreatePortal runs during loadCell, before changePlayerCell sets the arrival position.
        // lastSide is therefore initialized from the old cell's player position and may be wrong.
        // The cooldown lets the player arrive so eyePos-based lastSide can stabilize before the
        // trigger is armed. 30 frames ≈ 0.5 s at 60 fps is enough for the cell transition to settle.
        portal.cooldown    = 30;

        // --- Stage 2: build RTT portal scene ---
        // Wrapped in try-catch: if shader loading or cell loading fails, the portal still works
        // (walk-through teleport is active) but renders as a white quad.
        try
        {
            const ESM::RefId destCellId = door.getCellRef().getDestCell();

            // Destination info — spawn point from CellRef (may differ from actual door center).
            const ESM::Position destPos = door.getCellRef().getDoorDest();
            portal.destPoint = destPos.asVec3();
            portal.destRot   = Misc::Convert::makeOsgQuat(destPos);

            // Find the actual destination door that leads back to our source cell.
            // The spawn point (destPoint) is the player arrival position, not the door center.
            portal.destDoorPos = portal.destPoint;
            portal.destDoorRot = portal.destRot;

            MWWorld::CellStore* destCellStore
                = MWBase::Environment::get().getWorld()->findCellStore(destCellId);
            if (destCellStore)
            {
                const ESM::RefId sourceCellId = door.getCell()->getCell()->getId();
                // Pick the door closest to destPoint when multiple doors lead back to the same
                // source cell (e.g. several Hlaalu buildings sharing an exterior cell).
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

                    osg::Quat destCellRefRot = Misc::Convert::makeOsgQuat(dPos);
                    osg::Quat destNifRootQuat;
                    if (!ref.mBase->mModel.empty())
                    {
                        try
                        {
                            VFS::Path::Normalized destModelPath = Misc::ResourceHelpers::correctMeshPath(
                                VFS::Path::Normalized(ref.mBase->mModel));
                            osg::ref_ptr<const osg::Node> destNode
                                = mResourceSystem->getSceneManager()->getTemplate(destModelPath);
                            if (destNode)
                                destNifRootQuat = getNifRootQuat(destNode.get());
                        }
                        catch (...) {}
                    }
                    portal.destDoorPos = dPos.asVec3();
                    portal.destDoorRot = destCellRefRot * destNifRootQuat;
                }
            }

            Log(Debug::Info) << "PortalScene: destCellId=" << destCellId
                << " destCellStore=" << (destCellStore ? "found" : "NULL")
                << " center=(" << portal.destDoorPos.x() << "," << portal.destDoorPos.y()
                << "," << portal.destDoorPos.z() << ")";

            osg::ref_ptr<osg::Group> portalScene
                = buildPortalScene(destCellStore, portal.destDoorPos, mResourceSystem, mExteriorTerrainNode, mSkyManager);

            const auto screenW = static_cast<uint32_t>(Settings::video().mResolutionX);
            const auto screenH = static_cast<uint32_t>(Settings::video().mResolutionY);

            // Build sky scene for exterior destination portals. Sky is camera-relative and must
            // not be clipped by the portal plane — it is passed separately to PortalRTTNode and
            // added directly to the RTT camera, outside the ClipNode subtree.
            // mSkyNode/mEarlyRenderBinRoot have nodeMask=0 when in interior; children are fine.
            // Sun (Mask_Sun) is excluded by the portal camera cull mask — no occlusion-query crash.
            const bool destIsExterior = destCellStore && destCellStore->getCell()->isExterior();
            osg::ref_ptr<osg::Group> skyScene;
            if (destIsExterior && mSkyManager)
            {
                osg::ref_ptr<CameraRelativeTransform> skyWrapper = new CameraRelativeTransform;
                skyWrapper->setNodeMask(Mask_Sky);
                // RenderBin_Sky=-1 and the sky pass=-1 uniform are provided by the proxy
                // group inside populatePortalSkyGroup (shares mEarlyRenderBinRoot's StateSet).
                mSkyManager->populatePortalSkyGroup(skyWrapper.get());
                if (skyWrapper->getNumChildren() > 0)
                    skyScene = skyWrapper;
            }

            portal.rttNode = new PortalRTTNode(portalScene.get(), skyScene.get(), screenW, screenH, shouldAddMSAAIntermediateTarget());
            portal.destIsExterior = destCellStore && destCellStore->getCell()->isExterior();
            if (portal.destIsExterior)
                portal.rttNode->setClearColor(mExteriorSkyColor);
            {
                // Clip plane: keep geometry on the destination side of the portal plane.
                // Normal = destFwd = into destination (away from player).
                const osg::Vec3f destFwd = portal.destDoorRot * osg::Vec3f(0.f, -1.f, 0.f);
                portal.rttNode->setClipPlaneBoundary(destFwd, portal.destDoorPos);
            }
            portal.rttNode->setClipEnabled(true);
            portal.rttNode->setNodeMask(Mask_RenderToTexture);
            if (mRttParent)
                mRttParent->addChild(portal.rttNode);

            // Portal shader: samples the RTT texture at screen-space UV.
            // getProgram() throws on failure; caught below, rttNode cleaned up via portal.rttNode.
            Shader::ShaderManager& shaderMgr = mResourceSystem->getSceneManager()->getShaderManager();
            osg::ref_ptr<osg::Program> portalProgram = shaderMgr.getProgram("portal");

            osg::ref_ptr<PortalStateSetUpdater> ssUpdater
                = new PortalStateSetUpdater(portal.rttNode.get(), portalProgram.get());
            quadNode->addCullCallback(ssUpdater);
            quadNode->getOrCreateStateSet()->addUniform(
                new osg::Uniform("screenRes", osg::Vec2f(float(screenW), float(screenH))));

            portal.portalScene = portalScene;
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "PortalManager: RTT setup failed (" << e.what() << "); portal will be white";
            if (portal.rttNode && mRttParent)
                mRttParent->removeChild(portal.rttNode);
            portal.rttNode     = nullptr;
            portal.portalScene = nullptr;
        }

        mPortals.push_back(std::move(portal));
        return true;
    }

    void PortalManager::destroyPortal(const MWWorld::Ptr& door)
    {
        auto it = std::find_if(mPortals.begin(), mPortals.end(),
            [&door](const Portal& p) { return p.door.mRef == door.mRef; });
        if (it != mPortals.end())
        {
            // Do NOT call physics cleanup here — destroyPortal runs during cell unloading
            // while physics state is in flux; calling setCollisionFilterMask then crashes.
            // mGhostModeActive stays true so update() cleans up on the next stable frame.

            if (it->rttNode && mRttParent)
                mRttParent->removeChild(it->rttNode);

            // Detach the shared mExteriorTerrainNode from the portal scene so the portal's
            // LightManager no longer owns a reference to it. In the no-sky case portalScene IS
            // the LightManager; in the sky case the LightManager is the first child of the root.
            if (it->destIsExterior && mExteriorTerrainNode && it->portalScene)
            {
                it->portalScene->removeChild(mExteriorTerrainNode);
                for (unsigned int c = 0; c < it->portalScene->getNumChildren(); ++c)
                    if (auto* g = dynamic_cast<osg::Group*>(it->portalScene->getChild(c)))
                        g->removeChild(mExteriorTerrainNode);
            }
            mPortals.erase(it);
        }
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
            if (portal.destIsExterior && portal.rttNode)
                portal.rttNode->setClearColor(mExteriorSkyColor);

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

        ++mDebugFrame;

        // Always write current player position + look to /tmp/player_pos.txt so it can
        // be read from a terminal (watch -n0.5 cat /tmp/player_pos.txt) at any time.
        if (mDebugFrame % 8 == 0)
        {
            std::ofstream pf("/tmp/player_pos.txt", std::ios::trunc);
            if (pf.is_open())
                pf << "playerPos  " << playerPos.x()   << " " << playerPos.y()   << " " << playerPos.z()   << "\n"
                   << "playerLook " << playerLook.x()  << " " << playerLook.y()  << " " << playerLook.z()  << "\n";
        }

        // Find the nearest RTT portal for debug output.
        std::size_t nearestIdx = 0;
        float nearestDist = std::numeric_limits<float>::max();
        for (std::size_t j = 0; j < mPortals.size(); ++j)
        {
            if (!mPortals[j].rttNode) continue;
            const float d = (playerPos - mPortals[j].planePoint).length();
            if (d < nearestDist) { nearestDist = d; nearestIdx = j; }
        }

        for (std::size_t i = 0; i < mPortals.size(); ++i)
        {
            Portal& portal = mPortals[i];

            // Stage 3: update RTT camera to track the player's position and look direction.
            // Maps the player's transform relative to the source portal into destination space.
            if (portal.rttNode)
            {
                const osg::Vec3f diff  = eyePos - portal.planePoint;
                const osg::Vec3f local = portal.invRot * diff;
                // Clamp local.y away from zero so the camera never clips through the portal.
                // For outdoor portal local.y < 0 (player in front); for indoor local.y > 0.
                const float ly = (local.y() < 0.f)
                    ? std::min(local.y(), -10.f)
                    : std::max(local.y(),  10.f);

                const osg::Vec3f forward = portal.destDoorRot * osg::Vec3f(0.f, -1.f, 0.f);
                const osg::Vec3f right   = portal.destDoorRot * osg::Vec3f(1.f,  0.f, 0.f);
                const osg::Vec3f upVec   = portal.destDoorRot * osg::Vec3f(0.f,  0.f, 1.f);

                const float lyAbs = std::abs(ly);

                const osg::Vec3f camPos = portal.destDoorPos
                    - forward * lyAbs
                    - right   * local.x()
                    + upVec   * local.z();

                // RTT camera mirrors the player's lateral look angle through the portal.
                // Source and destination doors may have opposite local-y conventions, so we
                // take x/z (lateral) from the transformed player look but force y = -1
                // (destination convention: local -y = into cave), mirroring how the position
                // formula uses lyAbs to always displace the camera in the forward direction.
                // Both doors use local -y = forward but face opposite world directions.
                // Flip x (mirror) and y (opposite facing), keep z: 180-deg rotation around portal Z.
                const osg::Vec3f srcLocal = portal.invRot * playerLook;
                const osg::Vec3f rttLook = portal.destDoorRot * osg::Vec3f(-srcLocal.x(), -srcLocal.y(), srcLocal.z());
                // Fixed portal up avoids gimbal lock when rttLook approaches world-up.
                const osg::Vec3f rttUp = upVec;
                const osg::Matrixd portalView = osg::Matrix::lookAt(
                    osg::Vec3d(camPos),
                    osg::Vec3d(camPos + rttLook * 400.f),
                    osg::Vec3d(rttUp));

                if (i == nearestIdx && mDebugFrame % 15 == 0)
                {
                    std::ofstream out("/tmp/portal_0.txt", std::ios::trunc);
                    if (out.is_open())
                        out << "camPos   " << camPos.x() << " " << camPos.y() << " " << camPos.z() << "\n"
                            << "# local  " << local.x()  << " " << local.y()  << " " << local.z()  << "\n"
                            << "# lyAbs  " << lyAbs << "\n"
                            << "# destDoorPos " << portal.destDoorPos.x() << " " << portal.destDoorPos.y() << " " << portal.destDoorPos.z() << "\n"
                            << "# destFwd     " << forward.x() << " " << forward.y() << " " << forward.z() << "\n"
                            << "# playerLook  " << playerLook.x() << " " << playerLook.y() << " " << playerLook.z() << "\n";
                }

                portal.rttNode->setViewMatrix(portalView);

                // The main camera's near/far is auto-computed for the interior scene (typically
                // near = 3–10 units). For the exterior portal RTT, objects can be as close as
                // 1 unit to the RTT camera. Using the interior near clips nearby exterior
                // geometry, and the resulting depth-buffer range is suboptimal for the exterior,
                // causing z-fighting between nearly-coplanar surfaces (e.g. thatch and beams).
                // Use a fixed exterior-friendly projection with the same FOV/aspect as the main
                // camera but a smaller near (1 unit) and infinite far (reversed-z, no far clip).
                {
                    double fovY = 0.0, aspect = 0.0, nearP = 0.0, farP = 0.0;
                    if (projMatrix.getPerspective(fovY, aspect, nearP, farP) && fovY > 0.0 && aspect > 0.0)
                        portal.rttNode->setProjectionMatrix(
                            SceneUtil::getReversedZProjectionMatrixAsPerspectiveInf(fovY, aspect, 1.0));
                    else
                        portal.rttNode->setProjectionMatrix(projMatrix);
                }

            }

            if (portal.cooldown > 0)
            {
                --portal.cooldown;
                const float d = (eyePos - portal.planePoint) * portal.planeNormal;
                portal.lastSide = (d >= 0.f);
                continue;
            }

            // Use camera (eye) position for crossing so the trigger fires when the player's
            // view crosses the plane — the natural "walked through" moment.
            const float dist = (eyePos - portal.planePoint) * portal.planeNormal;
            const bool side = (dist >= 0.f);

            // --- Approach ghost mode ---
            // When the player is within the approach zone (positive side, within bounds) but
            // hasn't crossed yet, strip CollisionType_World from the player's collision mask
            // so cave-entrance rocks no longer block the path. A portal-guide floor box keeps
            // the player from falling through the interior floor while World collision is gone.
            constexpr float kApproachDist = 180.f;
            const bool inApproachZone = portal.lastSide && dist >= 0.f && dist < kApproachDist
                && isWithinBounds(eyePos, portal);

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
                    // Floor box: 300×300, 10 units thick, placed 5 units below the door base.
                    const float floorZ = portal.planePoint.z() - portal.halfExtents.y() - 5.f;
                    const osg::Vec3f floorCenter(portal.planePoint.x(), portal.planePoint.y(), floorZ);
                    world->addPortalFloor(floorCenter, 150.f, 150.f);
                    // Two angled guide walls funnelling the player toward the portal opening.
                    world->addPortalGuideWalls(portal.planePoint, portal.invRot.inverse(),
                        portal.halfExtents.x(), portal.halfExtents.y());
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
                }
            }

            // Trigger only when crossing from outside (lastSide=true) to inside.
            if (portal.lastSide && !side && isWithinBounds(eyePos, portal))
            {
                // Clean up ghost mode before the cell change destroys this portal.
                if (portal.approachActive)
                {
                    portal.approachActive = false;
                    mGhostModeActive = false;
                    MWBase::World* world = MWBase::Environment::get().getWorld();
                    world->setPlayerGhostMode(false);
                    world->removePortalFloor();
                    world->removePortalGuideWalls();
                }
                const ESM::RefId destCell = portal.door.getCellRef().getDestCell();
                const ESM::Position destPos = portal.door.getCellRef().getDoorDest();
                // changeToCell is synchronous; may call destroyPortal invalidating mPortals.
                MWBase::Environment::get().getWorld()->changeToCell(destCell, destPos, true, true, true);
                return;
            }

            portal.lastSide = side;
        }
    }

}
