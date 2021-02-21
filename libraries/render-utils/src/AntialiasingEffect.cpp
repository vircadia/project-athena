//
//  AntialiasingEffect.cpp
//  libraries/render-utils/src/
//
//  Created by Raffi Bedikian on 8/30/15
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AntialiasingEffect.h"

#include <glm/gtc/random.hpp>

#include <SharedUtil.h>
#include <gpu/Context.h>
#include <shaders/Shaders.h>
#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"
#include "RandomAndNoise.h"

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

gpu::PipelinePointer Antialiasing::_antialiasingPipeline;
gpu::PipelinePointer Antialiasing::_intensityPipeline;
gpu::PipelinePointer Antialiasing::_blendPipeline;
gpu::PipelinePointer Antialiasing::_debugBlendPipeline;

 #define TAA_JITTER_SEQUENCE_LENGTH 16

void AntialiasingSetupConfig::setIndex(int current) {
    _index = (current + TAA_JITTER_SEQUENCE_LENGTH) % TAA_JITTER_SEQUENCE_LENGTH;
    emit dirty();
}

void AntialiasingSetupConfig::setState(int state) {
    _state = (state) % 3;
    switch (_state) {
        case 0: {
            none();
            break;
        }
        case 1: {
            pause();
            break;
        }
        case 2:
        default: {
            play();
            break;
        }
    }
    emit dirty();
}

int AntialiasingSetupConfig::cycleStopPauseRun() {
    _state = (_state + 1) % 3;
    switch (_state) {
        case 0: {
            return none();
            break;
        }
        case 1: {
            return pause();
            break;
        }
        case 2:
        default: {
            return play();
            break;
        }
    }
    return _state;
}

int AntialiasingSetupConfig::prev() {
    setIndex(_index - 1);
    return _index;
}

int AntialiasingSetupConfig::next() {
    setIndex(_index + 1);
    return _index;
}

int AntialiasingSetupConfig::none() {
    _state = 0;
    stop = true;
    freeze = false;
    setIndex(-1);
    return _state;
}

int AntialiasingSetupConfig::pause() {
    _state = 1;
    stop = false;
    freeze = true;
    setIndex(0);
    return _state;
}

int AntialiasingSetupConfig::play() {
    _state = 2;
    stop = false;
    freeze = false;
    setIndex(0);
    return _state;
}

void AntialiasingSetupConfig::setAAMode(int mode) {
    this->mode = glm::clamp(mode, 0, (int)AntialiasingSetupConfig::MODE_COUNT);
    emit dirty();
}

AntialiasingSetup::AntialiasingSetup() {
    _sampleSequence.reserve(TAA_JITTER_SEQUENCE_LENGTH + 1);
    // Fill in with jitter samples
    for (int i = 0; i < TAA_JITTER_SEQUENCE_LENGTH; i++) {
        _sampleSequence.emplace_back(glm::vec2(halton::evaluate<2>(i), halton::evaluate<3>(i)) - vec2(0.5f));
    }
}

void AntialiasingSetup::configure(const Config& config) {
    _isStopped = config.stop;
    _isFrozen = config.freeze;

    if (config.freeze) {
        _freezedSampleIndex = config.getIndex();
    }
    _scale = config.scale;

    _mode = config.mode;
}

void AntialiasingSetup::run(const render::RenderContextPointer& renderContext, Output& output) {
    assert(renderContext->args);
    if (!_isStopped && _mode == AntialiasingSetupConfig::Mode::TAA) {
        RenderArgs* args = renderContext->args;

        gpu::doInBatch("AntialiasingSetup::run", args->_context, [&](gpu::Batch& batch) {
            auto offset = 0;
            auto count = _sampleSequence.size();
            if (_isFrozen) {
                count = 1;
                offset = _freezedSampleIndex;
            }
            batch.setProjectionJitterSequence(_sampleSequence.data() + offset, count);
            batch.setProjectionJitterScale(_scale);
        });
    }

    output = _mode;
}

Antialiasing::Antialiasing(bool isSharpenEnabled) : 
    _isSharpenEnabled{ isSharpenEnabled } {
}

Antialiasing::~Antialiasing() {
    _antialiasingBuffers.clear();
}

const gpu::PipelinePointer& Antialiasing::getAntialiasingPipeline() {   
    if (!_antialiasingPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::taa);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        // Good to go add the brand new pipeline
        _antialiasingPipeline = gpu::Pipeline::create(program, state);
    }
    
    return _antialiasingPipeline;
}

const gpu::PipelinePointer& Antialiasing::getIntensityPipeline() {
    if (!_intensityPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::gpu::program::drawWhite);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        PrepareStencil::testNoAA(*state);

        // Good to go add the brand new pipeline
        _intensityPipeline = gpu::Pipeline::create(program, state);
    }

    return _intensityPipeline;
}

const gpu::PipelinePointer& Antialiasing::getBlendPipeline() {
    if (!_blendPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::aa_blend);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        // Good to go add the brand new pipeline
        _blendPipeline = gpu::Pipeline::create(program, state);
    }
    return _blendPipeline;
}

const gpu::PipelinePointer& Antialiasing::getDebugBlendPipeline() {
    if (!_debugBlendPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::taa_blend);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        PrepareStencil::testNoAA(*state);

        // Good to go add the brand new pipeline
        _debugBlendPipeline = gpu::Pipeline::create(program, state);
    }
    return _debugBlendPipeline;
}

void Antialiasing::configure(const Config& config) {
    _sharpen = config.sharpen * 0.25f;
    if (!_isSharpenEnabled) {
        _sharpen = 0.0f;
    }
    _params.edit().setSharpenedOutput(_sharpen > 0.0f);
    _params.edit().blend = config.blend * config.blend;
    _params.edit().covarianceGamma = config.covarianceGamma;

    _params.edit().setConstrainColor(config.constrainColor);
    _params.edit().setFeedbackColor(config.feedbackColor);

    _params.edit().debugShowVelocityThreshold = config.debugShowVelocityThreshold;

    _params.edit().regionInfo.x = config.debugX;
    _debugFXAAX = config.debugFXAAX;

    _params.edit().setBicubicHistoryFetch(config.bicubicHistoryFetch);

    _params.edit().setDebug(config.debug);
    _params.edit().setShowDebugCursor(config.showCursorPixel);
    _params.edit().setDebugCursor(config.debugCursorTexcoord);
    _params.edit().setDebugOrbZoom(config.debugOrbZoom);

    _params.edit().setShowClosestFragment(config.showClosestFragment);
}


void Antialiasing::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& output) {
    assert(renderContext->args);
    
    RenderArgs* args = renderContext->args;

    auto& deferredFrameTransform = inputs.get0();
    const auto& deferredFrameBuffer = inputs.get1();
    const auto& sourceBuffer = deferredFrameBuffer->getLightingFramebuffer();
    const auto& linearDepthBuffer = inputs.get2();
    const auto& velocityTexture = deferredFrameBuffer->getDeferredVelocityTexture();
    const auto& mode = inputs.get3();

    _params.edit().regionInfo.z = mode == AntialiasingSetupConfig::Mode::TAA ? _debugFXAAX : 0.0f;

    int width = sourceBuffer->getWidth();
    int height = sourceBuffer->getHeight();

    if (_antialiasingBuffers._swapChain && _antialiasingBuffers._swapChain->get(0) && _antialiasingBuffers._swapChain->get(0)->getSize() != uvec2(width, height)) {
        _antialiasingBuffers.clear();
    }

    if (!_antialiasingBuffers._swapChain || !_intensityFramebuffer) {
        std::vector<gpu::FramebufferPointer> antiAliasingBuffers;
        // Link the antialiasing FBO to texture
        auto format = gpu::Element(gpu::VEC4, gpu::HALF, gpu::RGBA);
        auto defaultSampler = gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR, gpu::Sampler::WRAP_CLAMP);
        for (int i = 0; i < 2; i++) {
            antiAliasingBuffers.emplace_back(gpu::Framebuffer::create("antialiasing"));
            const auto& antiAliasingBuffer = antiAliasingBuffers.back();
            _antialiasingBuffers._textures[i] = gpu::Texture::createRenderBuffer(format, width, height, gpu::Texture::SINGLE_MIP, defaultSampler);
            antiAliasingBuffer->setRenderBuffer(0, _antialiasingBuffers._textures[i]);
        }
        _antialiasingBuffers._swapChain = std::make_shared<gpu::FramebufferSwapChain>(antiAliasingBuffers);

        _intensityTexture = gpu::Texture::createRenderBuffer(gpu::Element::COLOR_R_8, width, height, gpu::Texture::SINGLE_MIP, defaultSampler);
        _intensityFramebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("taaIntensity"));
        _intensityFramebuffer->setRenderBuffer(0, _intensityTexture);
        _intensityFramebuffer->setStencilBuffer(deferredFrameBuffer->getDeferredFramebuffer()->getDepthStencilBuffer(), deferredFrameBuffer->getDeferredFramebuffer()->getDepthStencilBufferFormat());
    }

    output = _intensityTexture;

    gpu::doInBatch("Antialiasing::run", args->_context, [&](gpu::Batch& batch) {
        PROFILE_RANGE_BATCH(batch, "TAA");

        batch.enableStereo(false);
        batch.setViewportTransform(args->_viewport);

        // Set the intensity buffer to 1 except when the stencil is masked as NoAA, where it should be 0
        // This is a bit of a hack as it is not possible and not portable to use the stencil value directly
        // as a texture
        batch.setFramebuffer(_intensityFramebuffer);
        batch.clearColorFramebuffer(gpu::Framebuffer::BUFFER_COLOR0, gpu::Vec4(0.0f));
        batch.setResourceTexture(0, nullptr);
        batch.setPipeline(getIntensityPipeline());
        batch.draw(gpu::TRIANGLE_STRIP, 4);

        // TAA step
        if (!_params->isFXAAEnabled()) {
            batch.setResourceFramebufferSwapChainTexture(ru::Texture::TaaHistory, _antialiasingBuffers._swapChain, 0);
            batch.setResourceTexture(ru::Texture::TaaVelocity, velocityTexture);
        } else {
            batch.setResourceTexture(ru::Texture::TaaHistory, nullptr);
            batch.setResourceTexture(ru::Texture::TaaVelocity, nullptr);
        }

        batch.setResourceTexture(ru::Texture::TaaSource, sourceBuffer->getRenderBuffer(0));
        batch.setResourceTexture(ru::Texture::TaaIntensity, _intensityTexture);

         // This is only used during debug
        batch.setResourceTexture(ru::Texture::TaaDepth, linearDepthBuffer->getLinearDepthTexture());

        batch.setUniformBuffer(ru::Buffer::TaaParams, _params);
        batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, deferredFrameTransform->getFrameTransformBuffer());

        batch.setFramebufferSwapChain(_antialiasingBuffers._swapChain, 1);
        batch.setPipeline(getAntialiasingPipeline());
        batch.draw(gpu::TRIANGLE_STRIP, 4);

        // Blend step
        batch.setResourceTexture(ru::Texture::TaaSource, nullptr);

        batch.setFramebuffer(sourceBuffer);
        if (_params->isDebug()) {
            batch.setPipeline(getDebugBlendPipeline());
            batch.setResourceFramebufferSwapChainTexture(ru::Texture::TaaNext, _antialiasingBuffers._swapChain, 1);
        } else {
            batch.setPipeline(getBlendPipeline());
            // Must match the binding point in the aa_blend.slf shader
            batch.setResourceFramebufferSwapChainTexture(0, _antialiasingBuffers._swapChain, 1);
            // Disable sharpen if FXAA
            if (!_blendParamsBuffer) {
                _blendParamsBuffer = std::make_shared<gpu::Buffer>(sizeof(glm::vec4), nullptr);
            }
            _blendParamsBuffer->setSubData(0, _sharpen * _params.get().regionInfo.z);
            batch.setUniformBuffer(0, _blendParamsBuffer);
        }
        batch.draw(gpu::TRIANGLE_STRIP, 4);
        batch.advance(_antialiasingBuffers._swapChain);

        batch.setUniformBuffer(ru::Buffer::TaaParams, nullptr);
        batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, nullptr);

        batch.setResourceTexture(ru::Texture::TaaDepth, nullptr);
        batch.setResourceTexture(ru::Texture::TaaHistory, nullptr);
        batch.setResourceTexture(ru::Texture::TaaVelocity, nullptr);
        batch.setResourceTexture(ru::Texture::TaaNext, nullptr);

        // Reset jitter sequence
        batch.setProjectionJitterSequence(nullptr, 0);
    });
}
