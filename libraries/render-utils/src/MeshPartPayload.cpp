//
//  MeshPartPayload.cpp
//  interface/src/renderer
//
//  Created by Sam Gateau on 10/3/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MeshPartPayload.h"

#include <BillboardMode.h>
#include <PerfStat.h>
#include <DualQuaternion.h>
#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include <procedural/Procedural.h>
#include "DeferredLightingEffect.h"

#include "RenderPipelines.h"

using namespace render;

bool ModelMeshPartPayload::enableMaterialProceduralShaders = false;

ModelMeshPartPayload::ModelMeshPartPayload(ModelPointer model, int meshIndex, int partIndex, int shapeIndex,
                                           const Transform& transform, const uint64_t& created) :
    _meshIndex(meshIndex),
    _created(created) {

    assert(model && model->isLoaded());

    auto& modelMesh = model->getGeometry()->getMeshes().at(_meshIndex);
    _meshNumVertices = (int)modelMesh->getNumVertices();
    const Model::MeshState& state = model->getMeshState(_meshIndex);

    updateMeshPart(modelMesh, partIndex);

    bool useDualQuaternionSkinning = model->getUseDualQuaternionSkinning();
    if (useDualQuaternionSkinning) {
        computeAdjustedLocalBound(state.clusterDualQuaternions);
    } else {
        computeAdjustedLocalBound(state.clusterMatrices);
    }

    updateTransformForSkinnedMesh(transform, state, useDualQuaternionSkinning);

    initCache(model, shapeIndex);

#if defined(Q_OS_MAC) || defined(Q_OS_ANDROID)
    // On mac AMD, we specifically need to have a _meshBlendshapeBuffer bound when using a deformed mesh pipeline
    // it cannot be null otherwise we crash in the drawcall using a deformed pipeline with a skinned only (not blendshaped) mesh
    if (_isBlendShaped) {
        std::vector<BlendshapeOffset> data(_meshNumVertices);
        const auto blendShapeBufferSize = _meshNumVertices * sizeof(BlendshapeOffset);
        _meshBlendshapeBuffer = std::make_shared<gpu::Buffer>(blendShapeBufferSize, reinterpret_cast<const gpu::Byte*>(data.data()), blendShapeBufferSize);
    } else if (_isSkinned) {
        BlendshapeOffset data;
        _meshBlendshapeBuffer = std::make_shared<gpu::Buffer>(sizeof(BlendshapeOffset), reinterpret_cast<const gpu::Byte*>(&data), sizeof(BlendshapeOffset));
    }
#endif
}

void ModelMeshPartPayload::initCache(const ModelPointer& model, int shapeID) {
    if (_drawMesh) {
        auto vertexFormat = _drawMesh->getVertexFormat();
        _isSkinned = vertexFormat->hasAttribute(gpu::Stream::SKIN_CLUSTER_WEIGHT) && vertexFormat->hasAttribute(gpu::Stream::SKIN_CLUSTER_INDEX);

        const HFMModel& hfmModel = model->getHFMModel();
        const HFMMesh& mesh = hfmModel.meshes.at(_meshIndex);

        _isBlendShaped = !mesh.blendshapes.isEmpty();
        _hasTangents = !mesh.tangents.isEmpty();
    }

    auto networkMaterial = model->getGeometry()->getShapeMaterial(shapeID);
    if (networkMaterial) {
        addMaterial(graphics::MaterialLayer(networkMaterial, 0));
    }
}

void ModelMeshPartPayload::updateMeshPart(const std::shared_ptr<const graphics::Mesh>& drawMesh, int partIndex) {
    _drawMesh = drawMesh;
    if (_drawMesh) {
        auto vertexFormat = _drawMesh->getVertexFormat();
        _drawPart = _drawMesh->getPartBuffer().get<graphics::Mesh::Part>(partIndex);
        _localBound = _drawMesh->evalPartBound(partIndex);
    }
}

void ModelMeshPartPayload::updateClusterBuffer(const std::vector<glm::mat4>& clusterMatrices) {
    // reset cluster buffer if we change the cluster buffer type
    if (_clusterBufferType != ClusterBufferType::Matrices) {
        _clusterBuffer.reset();
    }
    _clusterBufferType = ClusterBufferType::Matrices;

    // Once computed the cluster matrices, update the buffer(s)
    if (clusterMatrices.size() > 1) {
        if (!_clusterBuffer) {
            _clusterBuffer = std::make_shared<gpu::Buffer>(clusterMatrices.size() * sizeof(glm::mat4),
                (const gpu::Byte*) clusterMatrices.data());
        } else {
            _clusterBuffer->setSubData(0, clusterMatrices.size() * sizeof(glm::mat4),
                (const gpu::Byte*) clusterMatrices.data());
        }
    }
}

void ModelMeshPartPayload::updateClusterBuffer(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions) {
    // reset cluster buffer if we change the cluster buffer type
    if (_clusterBufferType != ClusterBufferType::DualQuaternions) {
        _clusterBuffer.reset();
    }
    _clusterBufferType = ClusterBufferType::DualQuaternions;

    // Once computed the cluster matrices, update the buffer(s)
    if (clusterDualQuaternions.size() > 1) {
        if (!_clusterBuffer) {
            _clusterBuffer = std::make_shared<gpu::Buffer>(clusterDualQuaternions.size() * sizeof(Model::TransformDualQuaternion),
                (const gpu::Byte*) clusterDualQuaternions.data());
        } else {
            _clusterBuffer->setSubData(0, clusterDualQuaternions.size() * sizeof(Model::TransformDualQuaternion),
                (const gpu::Byte*) clusterDualQuaternions.data());
        }
    }
}

void ModelMeshPartPayload::computeAdjustedLocalBound(const std::vector<glm::mat4>& clusterMatrices) {
    _adjustedLocalBound = _localBound;
    if (clusterMatrices.size() > 0) {
        _adjustedLocalBound.transform(clusterMatrices.back());

        for (int i = 0; i < (int)clusterMatrices.size() - 1; ++i) {
            AABox clusterBound = _localBound;
            clusterBound.transform(clusterMatrices[i]);
            _adjustedLocalBound += clusterBound;
        }
    }
}

void ModelMeshPartPayload::computeAdjustedLocalBound(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions) {
    _adjustedLocalBound = _localBound;
    if (clusterDualQuaternions.size() > 0) {
        Transform rootTransform(clusterDualQuaternions.back().getRotation(),
                                clusterDualQuaternions.back().getScale(),
                                clusterDualQuaternions.back().getTranslation());
        _adjustedLocalBound.transform(rootTransform);

        for (int i = 0; i < (int)clusterDualQuaternions.size() - 1; ++i) {
            AABox clusterBound = _localBound;
            Transform transform(clusterDualQuaternions[i].getRotation(),
                                clusterDualQuaternions[i].getScale(),
                                clusterDualQuaternions[i].getTranslation());
            clusterBound.transform(transform);
            _adjustedLocalBound += clusterBound;
        }
    }
}

void ModelMeshPartPayload::updateTransformForSkinnedMesh(const Transform& modelTransform, const Model::MeshState& meshState, bool useDualQuaternionSkinning) {
    _localTransform = Transform();
    if (useDualQuaternionSkinning) {
        if (meshState.clusterDualQuaternions.size() == 1 || meshState.clusterDualQuaternions.size() == 2) {
            const auto& dq = meshState.clusterDualQuaternions[0];
            _localTransform = Transform(dq.getRotation(),
                                       dq.getScale(),
                                       dq.getTranslation());
        }
    } else {
        if (meshState.clusterMatrices.size() == 1 || meshState.clusterMatrices.size() == 2) {
            _localTransform = Transform(meshState.clusterMatrices[0]);
        }
    }

    _parentTransform = modelTransform;
}

void ModelMeshPartPayload::bindMesh(gpu::Batch& batch) {
    batch.setIndexBuffer(gpu::UINT32, (_drawMesh->getIndexBuffer()._buffer), 0);
    batch.setInputFormat((_drawMesh->getVertexFormat()));
    if (_meshBlendshapeBuffer) {
        batch.setResourceBuffer(0, _meshBlendshapeBuffer);
    }
    batch.setInputStream(0, _drawMesh->getVertexStream());
}

void ModelMeshPartPayload::bindTransform(gpu::Batch& batch, const Transform& transform, RenderArgs::RenderMode renderMode) const {
    if (_clusterBuffer) {
        batch.setUniformBuffer(graphics::slot::buffer::Skinning, _clusterBuffer);
    }
    batch.setModelTransform(transform, _prevRenderTransform);
    if (renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }
}

void ModelMeshPartPayload::drawCall(gpu::Batch& batch) const {
    batch.drawIndexed(gpu::TRIANGLES, _drawPart._numIndices, _drawPart._startIndex);
}

void ModelMeshPartPayload::updateKey(const render::ItemKey& key) {
    ItemKey::Builder builder(key);
    builder.withTypeShape();

    if (_isBlendShaped || _isSkinned) {
        builder.withDeformed();
    }

    if (_drawMaterials.shouldUpdate()) {
        RenderPipelines::updateMultiMaterial(_drawMaterials);
    }

    auto matKey = _drawMaterials.getMaterialKey();
    if (matKey.isTranslucent()) {
        builder.withTransparent();
    }

    if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    _itemKey = builder.build();
}

void ModelMeshPartPayload::setShapeKey(bool invalidateShapeKey, PrimitiveMode primitiveMode, bool useDualQuaternionSkinning) {
    if (invalidateShapeKey) {
        _shapeKey = ShapeKey::Builder::invalid();
        return;
    }

    if (_drawMaterials.shouldUpdate()) {
        RenderPipelines::updateMultiMaterial(_drawMaterials);
    }

    ShapeKey::Builder builder;
    graphics::MaterialPointer material = _drawMaterials.empty() ? nullptr : _drawMaterials.top().material;
    graphics::MaterialKey drawMaterialKey = _drawMaterials.getMaterialKey();

    bool isWireframe = primitiveMode == PrimitiveMode::LINES;

    if (isWireframe) {
        builder.withWireframe();
    } else if (drawMaterialKey.isTranslucent()) {
        builder.withTranslucent();
    }

    if (_isSkinned || (_isBlendShaped && _meshBlendshapeBuffer)) {
        builder.withDeformed();
        if (useDualQuaternionSkinning) {
            builder.withDualQuatSkinned();
        }
    }

    if (material && material->isProcedural() && material->isReady()) {
        builder.withOwnPipeline();
    } else {
        bool hasTangents = drawMaterialKey.isNormalMap() && _hasTangents;
        bool hasLightmap = drawMaterialKey.isLightMap();
        bool isUnlit = drawMaterialKey.isUnlit();

        if (isWireframe) {
            hasTangents = hasLightmap = false;
        }

        builder.withMaterial();

        if (hasTangents) {
            builder.withTangents();
        }
        if (hasLightmap) {
            builder.withLightMap();
        }
        if (isUnlit) {
            builder.withUnlit();
        }
        if (material) {
            builder.withCullFaceMode(material->getCullFaceMode());
        }
    }

    _prevUseDualQuaternionSkinning = useDualQuaternionSkinning;
    _shapeKey = builder.build();
}

ItemKey ModelMeshPartPayload::getKey() const {
    return _itemKey;
}

Item::Bound ModelMeshPartPayload::getBound(RenderArgs* args) const {
    graphics::MaterialPointer material = _drawMaterials.empty() ? nullptr : _drawMaterials.top().material;
    if (material && material->isProcedural() && material->isReady()) {
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(_drawMaterials.top().material);
        if (procedural->hasVertexShader() && procedural->hasBoundOperator()) {
            return procedural->getBound(args);
        }
    }

    auto worldBound = _adjustedLocalBound;
    auto parentTransform = _parentTransform;
    if (args) {
        parentTransform.setRotation(BillboardModeHelpers::getBillboardRotation(parentTransform.getTranslation(), parentTransform.getRotation(), _billboardMode,
            args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    }

    worldBound.transform(parentTransform);
    return worldBound;
}

ShapeKey ModelMeshPartPayload::getShapeKey() const {
    return _shapeKey;
}

void ModelMeshPartPayload::render(RenderArgs* args) {
    PerformanceTimer perfTimer("ModelMeshPartPayload::render");

    if (!args || (args->_renderMode == RenderArgs::RenderMode::DEFAULT_RENDER_MODE && _cauterized)) {
        return;
    }

    gpu::Batch& batch = *(args->_batch);

    Transform transform = _parentTransform;
    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));

    Transform modelTransform = transform.worldTransform(_localTransform);
    bindTransform(batch, modelTransform, args->_renderMode);

    //Bind the index buffer and vertex buffer and Blend shapes if needed
    bindMesh(batch);

    // IF deformed pass the mesh key
    auto drawcallInfo = (uint16_t) (((_isBlendShaped && _meshBlendshapeBuffer && args->_enableBlendshape) << 0) | ((_isSkinned && args->_enableSkinning) << 1));
    if (drawcallInfo) {
        batch.setDrawcallUniform(drawcallInfo);
    }

    if (_shapeKey.hasOwnPipeline()) {
        if (!(enableMaterialProceduralShaders)) {
            return;
        }
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(_drawMaterials.top().material);
        auto& schema = _drawMaterials.getSchemaBuffer().get<graphics::MultiMaterial::Schema>();
        glm::vec4 outColor = glm::vec4(ColorUtils::tosRGBVec3(schema._albedo), schema._opacity);
        outColor = procedural->getColor(outColor);
        procedural->prepare(batch, transform.getTranslation(), transform.getScale(), transform.getRotation(), _created,
                            ProceduralProgramKey(outColor.a < 1.0f, _shapeKey.isDeformed(), _shapeKey.isDualQuatSkinned()));
        batch._glColor4f(outColor.r, outColor.g, outColor.b, outColor.a);
    } else {
        // apply material properties
        if (RenderPipelines::bindMaterials(_drawMaterials, batch, args->_renderMode, args->_enableTexturing)) {
            args->_details._materialSwitches++;
        }
    }

    // Draw!
    {
        PerformanceTimer perfTimer("batch.drawIndexed()");
        drawCall(batch);
    }

    const int INDICES_PER_TRIANGLE = 3;
    args->_details._trianglesRendered += _drawPart._numIndices / INDICES_PER_TRIANGLE;
}

bool ModelMeshPartPayload::passesZoneOcclusionTest(const std::unordered_set<QUuid>& containingZones) const {
    if (!_renderWithZones.isEmpty()) {
        if (!containingZones.empty()) {
            for (auto renderWithZone : _renderWithZones) {
                if (containingZones.find(renderWithZone) != containingZones.end()) {
                    return true;
                }
            }
        }
        return false;
    }
    return true;
}

void ModelMeshPartPayload::setBlendshapeBuffer(const std::unordered_map<int, gpu::BufferPointer>& blendshapeBuffers, const QVector<int>& blendedMeshSizes) {
    if (_meshIndex < blendedMeshSizes.length() && blendedMeshSizes.at(_meshIndex) == _meshNumVertices) {
        auto blendshapeBuffer = blendshapeBuffers.find(_meshIndex);
        if (blendshapeBuffer != blendshapeBuffers.end()) {
            _meshBlendshapeBuffer = blendshapeBuffer->second;
            if (_isSkinned || (_isBlendShaped && _meshBlendshapeBuffer)) {
                ShapeKey::Builder builder(_shapeKey);
                builder.withDeformed();
                if (_prevUseDualQuaternionSkinning) {
                    builder.withDualQuatSkinned();
                }
                _shapeKey = builder.build();
            }
        }
    }
}

namespace render {
template <> const ItemKey payloadGetKey(const ModelMeshPartPayload::Pointer& payload) {
    if (payload) {
        return payload->getKey();
    }
    return ItemKey::Builder::opaqueShape(); // for lack of a better idea
}

template <> const Item::Bound payloadGetBound(const ModelMeshPartPayload::Pointer& payload, RenderArgs* args) {
    if (payload) {
        return payload->getBound(args);
    }
    return Item::Bound();
}

template <> const ShapeKey shapeGetShapeKey(const ModelMeshPartPayload::Pointer& payload) {
    if (payload) {
        return payload->getShapeKey();
    }
    return ShapeKey::Builder::invalid();
}

template <> void payloadRender(const ModelMeshPartPayload::Pointer& payload, RenderArgs* args) {
    return payload->render(args);
}

template <> bool payloadPassesZoneOcclusionTest(const ModelMeshPartPayload::Pointer& payload, const std::unordered_set<QUuid>& containingZones) {
    if (payload) {
        return payload->passesZoneOcclusionTest(containingZones);
    }
    return false;
}
}
