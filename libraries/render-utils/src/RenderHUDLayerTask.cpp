//
//  Created by Sam Gateau on 2019/06/14
//  Copyright 2013-2019 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "RenderHUDLayerTask.h"

#include <gpu/Context.h>
#include "RenderCommonTask.h"

using namespace render;

void CompositeHUD::run(const RenderContextPointer& renderContext, const gpu::FramebufferPointer& inputs) {
    assert(renderContext->args);
    assert(renderContext->args->_context);

    // We do not want to render HUD elements in secondary camera
    if (nsightActive() || renderContext->args->_renderMode == RenderArgs::RenderMode::SECONDARY_CAMERA_RENDER_MODE) {
        return;
    }

    // Grab the HUD texture
#if !defined(DISABLE_QML)
    gpu::doInBatch("CompositeHUD", renderContext->args->_context, [&](gpu::Batch& batch) {
        PROFILE_RANGE_BATCH(batch, "HUD");
        batch.setSavedViewProjectionTransform(_transformSlot);
        if (inputs) {
            batch.setFramebuffer(inputs);
        }
        if (renderContext->args->_hudOperator) {
            renderContext->args->_hudOperator(batch, renderContext->args->_hudTexture);
        }
    });
#endif
}

void RenderHUDLayerTask::build(JobModel& task, const render::Varying& input, render::Varying& output,
        render::ShapePlumberPointer shapePlumber, uint transformSlot) {
    const auto& inputs = input.get<Input>();

    const auto& primaryFramebuffer = inputs[0];
    const auto& lightingModel = inputs[1];
    const auto& hudOpaque = inputs[2];
    const auto& hudTransparent = inputs[3];
    const auto& hazeFrame = inputs[4];
    const auto& deferredFrameTransform = inputs[5];

    // Composite the HUD and HUD overlays
    task.addJob<CompositeHUD>("HUD", primaryFramebuffer, transformSlot);

    // And HUD Layer objects
    const auto nullJitter = Varying(glm::vec2(0.0f, 0.0f));
    const auto hudOpaquesInputs = DrawLayered3D::Inputs(hudOpaque, deferredFrameTransform, lightingModel, hazeFrame).asVarying();
    const auto hudTransparentsInputs = DrawLayered3D::Inputs(hudTransparent, deferredFrameTransform, lightingModel, hazeFrame).asVarying();
    task.addJob<DrawLayered3D>("DrawHUDOpaque", hudOpaquesInputs, shapePlumber, true, false, transformSlot);
    task.addJob<DrawLayered3D>("DrawHUDTransparent", hudTransparentsInputs, shapePlumber, false, false, transformSlot);
}
