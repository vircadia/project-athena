//
//  Model.h
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 10/18/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Model_h
#define hifi_Model_h

#include <QBitArray>
#include <QObject>
#include <QUrl>
#include <QMutex>

#include <unordered_map>
#include <unordered_set>
#include <functional>

#include <AABox.h>
#include <DependencyManager.h>
#include <GeometryUtil.h>
#include <gpu/Batch.h>
#include <render/Forward.h>
#include <render/Scene.h>
#include <graphics-scripting/Forward.h>
#include <Transform.h>
#include <SpatiallyNestable.h>
#include <TriangleSet.h>
#include <DualQuaternion.h>

#include "RenderHifi.h"
#include "GeometryCache.h"
#include "TextureCache.h"
#include "Rig.h"
#include "PrimitiveMode.h"
#include "BillboardMode.h"

// Use dual quaternion skinning!
// Must match define in Skinning.slh
#define SKIN_DQ

class AbstractViewStateInterface;

class ViewFrustum;

namespace render {
    class Scene;
    class Transaction;
    typedef unsigned int ItemID;
}
class MeshPartPayload;
class ModelMeshPartPayload;
class ModelRenderLocations;

inline uint qHash(const std::shared_ptr<MeshPartPayload>& a, uint seed) {
    return qHash(a.get(), seed);
}

class Model;
using ModelPointer = std::shared_ptr<Model>;
using ModelWeakPointer = std::weak_ptr<Model>;

struct SortedTriangleSet {
    SortedTriangleSet(float distance, TriangleSet* triangleSet, int partIndex, int shapeID, int subMeshIndex) :
        distance(distance), triangleSet(triangleSet), partIndex(partIndex), shapeID(shapeID), subMeshIndex(subMeshIndex) {}

    float distance;
    TriangleSet* triangleSet;
    int partIndex;
    int shapeID;
    int subMeshIndex;
};

using BlendShapeOperator = std::function<void(int, const QVector<BlendshapeOffset>&, const QVector<int>&, const render::ItemIDs&)>;

/// A generic 3D model displaying geometry loaded from a URL.
class Model : public QObject, public std::enable_shared_from_this<Model>, public scriptable::ModelProvider {
    Q_OBJECT

public:

    typedef RenderArgs::RenderMode RenderMode;

    static void setAbstractViewStateInterface(AbstractViewStateInterface* viewState) { _viewState = viewState; }

    Model(QObject* parent = nullptr, SpatiallyNestable* spatiallyNestableOverride = nullptr, uint64_t created = 0);
    virtual ~Model();

    inline ModelPointer getThisPointer() const {
        return std::static_pointer_cast<Model>(std::const_pointer_cast<Model>(shared_from_this()));
    }

    /// Sets the URL of the model to render.
    // Should only be called from the model's rendering thread to avoid access violations of changed geometry.
    Q_INVOKABLE virtual void setURL(const QUrl& url);
    const QUrl& getURL() const { return _url; }

    // new Scene/Engine rendering support
    void setVisibleInScene(bool isVisible, const render::ScenePointer& scene = nullptr);
    bool isVisible() const;

    render::hifi::Tag getTagMask() const;
    void setTagMask(uint8_t mask, const render::ScenePointer& scene = nullptr);

    bool isGroupCulled() const;
    void setGroupCulled(bool isGroupCulled, const render::ScenePointer& scene = nullptr);

    bool canCastShadow() const;
    void setCanCastShadow(bool canCastShadow, const render::ScenePointer& scene = nullptr);

    void setHifiRenderLayer(render::hifi::Layer layer, const render::ScenePointer& scene = nullptr);

    bool isCauterized() const { return _cauterized; }
    void setCauterized(bool value, const render::ScenePointer& scene = nullptr);

    void setPrimitiveMode(PrimitiveMode primitiveMode, const render::ScenePointer& scene = nullptr);
    PrimitiveMode getPrimitiveMode() const { return _primitiveMode; }

    void setBillboardMode(BillboardMode billboardMode, const render::ScenePointer& scene = nullptr);
    BillboardMode getBillboardMode() const { return _billboardMode; }

    void setCullWithParent(bool value, const render::ScenePointer& scene = nullptr);

    void setRenderWithZones(const QVector<QUuid>& renderWithZones, const render::ScenePointer& scene = nullptr);
    const QVector<QUuid>& getRenderWithZones() const { return _renderWithZones; }

    // Access the current RenderItemKey Global Flags used by the model and applied to the render items  representing the parts of the model.
    const render::ItemKey getRenderItemKeyGlobalFlags() const;

    bool needsFixupInScene() const;

    bool needsReload() const { return _needsReload; }
    bool addToScene(const render::ScenePointer& scene,
                    render::Transaction& transaction) {
        auto getters = render::Item::Status::Getters(0);
        return addToScene(scene, transaction, getters);
    }
    bool addToScene(const render::ScenePointer& scene,
                    render::Transaction& transaction,
                    BlendShapeOperator modelBlendshapeOperator) {
        auto getters = render::Item::Status::Getters(0);
        return addToScene(scene, transaction, getters, modelBlendshapeOperator);
    }
    bool addToScene(const render::ScenePointer& scene,
                    render::Transaction& transaction,
                    render::Item::Status::Getters& statusGetters,
                    BlendShapeOperator modelBlendshapeOperator = nullptr);
    void removeFromScene(const render::ScenePointer& scene, render::Transaction& transaction);
    bool isRenderable() const;

    void updateRenderItemsKey(const render::ScenePointer& scene);

    virtual void updateRenderItems();
    void setRenderItemsNeedUpdate();
    bool getRenderItemsNeedUpdate() { return _renderItemsNeedUpdate; }
    AABox getRenderableMeshBound() const;
    const render::ItemIDs& fetchRenderItemIDs() const;

    bool maybeStartBlender();

    bool isLoaded() const { return (bool)_renderGeometry && _renderGeometry->isHFMModelLoaded(); }
    bool isAddedToScene() const { return _addedToScene; }

    void reset();

    void setSnapModelToRegistrationPoint(bool snapModelToRegistrationPoint, const glm::vec3& registrationPoint);
    bool getSnapModelToRegistrationPoint() { return _snapModelToRegistrationPoint; }
    bool getSnappedToRegistrationPoint() { return _snappedToRegistrationPoint; }

    virtual void simulate(float deltaTime, bool fullUpdate = true);
    virtual void updateClusterMatrices();
    virtual void updateBlendshapes();

    /// Returns a reference to the shared geometry.
    const Geometry::Pointer& getGeometry() const { return _renderGeometry; }

    const QVariantMap getTextures() const { assert(isLoaded()); return _renderGeometry->getTextures(); }
    Q_INVOKABLE virtual void setTextures(const QVariantMap& textures);

    /// Provided as a convenience, will crash if !isLoaded()
    // And so that getHFMModel() isn't chained everywhere
    const HFMModel& getHFMModel() const { assert(isLoaded()); return _renderGeometry->getHFMModel(); }
    const MaterialMapping& getMaterialMapping() const { assert(isLoaded()); return _renderGeometry->getMaterialMapping(); }

    bool didVisualGeometryRequestFail() const { return _visualGeometryRequestFailed; }

    glm::mat4 getWorldToHFMMatrix() const;

    QStringList getJointNames() const;

    /// Sets the joint state at the specified index.
    void setJointState(int index, bool valid, const glm::quat& rotation, const glm::vec3& translation, float priority);
    void setJointRotation(int index, bool valid, const glm::quat& rotation, float priority);
    void setJointTranslation(int index, bool valid, const glm::vec3& translation, float priority);

    bool findRayIntersectionAgainstSubMeshes(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& viewFrustumPos,
                                             float& distance, BoxFace& face, glm::vec3& surfaceNormal,
                                             QVariantMap& extraInfo, bool pickAgainstTriangles = false, bool allowBackface = false);
    bool findParabolaIntersectionAgainstSubMeshes(const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration,
                                                  const glm::vec3& viewFrustumPos, float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal,
                                                  QVariantMap& extraInfo, bool pickAgainstTriangles = false, bool allowBackface = false);

    void setOffset(const glm::vec3& offset);
    const glm::vec3& getOffset() const { return _offset; }
    glm::vec3 getOriginalOffset() const;

    void setScaleToFit(bool scaleToFit, float largestDimension = 0.0f, bool forceRescale = false);
    void setScaleToFit(bool scaleToFit, const glm::vec3& dimensions, bool forceRescale = false);
    bool getScaleToFit() const { return _scaleToFit; } /// is scale to fit enabled

    void setSnapModelToCenter(bool snapModelToCenter) {
        setSnapModelToRegistrationPoint(snapModelToCenter, glm::vec3(0.5f,0.5f,0.5f));
    };
    bool getSnapModelToCenter() {
        return _snapModelToRegistrationPoint && _registrationPoint == glm::vec3(0.5f,0.5f,0.5f);
    }

    /// Returns the number of joint states in the model.
    int getJointStateCount() const { return (int)_rig.getJointStateCount(); }
    bool getJointPositionInWorldFrame(int jointIndex, glm::vec3& position) const;
    bool getJointRotationInWorldFrame(int jointIndex, glm::quat& rotation) const;

    /// \param jointIndex index of joint in model structure
    /// \param rotation[out] rotation of joint in model-frame
    /// \return true if joint exists
    bool getJointRotation(int jointIndex, glm::quat& rotation) const;
    bool getJointTranslation(int jointIndex, glm::vec3& translation) const;

    // model frame
    bool getAbsoluteJointRotationInRigFrame(int jointIndex, glm::quat& rotationOut) const;
    bool getAbsoluteJointTranslationInRigFrame(int jointIndex, glm::vec3& translationOut) const;

    bool getRelativeDefaultJointRotation(int jointIndex, glm::quat& rotationOut) const;
    bool getRelativeDefaultJointTranslation(int jointIndex, glm::vec3& translationOut) const;

    /// Returns the index of the parent of the indexed joint, or -1 if not found.
    int getParentJointIndex(int jointIndex) const;

    /// Returns the extents of the model in its bind pose.
    Extents getBindExtents() const;

    /// Returns the extents of the model's mesh
    Extents getMeshExtents() const;

    /// Returns the unscaled extents of the model's mesh
    Extents getUnscaledMeshExtents() const;

    void setTranslation(const glm::vec3& translation);
    void setRotation(const glm::quat& rotation);
    void overrideModelTransformAndOffset(const Transform& transform, const glm::vec3& offset);
    bool isOverridingModelTransformAndOffset() { return _overrideModelTransform; };
    void stopTransformAndOffsetOverride() { _overrideModelTransform = false; };
    void setTransformNoUpdateRenderItems(const Transform& transform); // temporary HACK

    const glm::vec3& getTranslation() const { return _translation; }
    const glm::quat& getRotation() const { return _rotation; }
    const glm::vec3& getOverrideTranslation() const { return _overrideTranslation; }
    const glm::quat& getOverrideRotation() const { return _overrideRotation; }

    glm::vec3 getNaturalDimensions() const;

    Transform getTransform() const;

    void setScale(const glm::vec3& scale);
    const glm::vec3& getScale() const { return _scale; }

    /// enables/disables scale to fit behavior, the model will be automatically scaled to the specified largest dimension
    bool getIsScaledToFit() const { return _scaledToFit; } /// is model scaled to fit
    glm::vec3 getScaleToFitDimensions() const; /// the dimensions model is scaled to, including inferred y/z

    int getBlendshapeCoefficientsNum() const { return _blendshapeCoefficients.size(); }
    float getBlendshapeCoefficient(int index) const {
        return ((index < 0) && (index >= _blendshapeCoefficients.size())) ? 0.0f : _blendshapeCoefficients.at(index);
     }

    Rig& getRig() { return _rig; }
    const Rig& getRig() const { return _rig; }

    const glm::vec3& getRegistrationPoint() const { return _registrationPoint; }

    // returns 'true' if needs fullUpdate after geometry change
    virtual bool updateGeometry();

    void setLoadingPriority(float priority) { _loadingPriority = priority; }

    size_t getRenderInfoVertexCount() const { return _renderInfoVertexCount; }
    size_t getRenderInfoTextureSize();
    int getRenderInfoTextureCount();
    int getRenderInfoDrawCalls() const { return _renderInfoDrawCalls; }
    bool getRenderInfoHasTransparent() const { return _renderInfoHasTransparent; }

    class TransformDualQuaternion {
    public:
        TransformDualQuaternion() {}
        TransformDualQuaternion(const glm::mat4& m) {
            AnimPose p(m);
            _scale.x = p.scale().x;
            _scale.y = p.scale().y;
            _scale.z = p.scale().z;
            _scale.w = 0.0f;
            _dq = DualQuaternion(p.rot(), p.trans());
        }
        TransformDualQuaternion(const glm::vec3& scale, const glm::quat& rot, const glm::vec3& trans) {
            _scale.x = scale.x;
            _scale.y = scale.y;
            _scale.z = scale.z;
            _scale.w = 0.0f;
            _dq = DualQuaternion(rot, trans);
        }
        TransformDualQuaternion(const Transform& transform) {
            _scale = glm::vec4(transform.getScale(), 0.0f);
            _scale.w = 0.0f;
            _dq = DualQuaternion(transform.getRotation(), transform.getTranslation());
        }
        glm::vec3 getScale() const { return glm::vec3(_scale); }
        glm::quat getRotation() const { return _dq.getRotation(); }
        glm::vec3 getTranslation() const { return _dq.getTranslation(); }
        glm::mat4 getMatrix() const { return createMatFromScaleQuatAndPos(getScale(), getRotation(), getTranslation()); };

        void setCauterizationParameters(float cauterizationAmount, const glm::vec3& cauterizedPosition) {
            _scale.w = cauterizationAmount;
            _cauterizedPosition = glm::vec4(cauterizedPosition, 1.0f);
        }
    protected:
        glm::vec4 _scale { 1.0f, 1.0f, 1.0f, 0.0f };
        DualQuaternion _dq;
        glm::vec4 _cauterizedPosition { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    class MeshState {
    public:
        std::vector<TransformDualQuaternion> clusterDualQuaternions;
        std::vector<glm::mat4> clusterMatrices;
    };

    const MeshState& getMeshState(int index) { return _meshStates.at(index); }

    const QMap<render::ItemID, render::PayloadPointer>& getRenderItems() const { return _modelMeshRenderItemsMap; }
    BlendShapeOperator getModelBlendshapeOperator() const { return _modelBlendshapeOperator; }

    void renderDebugMeshBoxes(gpu::Batch& batch, bool forward);

    int getResourceDownloadAttempts() { return _renderWatcher.getResourceDownloadAttempts(); }
    int getResourceDownloadAttemptsRemaining() { return _renderWatcher.getResourceDownloadAttemptsRemaining(); }

    Q_INVOKABLE MeshProxyList getMeshes() const;
    virtual scriptable::ScriptableModelBase getScriptableModel() override;
    virtual bool replaceScriptableModelMeshPart(scriptable::ScriptableModelBasePointer model, int meshIndex, int partIndex) override;

    void scaleToFit();
    void snapToRegistrationPoint();
    bool getUseDualQuaternionSkinning() const { return _useDualQuaternionSkinning; }
    void setUseDualQuaternionSkinning(bool value);

    void addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName);
    void removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName);

    void setBlendshapeCoefficients(const QVector<float>& coefficients) { _blendshapeCoefficients = coefficients; }

public slots:
    void loadURLFinished(bool success);

signals:
    void setURLFinished(bool success);
    void setCollisionModelURLFinished(bool success);
    void requestRenderUpdate();
    void rigReady();
    void rigReset();

protected:

    std::unordered_map<unsigned int, quint16> _priorityMap; // only used for materialMapping
    std::unordered_map<unsigned int, std::vector<graphics::MaterialLayer>> _materialMapping; // generated during applyMaterialMapping
    std::mutex _materialMappingMutex;
    void applyMaterialMapping();

    const QVector<float>& getBlendshapeCoefficients() const { return _blendshapeCoefficients; }

    /// Clear the joint states
    void clearJointState(int index);

    /// \param jointIndex index of joint in model structure
    /// \param position[out] position of joint in model-frame
    /// \return true if joint exists
    bool getJointPosition(int jointIndex, glm::vec3& position) const;

    Geometry::Pointer _renderGeometry; // only ever set by its watcher

    GeometryResourceWatcher _renderWatcher;

    SpatiallyNestable* _spatiallyNestableOverride;

    glm::vec3 _translation; // this is the translation in world coordinates to the model's registration point
    glm::quat _rotation;
    glm::vec3 _scale { 1.0f };

    glm::vec3 _overrideTranslation;
    glm::quat _overrideRotation;

    // For entity models this is the translation for the minimum extent of the model (in original mesh coordinate space)
    // to the model's registration point. For avatar models this is the translation from the avatar's hips, as determined
    // by the default pose, to the origin.
    glm::vec3 _offset;

    static float FAKE_DIMENSION_PLACEHOLDER;

    bool _scaleToFit; /// If you set scaleToFit, we will calculate scale based on MeshExtents
    glm::vec3 _scaleToFitDimensions; /// this is the dimensions that scale to fit will use
    bool _scaledToFit; /// have we scaled to fit

    bool _snapModelToRegistrationPoint; /// is the model's offset automatically adjusted to a registration point in model space
    bool _snappedToRegistrationPoint; /// are we currently snapped to a registration point
    glm::vec3 _registrationPoint { glm::vec3(0.5f) }; /// the point in model space our center is snapped to
    bool _forceOffset { false };

    std::vector<MeshState> _meshStates;

    virtual void initJointStates();

    void setScaleInternal(const glm::vec3& scale);

    virtual void updateRig(float deltaTime, glm::mat4 parentTransform);

    /// Allow sub classes to force invalidating the bboxes
    void invalidCalculatedMeshBoxes() {
        _triangleSetsValid = false;
    }

    // hook for derived classes to be notified when setUrl invalidates the current model.
    virtual void onInvalidate() {};

    virtual void deleteGeometry();

    QUrl _url;

    BlendShapeOperator _modelBlendshapeOperator { nullptr };
    QVector<float> _blendshapeCoefficients;
    QVector<float> _blendedBlendshapeCoefficients;
    int _blendNumber { 0 };

    mutable QMutex _mutex{ QMutex::Recursive };

    bool _overrideModelTransform { false };
    bool _triangleSetsValid { false };
    void calculateTriangleSets(const HFMModel& hfmModel);
    std::vector<std::vector<TriangleSet>> _modelSpaceMeshTriangleSets; // model space triangles for all sub meshes

    virtual void createRenderItemSet();

    PrimitiveMode _primitiveMode { PrimitiveMode::SOLID };
    BillboardMode _billboardMode { BillboardMode::NONE };
    bool _useDualQuaternionSkinning { false };

    // debug rendering support
    int _debugMeshBoxesID = GeometryCache::UNKNOWN_ID;

    static AbstractViewStateInterface* _viewState;

    QVector<std::shared_ptr<ModelMeshPartPayload>> _modelMeshRenderItems;
    QMap<render::ItemID, render::PayloadPointer> _modelMeshRenderItemsMap;
    render::ItemIDs _modelMeshRenderItemIDs;
    using ShapeInfo = struct { int meshIndex; };
    std::vector<ShapeInfo> _modelMeshRenderItemShapes;
    std::vector<std::string> _modelMeshMaterialNames;

    bool _addedToScene { false }; // has been added to scene
    bool _needsFixupInScene { true }; // needs to be removed/re-added to scene
    bool _needsReload { true };
    bool _needsUpdateClusterMatrices { true };
    QVariantMap _pendingTextures { };

    friend class ModelMeshPartPayload;
    Rig _rig;

    bool _visualGeometryRequestFailed { false };

    bool _renderItemsNeedUpdate { false };

    size_t _renderInfoVertexCount { 0 };
    int _renderInfoTextureCount { 0 };
    size_t _renderInfoTextureSize { 0 };
    bool _hasCalculatedTextureInfo { false };
    int _renderInfoDrawCalls { 0 };
    int _renderInfoHasTransparent { false };

    // This Render ItemKey Global Flags capture the Model wide global set of flags that should be communicated to all the render items representing the Model.
    // The flags concerned are:
    //  - isVisible: if true the Model is visible globally in the scene, regardless of the other flags in the item keys (tags or layer or shadow caster).
    //  - TagBits: the view mask defined through the TagBits telling in which view the Model is rendered if visible.
    //  - Layer: In which Layer this Model lives.
    //  - CastShadow: if true and visible and rendered in the view, the Model cast shadows if in a Light volume casting shadows.
    //  - CullGroup: if true, the render items representing the parts of the Model are culled by a single Meta render item that knows about them, they are not culled individually.
    //               For this to work, a Meta RI must exists and knows about the RIs of this Model.
    //  
    render::ItemKey _renderItemKeyGlobalFlags;
    bool _cauterized { false };
    bool _cullWithParent { false };
    QVector<QUuid> _renderWithZones;

    bool shouldInvalidatePayloadShapeKey(int meshIndex);

    uint64_t _created;

private:
    float _loadingPriority { 0.0f };

    void calculateTextureInfo();

    std::set<unsigned int> getMeshIDsFromMaterialID(QString parentMaterialName);
};

Q_DECLARE_METATYPE(ModelPointer)
Q_DECLARE_METATYPE(Geometry::WeakPointer)
Q_DECLARE_METATYPE(BlendshapeOffset)

/// Handle management of pending models that need blending
class ModelBlender : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:

    /// Adds the specified model to the list requiring vertex blends.
    void noteRequiresBlend(ModelPointer model);

    bool shouldComputeBlendshapes() { return _computeBlendshapes; }

public slots:
    void setBlendedVertices(ModelPointer model, int blendNumber, QVector<BlendshapeOffset> blendshapeOffsets, QVector<int> blendedMeshSizes);
    void setComputeBlendshapes(bool computeBlendshapes) { _computeBlendshapes = computeBlendshapes; }

private:
    using Mutex = std::mutex;
    using Lock = std::unique_lock<Mutex>;

    ModelBlender();
    virtual ~ModelBlender();

    std::queue<ModelWeakPointer> _modelsRequiringBlendsQueue;
    std::set<ModelWeakPointer, std::owner_less<ModelWeakPointer>> _modelsRequiringBlendsSet;
    int _pendingBlenders;
    Mutex _mutex;

    bool _computeBlendshapes { true };
};


#endif // hifi_Model_h
