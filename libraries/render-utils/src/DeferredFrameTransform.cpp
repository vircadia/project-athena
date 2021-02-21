//
//  DeferredFrameTransform.cpp
//  libraries/render-utils/src/
//
//  Created by Sam Gateau 6/3/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "DeferredFrameTransform.h"

#include "gpu/Context.h"
#include "render/Engine.h"

DeferredFrameTransform::DeferredFrameTransform() {
    FrameTransform frameTransform;
    _frameTransformBuffer = gpu::BufferView(std::make_shared<gpu::Buffer>(sizeof(FrameTransform), (const gpu::Byte*) &frameTransform));
}

void DeferredFrameTransform::update(RenderArgs* args) {

    // Update the depth info with near and far (same for stereo)
    auto nearZ = args->getViewFrustum().getNearClip();
    auto farZ = args->getViewFrustum().getFarClip();

    auto& frameTransformBuffer = _frameTransformBuffer.edit<FrameTransform>();
    frameTransformBuffer.infos.depthInfo = glm::vec4(nearZ * farZ, farZ - nearZ, -farZ, 0.0f);
    frameTransformBuffer.infos.pixelInfo = args->_viewport;

    args->getViewFrustum().evalProjectionMatrix(frameTransformBuffer.infos.projectionMono);

    // Running in stereo ?
    bool isStereo = args->isStereo();
    if (!isStereo) {
        frameTransformBuffer.infos.stereoInfo = glm::vec4(0.0f, (float)args->_viewport.z, 0.0f, 0.0f);
        frameTransformBuffer.infos.invPixelInfo = glm::vec4(1.0f / args->_viewport.z, 1.0f / args->_viewport.w, 0.0f, 0.0f);
    } else {
        frameTransformBuffer.infos.pixelInfo.z *= 0.5f;
        frameTransformBuffer.infos.stereoInfo = glm::vec4(1.0f, (float)(args->_viewport.z >> 1), 0.0f, 1.0f);
        frameTransformBuffer.infos.invPixelInfo = glm::vec4(2.0f / (float)(args->_viewport.z), 1.0f / args->_viewport.w, 0.0f, 0.0f);
    }
}

void GenerateDeferredFrameTransform::run(const render::RenderContextPointer& renderContext, Output& frameTransform) {
    if (!frameTransform) {
        frameTransform = std::make_shared<DeferredFrameTransform>();
    }

    RenderArgs* args = renderContext->args;
    frameTransform->update(args);

    gpu::doInBatch("GenerateDeferredFrameTransform::run", args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;

        glm::mat4 projMat;
        Transform viewMat;
        args->getViewFrustum().evalProjectionMatrix(projMat);
        args->getViewFrustum().evalViewTransform(viewMat);
        batch.setProjectionTransform(projMat);
        batch.setViewTransform(viewMat);
        // This is the main view / projection transform that will be reused later on
        batch.saveViewProjectionTransform(_transformSlot);
        // Copy it to the deferred transform for the lighting pass
        batch.copySavedViewProjectionTransformToBuffer(_transformSlot, frameTransform->getFrameTransformBuffer()._buffer,
                                                       sizeof(DeferredFrameTransform::DeferredFrameInfo));
    });
}
