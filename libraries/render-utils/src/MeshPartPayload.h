//
//  MeshPartPayload.h
//  interface/src/renderer
//
//  Created by Sam Gateau on 10/3/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MeshPartPayload_h
#define hifi_MeshPartPayload_h

#include "Model.h"

#include <gpu/Batch.h>
#include <render/Scene.h>
#include <graphics/Geometry.h>

class ModelMeshPartPayload {
public:
    typedef render::Payload<ModelMeshPartPayload> Payload;
    typedef Payload::DataPointer Pointer;

    ModelMeshPartPayload(ModelPointer model, int meshIndex, int partIndex, int shapeIndex, const Transform& transform, const uint64_t& created);

    virtual void updateMeshPart(const std::shared_ptr<const graphics::Mesh>& drawMesh, int partIndex);

    void updateClusterBuffer(const std::vector<glm::mat4>& clusterMatrices); // matrix palette skinning
    void updateClusterBuffer(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions); // dual quaternion skinning

    void computeAdjustedLocalBound(const std::vector<glm::mat4>& clusterMatrices); // matrix palette skinning
    void computeAdjustedLocalBound(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions); // dual quaternion skinning

    void updateTransformForSkinnedMesh(const Transform& modelTransform, const Model::MeshState& meshState, bool useDualQuaternionSkinning);

    // ModelMeshPartPayload functions to perform render
    void bindMesh(gpu::Batch& batch);
    virtual void bindTransform(gpu::Batch& batch, const Transform& transform, RenderArgs::RenderMode renderMode) const;
    void drawCall(gpu::Batch& batch) const;

    void updateKey(const render::ItemKey& key);
    void setShapeKey(bool invalidateShapeKey, PrimitiveMode primitiveMode, bool useDualQuaternionSkinning);

    // Render Item interface
    render::ItemKey getKey() const;
    render::Item::Bound getBound(RenderArgs* args) const;
    render::ShapeKey getShapeKey() const;
    void render(RenderArgs* args);

    size_t getVerticesCount() const { return _drawMesh ? _drawMesh->getNumVertices() : 0; }
    size_t getMaterialTextureSize() { return _drawMaterials.getTextureSize(); }
    int getMaterialTextureCount() { return _drawMaterials.getTextureCount(); }
    bool hasTextureInfo() const { return _drawMaterials.hasTextureInfo(); }

    void setCauterized(bool cauterized) { _cauterized = cauterized; }
    void setCullWithParent(bool value) { _cullWithParent = value; }
    void setRenderWithZones(const QVector<QUuid>& renderWithZones) { _renderWithZones = renderWithZones; }
    void setBillboardMode(BillboardMode billboardMode) { _billboardMode = billboardMode; }
    bool passesZoneOcclusionTest(const std::unordered_set<QUuid>& containingZones) const;

    void addMaterial(graphics::MaterialLayer material) { _drawMaterials.push(material); }
    void removeMaterial(graphics::MaterialPointer material) { _drawMaterials.remove(material); }

    void setBlendshapeBuffer(const std::unordered_map<int, gpu::BufferPointer>& blendshapeBuffers, const QVector<int>& blendedMeshSizes);

    static bool enableMaterialProceduralShaders;

protected: 
    mutable Transform _prevRenderTransform;

private:
    void initCache(const ModelPointer& model, int shapeID);

    int _meshIndex;
    std::shared_ptr<const graphics::Mesh> _drawMesh;
    graphics::Mesh::Part _drawPart;
    graphics::MultiMaterial _drawMaterials;

    gpu::BufferPointer _clusterBuffer;
    enum class ClusterBufferType { Matrices, DualQuaternions };
    ClusterBufferType _clusterBufferType { ClusterBufferType::Matrices };

    gpu::BufferPointer _meshBlendshapeBuffer;
    int _meshNumVertices;

    render::ItemKey _itemKey { render::ItemKey::Builder::opaqueShape().build() };
    render::ShapeKey _shapeKey { render::ShapeKey::Builder::invalid() };

    bool _isSkinned { false };
    bool _isBlendShaped { false };
    bool _hasTangents { false };
    bool _prevUseDualQuaternionSkinning { false };
    bool _cauterized { false };
    bool _cullWithParent { false };
    QVector<QUuid> _renderWithZones;
    BillboardMode _billboardMode;
    uint64_t _created;

    Transform _localTransform;
    Transform _parentTransform;
    graphics::Box _localBound;
    graphics::Box _adjustedLocalBound;
};

namespace render {
    template <> const ItemKey payloadGetKey(const ModelMeshPartPayload::Pointer& payload);
    template <> const Item::Bound payloadGetBound(const ModelMeshPartPayload::Pointer& payload, RenderArgs* args);
    template <> const ShapeKey shapeGetShapeKey(const ModelMeshPartPayload::Pointer& payload);
    template <> void payloadRender(const ModelMeshPartPayload::Pointer& payload, RenderArgs* args);
    template <> bool payloadPassesZoneOcclusionTest(const ModelMeshPartPayload::Pointer& payload, const std::unordered_set<QUuid>& containingZones);
}

#endif // hifi_MeshPartPayload_h
