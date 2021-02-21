//
//  BloomEffect.cpp
//  render-utils/src/
//
//  Created by Olivier Prat on 09/25/17.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "BloomEffect.h"

#include <gpu/Context.h>

#include <shaders/Shaders.h>

#include <render/BlurTask.h>
#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"

#define BLOOM_BLUR_LEVEL_COUNT  3

BloomThreshold::BloomThreshold(unsigned int downsamplingFactor) {
    assert(downsamplingFactor > 0);
    auto& params = _parameters.edit();
    params._sampleCount = downsamplingFactor;
    params._offset = (1.0f - downsamplingFactor) * 0.5f;
}

void BloomThreshold::configure(const Config& config) {}

void BloomThreshold::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());

    RenderArgs* args = renderContext->args;

    const auto frameTransform = inputs.get0();
    const auto inputFrameBuffer = inputs.get1();
    const auto bloomFrame = inputs.get2();
    const auto lightingModel = inputs.get3();
    const auto& bloomStage = renderContext->_scene->getStage<BloomStage>();
    graphics::BloomPointer bloom;
    if (bloomStage && bloomFrame->_blooms.size()) {
        bloom = bloomStage->getBloom(bloomFrame->_blooms.front());
    }
    if (!bloom || (lightingModel && !lightingModel->isBloomEnabled())) {
        renderContext->taskFlow.abortTask();
        return;
    }

    assert(inputFrameBuffer->hasColor());

    auto inputBuffer = inputFrameBuffer->getRenderBuffer(0);
    auto bufferSize = gpu::Vec2u(inputBuffer->getDimensions());

    if (!_outputBuffer || _outputBuffer->getSize() != bufferSize) {
        auto colorTexture = gpu::TexturePointer(gpu::Texture::createRenderBuffer(inputBuffer->getTexelFormat(), bufferSize.x, bufferSize.y,
                                                gpu::Texture::SINGLE_MIP, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR_MIP_POINT, gpu::Sampler::WRAP_CLAMP)));

        _outputBuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("BloomThreshold"));
        _outputBuffer->setRenderBuffer(0, colorTexture);
        _outputBuffer->setStencilBuffer(inputFrameBuffer->getDepthStencilBuffer(), inputFrameBuffer->getDepthStencilBufferFormat());

        _parameters.edit()._deltaUV = { 1.0f / bufferSize.x, 1.0f / bufferSize.y };
    }

    if (!_pipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::bloomThreshold);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        _pipeline = gpu::Pipeline::create(program, state);
    }

    glm::ivec4 viewport{ 0, 0, bufferSize.x, bufferSize.y };

    _parameters.edit()._threshold = bloom->getBloomThreshold();

    gpu::doInBatch("BloomThreshold::run", args->_context, [&](gpu::Batch& batch) {
        batch.enableStereo(false);

        batch.setViewportTransform(viewport);
        batch.setProjectionTransform(glm::mat4());
        batch.resetViewTransform();
        batch.setModelTransform(gpu::Framebuffer::evalSubregionTexcoordTransform(bufferSize, viewport));
        batch.setPipeline(_pipeline);

        batch.setFramebuffer(_outputBuffer);
        batch.setResourceTexture(render_utils::slot::texture::BloomColor, inputBuffer);
        batch.setUniformBuffer(render_utils::slot::buffer::BloomParams, _parameters);
        batch.draw(gpu::TRIANGLE_STRIP, 4);
    });

    outputs.edit0() = _outputBuffer;
    outputs.edit1() = 0.5f + bloom->getBloomSize() * 3.5f;
    outputs.edit2() = bloom;
}

BloomApply::BloomApply() {

}

void BloomApply::configure(const Config& config) {}

void BloomApply::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;

    static const auto BLUR0_SLOT = 0;
    static const auto BLUR1_SLOT = 1;
    static const auto BLUR2_SLOT = 2;

    if (!_pipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::bloomApply);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        state->setDepthTest(gpu::State::DepthTest(false, false));
        _pipeline = gpu::Pipeline::create(program, state);
    }

    const auto frameBuffer = inputs.get0();
    const auto framebufferSize = frameBuffer->getSize();
    const auto blur0FB = inputs.get1();
    const auto blur1FB = inputs.get2();
    const auto blur2FB = inputs.get3();
    const auto bloom = inputs.get4();
    const glm::ivec4 viewport{ 0, 0, framebufferSize.x, framebufferSize.y };

    const auto newIntensity = bloom->getBloomIntensity() / 3.0f;
    auto& parameters = _parameters.edit();
    parameters._intensities.x = newIntensity;
    parameters._intensities.y = newIntensity;
    parameters._intensities.z = newIntensity;

    gpu::doInBatch("BloomApply::run", args->_context, [&](gpu::Batch& batch) {
        batch.enableStereo(false);

        batch.setFramebuffer(frameBuffer);

        batch.setViewportTransform(viewport);
        batch.setProjectionTransform(glm::mat4());
        batch.resetViewTransform();
        batch.setPipeline(_pipeline);

        batch.setModelTransform(gpu::Framebuffer::evalSubregionTexcoordTransform(framebufferSize, viewport));
        batch.setResourceTexture(BLUR0_SLOT, blur0FB->getRenderBuffer(0));
        batch.setResourceTexture(BLUR1_SLOT, blur1FB->getRenderBuffer(0));
        batch.setResourceTexture(BLUR2_SLOT, blur2FB->getRenderBuffer(0));
        batch.setUniformBuffer(render_utils::slot::buffer::BloomParams, _parameters);
        batch.draw(gpu::TRIANGLE_STRIP, 4);
    });
}

void BloomDraw::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;

    const auto frameBuffer = inputs.get0();
    const auto bloomFrameBuffer = inputs.get1();

    if (frameBuffer && bloomFrameBuffer) {
        const auto framebufferSize = frameBuffer->getSize();

        if (!_pipeline) {
            gpu::ShaderPointer program = gpu::Shader::createProgram(shader::gpu::program::drawTransformUnitQuadTextureOpaque);
            gpu::StatePointer state = gpu::StatePointer(new gpu::State());
            state->setDepthTest(gpu::State::DepthTest(false, false));
            state->setBlendFunction(true, gpu::State::ONE, gpu::State::BLEND_OP_ADD, gpu::State::ONE,
                                    gpu::State::ZERO, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
            _pipeline = gpu::Pipeline::create(program, state);
        }

        gpu::doInBatch("BloomDraw::run", args->_context, [&](gpu::Batch& batch) {
            batch.enableStereo(false);

            batch.setFramebuffer(frameBuffer);

            batch.setViewportTransform(args->_viewport);
            batch.setProjectionTransform(glm::mat4());
            batch.resetViewTransform();
            batch.setPipeline(_pipeline);

            batch.setModelTransform(gpu::Framebuffer::evalSubregionTexcoordTransform(framebufferSize, args->_viewport));
            batch.setResourceTexture(0, bloomFrameBuffer->getRenderBuffer(0));
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        });
    }
}

void DebugBloomConfig::setMode(int mode) {
    _mode = std::min((int)DebugBloomConfig::MODE_COUNT, std::max(0, mode));
    emit dirty();
}

DebugBloom::DebugBloom() {
    _params = std::make_shared<gpu::Buffer>(sizeof(glm::vec4), nullptr);
}

void DebugBloom::configure(const Config& config) {
    _mode = (DebugBloomConfig::Mode) config.getMode();
    assert(_mode < DebugBloomConfig::MODE_COUNT);
}

void DebugBloom::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;

    if (_mode == DebugBloomConfig::OFF) {
        return;
    }

    const auto frameBuffer = inputs.get0();
    const auto combinedBlurBuffer = inputs.get4();
    const auto framebufferSize = frameBuffer->getSize();
    const auto level0FB = inputs.get1();
    const auto level1FB = inputs.get2();
    const auto level2FB = inputs.get3();
    const gpu::TexturePointer levelTextures[BLOOM_BLUR_LEVEL_COUNT] = {
        level0FB->getRenderBuffer(0),
        level1FB->getRenderBuffer(0),
        level2FB->getRenderBuffer(0)
    };

    if (!_pipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::gpu::program::drawTextureOpaqueTexcoordRect);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        state->setDepthTest(gpu::State::DepthTest(false));
        _pipeline = gpu::Pipeline::create(program, state);
    }

    gpu::doInBatch("DebugBloom::run", args->_context, [&](gpu::Batch& batch) {
        batch.enableStereo(false);

        batch.setFramebuffer(frameBuffer);

        batch.setViewportTransform(args->_viewport);
        batch.setProjectionTransform(glm::mat4());
        batch.resetViewTransform();
        batch.setPipeline(_pipeline);

        Transform modelTransform;
        if (_mode == DebugBloomConfig::MODE_ALL_LEVELS) {
            _params->setSubData(0, vec4(0.0f, 0.0f, 1.f, 1.f));
            batch.setUniformBuffer(0, _params);

            modelTransform = gpu::Framebuffer::evalSubregionTexcoordTransform(framebufferSize, args->_viewport / 2);
            modelTransform.postTranslate(glm::vec3(-1.0f, 1.0f, 0.0f));
            batch.setModelTransform(modelTransform);
            batch.setResourceTexture(0, levelTextures[0]);
            batch.draw(gpu::TRIANGLE_STRIP, 4);

            modelTransform.postTranslate(glm::vec3(2.0f, 0.0f, 0.0f));
            batch.setModelTransform(modelTransform);
            batch.setResourceTexture(0, levelTextures[1]);
            batch.draw(gpu::TRIANGLE_STRIP, 4);

            modelTransform.postTranslate(glm::vec3(-2.0f, -2.0f, 0.0f));
            batch.setModelTransform(modelTransform);
            batch.setResourceTexture(0, levelTextures[2]);
            batch.draw(gpu::TRIANGLE_STRIP, 4);

            modelTransform.postTranslate(glm::vec3(2.0f, 0.0f, 0.0f));
            batch.setModelTransform(modelTransform);
            batch.setResourceTexture(0, combinedBlurBuffer->getRenderBuffer(0));
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        } else {
            auto viewport = args->_viewport;
            auto blurLevel = _mode - DebugBloomConfig::MODE_LEVEL0;

            viewport.z /= 2;

            _params->setSubData(0, vec4(0.5f, 0.0f, 0.5f, 1.f));
            batch.setUniformBuffer(0, _params);

            modelTransform = gpu::Framebuffer::evalSubregionTexcoordTransform(framebufferSize, viewport);
            modelTransform.postTranslate(glm::vec3(-1.0f, 0.0f, 0.0f));
            batch.setModelTransform(modelTransform);
            batch.setResourceTexture(0, levelTextures[blurLevel]);
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        }
    });
}

BloomEffect::BloomEffect() {}

void BloomEffect::configure(const Config& config) {
    std::string blurName { "BloomBlurN" };

    for (auto i = 0; i < BLOOM_BLUR_LEVEL_COUNT; i++) {
        blurName.back() = '0' + i;
        auto blurConfig = config.getConfig<render::BlurGaussian>(blurName);
        blurConfig->filterScale = 1.0f;
    }
}

void BloomEffect::build(JobModel& task, const render::Varying& inputs, render::Varying& outputs) {
    // Start by computing threshold of color buffer input at quarter resolution
    const auto bloomOutputs = task.addJob<BloomThreshold>("BloomThreshold", inputs, 4U);

    // Multi-scale blur, each new blur is half resolution of the previous pass
    const auto blurInputBuffer = bloomOutputs.getN<BloomThreshold::Outputs>(0);
    const auto sigma = bloomOutputs.getN<BloomThreshold::Outputs>(1);
    const auto blurInput0 = render::BlurGaussian::Inputs(blurInputBuffer, true, 1U, 9, sigma).asVarying();
    const auto blurFB0 = task.addJob<render::BlurGaussian>("BloomBlur0", blurInput0);
    const auto blurInput1 = render::BlurGaussian::Inputs(blurFB0, true, 2U, 9, sigma).asVarying();
    const auto blurFB1 = task.addJob<render::BlurGaussian>("BloomBlur1", blurInput1);
    const auto blurInput2 = render::BlurGaussian::Inputs(blurFB1, true, 2U, 9, sigma).asVarying();
    const auto blurFB2 = task.addJob<render::BlurGaussian>("BloomBlur2", blurInput2);

    const auto& frameBuffer = inputs.getN<Inputs>(1);

    // Mix all blur levels at quarter resolution
    const auto bloom = bloomOutputs.getN<BloomThreshold::Outputs>(2);
    const auto applyInput = BloomApply::Inputs(blurInputBuffer, blurFB0, blurFB1, blurFB2, bloom).asVarying();
    task.addJob<BloomApply>("BloomApply", applyInput);
    // And then blend result in additive manner on top of final color buffer
    const auto drawInput = BloomDraw::Inputs(frameBuffer, blurInputBuffer).asVarying();
    task.addJob<BloomDraw>("BloomDraw", drawInput);

    const auto debugInput = DebugBloom::Inputs(frameBuffer, blurFB0, blurFB1, blurFB2, blurInputBuffer).asVarying();
    task.addJob<DebugBloom>("DebugBloom", debugInput);
}
