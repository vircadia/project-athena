
//
//  RenderForwardTask.cpp
//  render-utils/src/
//
//  Created by Zach Pomerantz on 12/13/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderForwardTask.h"

#include <PerfStat.h>
#include <PathUtils.h>
#include <ViewFrustum.h>
#include <gpu/Context.h>
#include <gpu/Texture.h>
#include <graphics/ShaderConstants.h>
#include <render/ShapePipeline.h>

#include <render/FilterTask.h>

#include "RenderHifi.h"
#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"
#include "ZoneRenderer.h"
#include "FadeEffect.h"
#include "ToneMapAndResampleTask.h"
#include "BackgroundStage.h"
#include "FramebufferCache.h"
#include "TextureCache.h"
#include "RenderCommonTask.h"
#include "RenderHUDLayerTask.h"

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

using namespace render;

extern void initForwardPipelines(ShapePlumber& plumber);

void PreparePrimaryFramebufferMSAAConfig::setResolutionScale(float scale) {
    const float SCALE_RANGE_MIN = 0.1f;
    const float SCALE_RANGE_MAX = 2.0f;
    resolutionScale = std::max(SCALE_RANGE_MIN, std::min(SCALE_RANGE_MAX, scale));
}

void PreparePrimaryFramebufferMSAAConfig::setNumSamples(int num) {
    numSamples = std::max(1, std::min(32, num));
    emit dirty();
}

void RenderForwardTask::configure(const Config& config) {
    // Propagate resolution scale to sub jobs who need it
    auto preparePrimaryBufferConfig = config.getConfig<PreparePrimaryFramebufferMSAA>("PreparePrimaryBufferForward");
    assert(preparePrimaryBufferConfig);
    preparePrimaryBufferConfig->setResolutionScale(config.resolutionScale);
}

void RenderForwardTask::build(JobModel& task, const render::Varying& input, render::Varying& output, uint8_t transformOffset) {
    task.addJob<SetRenderMethod>("SetRenderMethodTask", render::Args::FORWARD);

    // Prepare the ShapePipelines
    auto fadeEffect = DependencyManager::get<FadeEffect>();
    ShapePlumberPointer shapePlumber = std::make_shared<ShapePlumber>();
    initForwardPipelines(*shapePlumber);

    uint backgroundViewTransformSlot = render::RenderEngine::TS_BACKGROUND_VIEW + transformOffset;
    uint mainViewTransformSlot = render::RenderEngine::TS_MAIN_VIEW + transformOffset;

    // Unpack inputs
    const auto& inputs = input.get<Input>();
    
    // Separate the fetched items
    const auto& fetchedItems = inputs.get0();

        const auto& items = fetchedItems.get0();

            // Extract opaques / transparents / lights / metas / layered / background
            const auto& opaques = items[RenderFetchCullSortTask::OPAQUE_SHAPE];
            const auto& transparents = items[RenderFetchCullSortTask::TRANSPARENT_SHAPE];
            const auto& metas = items[RenderFetchCullSortTask::META];
            const auto& inFrontOpaque = items[RenderFetchCullSortTask::LAYER_FRONT_OPAQUE_SHAPE];
            const auto& inFrontTransparent = items[RenderFetchCullSortTask::LAYER_FRONT_TRANSPARENT_SHAPE];
            const auto& hudOpaque = items[RenderFetchCullSortTask::LAYER_HUD_OPAQUE_SHAPE];
            const auto& hudTransparent = items[RenderFetchCullSortTask::LAYER_HUD_TRANSPARENT_SHAPE];

    // Lighting model comes next, the big configuration of the view
    const auto& lightingModel = inputs[1];

    // Extract the Lighting Stages Current frame ( and zones)
    const auto& lightingStageInputs = inputs.get2();
        // Fetch the current frame stacks from all the stages
        const auto currentStageFrames = lightingStageInputs.get0();
            const auto lightFrame = currentStageFrames[0];
            const auto backgroundFrame = currentStageFrames[1];
            const auto hazeFrame = currentStageFrames[2];
 
        const auto& zones = lightingStageInputs[1];

    // First job, alter faded
    fadeEffect->build(task, opaques);


    // GPU jobs: Start preparing the main framebuffer
    const auto scaledPrimaryFramebuffer = task.addJob<PreparePrimaryFramebufferMSAA>("PreparePrimaryBufferForward");

    // Prepare deferred, generate the shared Deferred Frame Transform. Only valid with the scaled frame buffer
    const auto deferredFrameTransform = task.addJob<GenerateDeferredFrameTransform>("DeferredFrameTransform", mainViewTransformSlot);

    // Prepare Forward Framebuffer pass 
    const auto prepareForwardInputs = PrepareForward::Inputs(scaledPrimaryFramebuffer, lightFrame).asVarying();
    task.addJob<PrepareForward>("PrepareForward", prepareForwardInputs);

    // draw a stencil mask in hidden regions of the framebuffer.
    task.addJob<PrepareStencil>("PrepareStencil", scaledPrimaryFramebuffer);

    // Draw opaques forward
    const auto opaqueInputs = DrawForward::Inputs(opaques, lightingModel, hazeFrame).asVarying();
    task.addJob<DrawForward>("DrawOpaques", opaqueInputs, shapePlumber, true, mainViewTransformSlot);

    // Similar to light stage, background stage has been filled by several potential render items and resolved for the frame in this job
    const auto backgroundInputs = DrawBackgroundStage::Inputs(lightingModel, backgroundFrame, hazeFrame).asVarying();
    task.addJob<DrawBackgroundStage>("DrawBackgroundForward", backgroundInputs, backgroundViewTransformSlot);

    // Draw transparent objects forward
    const auto transparentInputs = DrawForward::Inputs(transparents, lightingModel, hazeFrame).asVarying();
    task.addJob<DrawForward>("DrawTransparents", transparentInputs, shapePlumber, false, mainViewTransformSlot);

     // Layered
    const auto nullJitter = Varying(glm::vec2(0.0f, 0.0f));
    const auto inFrontOpaquesInputs = DrawLayered3D::Inputs(inFrontOpaque, deferredFrameTransform, lightingModel, hazeFrame).asVarying();
    const auto inFrontTransparentsInputs = DrawLayered3D::Inputs(inFrontTransparent, deferredFrameTransform, lightingModel, hazeFrame).asVarying();
    task.addJob<DrawLayered3D>("DrawInFrontOpaque", inFrontOpaquesInputs, shapePlumber, true, false, mainViewTransformSlot);
    task.addJob<DrawLayered3D>("DrawInFrontTransparent", inFrontTransparentsInputs, shapePlumber, false, false, mainViewTransformSlot);

    {  // Debug the bounds of the rendered items, still look at the zbuffer

        task.addJob<DrawBounds>("DrawMetaBounds", metas, mainViewTransformSlot);
        task.addJob<DrawBounds>("DrawBounds", opaques, mainViewTransformSlot);
        task.addJob<DrawBounds>("DrawTransparentBounds", transparents, mainViewTransformSlot);

        task.addJob<DrawBounds>("DrawZones", zones, mainViewTransformSlot);
        const auto debugZoneInputs = DebugZoneLighting::Inputs(deferredFrameTransform, lightFrame, backgroundFrame).asVarying();
        task.addJob<DebugZoneLighting>("DrawZoneStack", debugZoneInputs);
    }

    const auto newResolvedFramebuffer = task.addJob<NewFramebuffer>("MakeResolvingFramebuffer", gpu::Element(gpu::SCALAR, gpu::FLOAT, gpu::R11G11B10));

    const auto resolveInputs = ResolveFramebuffer::Inputs(scaledPrimaryFramebuffer, newResolvedFramebuffer).asVarying();
    const auto resolvedFramebuffer = task.addJob<ResolveFramebuffer>("Resolve", resolveInputs);

    const auto destFramebuffer = static_cast<gpu::FramebufferPointer>(nullptr);

    const auto toneMappingInputs = ToneMapAndResample::Input(resolvedFramebuffer, destFramebuffer).asVarying();
    const auto toneMappedBuffer = task.addJob<ToneMapAndResample>("ToneMapping", toneMappingInputs);
    // HUD Layer
    const auto renderHUDLayerInputs = RenderHUDLayerTask::Input(toneMappedBuffer, lightingModel, hudOpaque, hudTransparent, hazeFrame, deferredFrameTransform).asVarying();
    task.addJob<RenderHUDLayerTask>("RenderHUDLayer", renderHUDLayerInputs, shapePlumber, mainViewTransformSlot);
}

gpu::FramebufferPointer PreparePrimaryFramebufferMSAA::createFramebuffer(const char* name, const glm::uvec2& frameSize, int numSamples) {
    gpu::FramebufferPointer framebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create(name));

    auto defaultSampler = gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR);

    auto colorFormat = gpu::Element(gpu::SCALAR, gpu::FLOAT, gpu::R11G11B10);
    auto colorTexture =
        gpu::Texture::createRenderBufferMultisample(colorFormat, frameSize.x, frameSize.y, numSamples, defaultSampler);
    framebuffer->setRenderBuffer(0, colorTexture);

    auto depthFormat = gpu::Element(gpu::SCALAR, gpu::UINT32, gpu::DEPTH_STENCIL);  // Depth24_Stencil8 texel format
    auto depthTexture =
        gpu::Texture::createRenderBufferMultisample(depthFormat, frameSize.x, frameSize.y, numSamples, defaultSampler);
    framebuffer->setDepthStencilBuffer(depthTexture, depthFormat);

    return framebuffer;
}

void PreparePrimaryFramebufferMSAA::configure(const Config& config) {
    _resolutionScale = config.getResolutionScale();
    _numSamples = config.getNumSamples();
}

void PreparePrimaryFramebufferMSAA::run(const RenderContextPointer& renderContext, gpu::FramebufferPointer& framebuffer) {
    glm::uvec2 frameSize(renderContext->args->_viewport.z, renderContext->args->_viewport.w);
    glm::uvec2 scaledFrameSize(glm::vec2(frameSize) * _resolutionScale);

    // Resizing framebuffers instead of re-building them seems to cause issues with threaded rendering
    if (!_framebuffer || (_framebuffer->getSize() != scaledFrameSize) || (_framebuffer->getNumSamples() != _numSamples)) {
        _framebuffer = createFramebuffer("forward", scaledFrameSize, _numSamples);
    }

    framebuffer = _framebuffer;

    // Set viewport for the rest of the scaled passes
    renderContext->args->_viewport.z = scaledFrameSize.x;
    renderContext->args->_viewport.w = scaledFrameSize.y;
}

void PrepareForward::run(const RenderContextPointer& renderContext, const Inputs& inputs) {
    RenderArgs* args = renderContext->args;

    auto primaryFramebuffer = inputs.get0();
    auto lightStageFrame = inputs.get1();

    gpu::doInBatch("RenderForward::Draw::run", args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;

        batch.enableStereo(false);
        batch.setViewportTransform(args->_viewport);
        batch.setStateScissorRect(args->_viewport);

        batch.setFramebuffer(primaryFramebuffer);
        batch.clearFramebuffer(gpu::Framebuffer::BUFFER_COLOR0 | gpu::Framebuffer::BUFFER_DEPTH |
            gpu::Framebuffer::BUFFER_STENCIL,
            vec4(vec3(0), 0), 1.0, 0, true);

        graphics::LightPointer keySunLight;
        auto lightStage = args->_scene->getStage<LightStage>();
        if (lightStage) {
            keySunLight = lightStage->getCurrentKeyLight(*lightStageFrame);
        }

        graphics::LightPointer keyAmbiLight;
        if (lightStage) {
            keyAmbiLight = lightStage->getCurrentAmbientLight(*lightStageFrame);
        }

        if (keySunLight) {
            batch.setUniformBuffer(gr::Buffer::KeyLight, keySunLight->getLightSchemaBuffer());
        }

        if (keyAmbiLight) {
            batch.setUniformBuffer(gr::Buffer::AmbientLight, keyAmbiLight->getAmbientSchemaBuffer());

            if (keyAmbiLight->getAmbientMap()) {
                batch.setResourceTexture(ru::Texture::Skybox, keyAmbiLight->getAmbientMap());
            }
        }
    });
}

void DrawForward::run(const RenderContextPointer& renderContext, const Inputs& inputs) {
    RenderArgs* args = renderContext->args;

    const auto& inItems = inputs.get0();
    const auto& lightingModel = inputs.get1();
    const auto& hazeFrame = inputs.get2();

    graphics::HazePointer haze;
    const auto& hazeStage = renderContext->args->_scene->getStage<HazeStage>();
    if (hazeStage && hazeFrame->_hazes.size() > 0) {
        haze = hazeStage->getHaze(hazeFrame->_hazes.front());
    }

    gpu::doInBatch("DrawForward::run", args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;

        // Setup projection
        batch.setSavedViewProjectionTransform(_transformSlot);
        batch.setModelTransform(Transform());

        // Setup lighting model for all items;
        batch.setUniformBuffer(ru::Buffer::LightModel, lightingModel->getParametersBuffer());
        batch.setResourceTexture(ru::Texture::AmbientFresnel, lightingModel->getAmbientFresnelLUT());

        if (haze) {
            batch.setUniformBuffer(graphics::slot::buffer::Buffer::HazeParams, haze->getHazeParametersBuffer());
        }

        // From the lighting model define a global shapeKey ORED with individiual keys
        ShapeKey::Builder keyBuilder;
        if (lightingModel->isWireframeEnabled()) {
            keyBuilder.withWireframe();
        }
        ShapeKey globalKey = keyBuilder.build();
        args->_globalShapeKey = globalKey._flags.to_ulong();

        // Render items
        if (_opaquePass) {
            renderStateSortShapes(renderContext, _shapePlumber, inItems, -1, globalKey);
        } else {
            renderShapes(renderContext, _shapePlumber, inItems, -1, globalKey);
        }

        args->_batch = nullptr;
        args->_globalShapeKey = 0;
    });
}


