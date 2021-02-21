//
//  Created by Bradley Austin Davis on 2018/01/09
//  Copyright 2013-2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderCommonTask.h"

#include <gpu/Context.h>
#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include "DeferredLightingEffect.h"
#include "RenderUtilsLogging.h"

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}


using namespace render;

void BeginGPURangeTimer::run(const render::RenderContextPointer& renderContext, gpu::RangeTimerPointer& timer) {
    timer = _gpuTimer;
    gpu::doInBatch("BeginGPURangeTimer", renderContext->args->_context, [&](gpu::Batch& batch) {
        _gpuTimer->begin(batch);
        batch.pushProfileRange(timer->name().c_str());
    });
}

void EndGPURangeTimer::run(const render::RenderContextPointer& renderContext, const gpu::RangeTimerPointer& timer) {
    gpu::doInBatch("EndGPURangeTimer", renderContext->args->_context, [&](gpu::Batch& batch) {
        batch.popProfileRange();
        timer->end(batch);
    });
    
    auto config = std::static_pointer_cast<Config>(renderContext->jobConfig);
    config->setGPUBatchRunTime(timer->getGPUAverage(), timer->getBatchAverage());
}

DrawLayered3D::DrawLayered3D(const render::ShapePlumberPointer& shapePlumber, bool opaque, bool jitter, unsigned int transformSlot) :
    _shapePlumber(shapePlumber),
    _transformSlot(transformSlot),
    _opaquePass(opaque),
    _isJitterEnabled(jitter) {
}

void DrawLayered3D::run(const RenderContextPointer& renderContext, const Inputs& inputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());

    auto config = std::static_pointer_cast<Config>(renderContext->jobConfig);

    const auto& inItems = inputs.get0();
    const auto& frameTransform = inputs.get1();
    const auto& lightingModel = inputs.get2();
    const auto& hazeFrame = inputs.get3();
    
    config->setNumDrawn((int)inItems.size());
    emit config->numDrawnChanged();

    RenderArgs* args = renderContext->args;

    graphics::HazePointer haze;
    const auto& hazeStage = renderContext->args->_scene->getStage<HazeStage>();
    if (hazeStage && hazeFrame->_hazes.size() > 0) {
        // We use _hazes.back() here because the last haze object will always have haze disabled.
        haze = hazeStage->getHaze(hazeFrame->_hazes.back());
    }

    // Clear the framebuffer without stereo
    // Needs to be distinct from the other batch because using the clear call 
    // while stereo is enabled triggers a warning
    if (_opaquePass) {
        gpu::doInBatch("DrawLayered3D::run::clear", args->_context, [&](gpu::Batch& batch) {
            batch.enableStereo(false);
            batch.clearDepthFramebuffer(true, false);
        });
    }

    if (!inItems.empty()) {
        // Render the items
        gpu::doInBatch("DrawLayered3D::main", args->_context, [&](gpu::Batch& batch) {
            PROFILE_RANGE_BATCH(batch, "DrawLayered3D::main");
            args->_batch = &batch;
            batch.setViewportTransform(args->_viewport);
            batch.setStateScissorRect(args->_viewport);

            batch.setProjectionJitterEnabled(_isJitterEnabled);
            batch.setSavedViewProjectionTransform(_transformSlot);

            // Setup lighting model for all items;
            batch.setUniformBuffer(ru::Buffer::LightModel, lightingModel->getParametersBuffer());
            batch.setResourceTexture(ru::Texture::AmbientFresnel, lightingModel->getAmbientFresnelLUT());
            batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, frameTransform->getFrameTransformBuffer());

            if (haze) {
                batch.setUniformBuffer(graphics::slot::buffer::Buffer::HazeParams, haze->getHazeParametersBuffer());
            }

            if (_opaquePass) {
                renderStateSortShapes(renderContext, _shapePlumber, inItems, _maxDrawn);
            } else {
                renderShapes(renderContext, _shapePlumber, inItems, _maxDrawn);
            }
            args->_batch = nullptr;
        });
    }
}

void Blit::run(const RenderContextPointer& renderContext, const gpu::FramebufferPointer& srcFramebuffer) {
    assert(renderContext->args);
    assert(renderContext->args->_context);

    RenderArgs* renderArgs = renderContext->args;
    auto blitFbo = renderArgs->_blitFramebuffer;

    if (!blitFbo) {
        qCWarning(renderutils) << "Blit::run - no blit frame buffer.";
        return;
    }

    // Determine size from viewport
    int width = renderArgs->_viewport.z;
    int height = renderArgs->_viewport.w;

    // Blit primary to blit FBO
    auto primaryFbo = srcFramebuffer;

    gpu::doInBatch("Blit", renderArgs->_context, [&](gpu::Batch& batch) {
        batch.setFramebuffer(blitFbo);

        gpu::Vec4i rect;
        rect.z = width;
        rect.w = height;

        batch.blit(primaryFbo, rect, blitFbo, rect);
    });
}

NewFramebuffer::NewFramebuffer(gpu::Element pixelFormat) {
    _pixelFormat = pixelFormat;
}

void NewFramebuffer::run(const render::RenderContextPointer& renderContext, Output& output) {
    RenderArgs* args = renderContext->args;
    glm::uvec2 frameSize(args->_viewport.z, args->_viewport.w);
    output.reset();

    if (_outputFramebuffer && _outputFramebuffer->getSize() != frameSize) {
        _outputFramebuffer.reset();
    }

    if (!_outputFramebuffer) {
        _outputFramebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("newFramebuffer.out"));
        auto colorFormat = _pixelFormat;
        auto defaultSampler = gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR);
        auto colorTexture = gpu::Texture::createRenderBuffer(colorFormat, frameSize.x, frameSize.y, gpu::Texture::SINGLE_MIP, defaultSampler);
        _outputFramebuffer->setRenderBuffer(0, colorTexture);
    }

    output = _outputFramebuffer;
}

void NewOrDefaultFramebuffer::run(const render::RenderContextPointer& renderContext, const Input& input, Output& output) {
    RenderArgs* args = renderContext->args;
    // auto frameSize = input;
    glm::uvec2 frameSize(args->_viewport.z, args->_viewport.w);
    output.reset();

    // First if the default Framebuffer is the correct size then use it
    auto destBlitFbo = args->_blitFramebuffer;
    if (destBlitFbo && destBlitFbo->getSize() == frameSize) {
        output = destBlitFbo;
        return;
    }

    // Else use the lodal Framebuffer
    if (_outputFramebuffer && _outputFramebuffer->getSize() != frameSize) {
        _outputFramebuffer.reset();
    }

    if (!_outputFramebuffer) {
        _outputFramebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("newOrDefaultFramebuffer.out"));
        auto colorFormat = gpu::Element::COLOR_SRGBA_32;
        auto defaultSampler = gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR);
        auto colorTexture = gpu::Texture::createRenderBuffer(colorFormat, frameSize.x, frameSize.y, gpu::Texture::SINGLE_MIP, defaultSampler);
        _outputFramebuffer->setRenderBuffer(0, colorTexture);
    }

    output = _outputFramebuffer;
}

void ResolveFramebuffer::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    RenderArgs* args = renderContext->args;
    auto srcFbo = inputs.get0();
    auto destFbo = inputs.get1();

    if (!destFbo) {
        destFbo = args->_blitFramebuffer;
    }
    outputs = destFbo;

    // Check valid src and dest
    if (!srcFbo || !destFbo) {
        return;
    }
    
    // Check valid size for sr and dest
    auto frameSize(srcFbo->getSize());
    if (destFbo->getSize() != frameSize) {
        return;
    }

    gpu::Vec4i rectSrc;
    rectSrc.z = frameSize.x;
    rectSrc.w = frameSize.y;
    gpu::doInBatch("Resolve", args->_context, [&](gpu::Batch& batch) { 
        batch.blit(srcFbo, rectSrc, destFbo, rectSrc);
    });
}

 void ExtractFrustums::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& output) {
    assert(renderContext->args);
    assert(renderContext->args->_context);

    RenderArgs* args = renderContext->args;

    const auto& shadowFrame = inputs;

    // Return view frustum
    auto& viewFrustum = output[VIEW_FRUSTUM].edit<ViewFrustumPointer>();
    if (!viewFrustum) {
        viewFrustum = std::make_shared<ViewFrustum>(args->getViewFrustum());
    } else {
        *viewFrustum = args->getViewFrustum();
    }

    // Return shadow frustum
    LightStage::ShadowPointer globalShadow;
    if (shadowFrame && !shadowFrame->_objects.empty() && shadowFrame->_objects[0]) {
        globalShadow = shadowFrame->_objects[0];
    }
    for (auto i = 0; i < SHADOW_CASCADE_FRUSTUM_COUNT; i++) {
        auto& shadowFrustum = output[SHADOW_CASCADE0_FRUSTUM+i].edit<ViewFrustumPointer>();
        if (globalShadow && i<(int)globalShadow->getCascadeCount()) {
            auto& cascade = globalShadow->getCascade(i);
            shadowFrustum = cascade.getFrustum();
        } else {
            shadowFrustum.reset();
        }
    }
}

