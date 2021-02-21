//
//  CauterizedMeshPartPayload.cpp
//  interface/src/renderer
//
//  Created by Andrew Meadows 2017.01.17
//  Copyright 2017 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "CauterizedMeshPartPayload.h"

#include <PerfStat.h>
#include <graphics/ShaderConstants.h>

#include "CauterizedModel.h"

using namespace render;

CauterizedMeshPartPayload::CauterizedMeshPartPayload(ModelPointer model, int meshIndex, int partIndex, int shapeIndex,
                                                     const Transform& transform, const uint64_t& created)
    : ModelMeshPartPayload(model, meshIndex, partIndex, shapeIndex, transform, created) {}

void CauterizedMeshPartPayload::updateClusterBuffer(const std::vector<glm::mat4>& clusterMatrices,
                                                    const std::vector<glm::mat4>& cauterizedClusterMatrices) {
    ModelMeshPartPayload::updateClusterBuffer(clusterMatrices);

    if (cauterizedClusterMatrices.size() > 1) {
        if (!_cauterizedClusterBuffer) {
            _cauterizedClusterBuffer = std::make_shared<gpu::Buffer>(cauterizedClusterMatrices.size() * sizeof(glm::mat4),
                (const gpu::Byte*) cauterizedClusterMatrices.data());
        } else {
            _cauterizedClusterBuffer->setSubData(0, cauterizedClusterMatrices.size() * sizeof(glm::mat4),
                (const gpu::Byte*) cauterizedClusterMatrices.data());
        }
    }
}

void CauterizedMeshPartPayload::updateClusterBuffer(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions,
                                                    const std::vector<Model::TransformDualQuaternion>& cauterizedClusterDualQuaternions) {
    ModelMeshPartPayload::updateClusterBuffer(clusterDualQuaternions);

    if (cauterizedClusterDualQuaternions.size() > 1) {
        if (!_cauterizedClusterBuffer) {
            _cauterizedClusterBuffer = std::make_shared<gpu::Buffer>(cauterizedClusterDualQuaternions.size() * sizeof(Model::TransformDualQuaternion),
                (const gpu::Byte*) cauterizedClusterDualQuaternions.data());
        } else {
            _cauterizedClusterBuffer->setSubData(0, cauterizedClusterDualQuaternions.size() * sizeof(Model::TransformDualQuaternion),
                (const gpu::Byte*) cauterizedClusterDualQuaternions.data());
        }
    }
}

void CauterizedMeshPartPayload::updateTransformForCauterizedMesh(const Transform& modelTransform, const Model::MeshState& meshState, bool useDualQuaternionSkinning) {
    Transform renderTransform = modelTransform;
    if (useDualQuaternionSkinning) {
        if (meshState.clusterDualQuaternions.size() == 1 || meshState.clusterDualQuaternions.size() == 2) {
            const auto& dq = meshState.clusterDualQuaternions[0];
            Transform transform(dq.getRotation(),
                                dq.getScale(),
                                dq.getTranslation());
            renderTransform = modelTransform.worldTransform(Transform(transform));
        }
    } else {
        if (meshState.clusterMatrices.size() == 1 || meshState.clusterMatrices.size() == 2) {
            renderTransform = modelTransform.worldTransform(Transform(meshState.clusterMatrices[0]));
        }
    }

    _cauterizedTransform = renderTransform;
}

void CauterizedMeshPartPayload::bindTransform(gpu::Batch& batch, const Transform& transform, RenderArgs::RenderMode renderMode) const {
    bool useCauterizedMesh = (renderMode != RenderArgs::RenderMode::SHADOW_RENDER_MODE && renderMode != RenderArgs::RenderMode::SECONDARY_CAMERA_RENDER_MODE) && _enableCauterization;
    if (useCauterizedMesh) {
        if (_cauterizedClusterBuffer) {
            batch.setUniformBuffer(graphics::slot::buffer::Skinning, _cauterizedClusterBuffer);
        }
        batch.setModelTransform(_cauterizedTransform, _prevRenderTransform);
        if (renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
            _prevRenderTransform = _cauterizedTransform;
        }
    } else {
        ModelMeshPartPayload::bindTransform(batch, transform, renderMode);
    }
}
