//
//  ZoneRenderer.cpp
//  render/src/render-utils
//
//  Created by Sam Gateau on 4/4/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "ZoneRenderer.h"

#include <gpu/Context.h>

#include <render/FilterTask.h>
#include <render/DrawTask.h>
#include <shaders/Shaders.h>
#include <gpu/ShaderConstants.h>
#include <graphics/ShaderConstants.h>

#include "StencilMaskPass.h"
#include "DeferredLightingEffect.h"

#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"
#include "DeferredLightingEffect.h"

#include "BloomStage.h"

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

using namespace render;

const Selection::Name ZoneRendererTask::ZONES_SELECTION { "RankedZones" };

void ZoneRendererTask::build(JobModel& task, const Varying& input, Varying& output) {
    // Filter out the sorted list of zones
    // FIXME: the zones in the selection are already sorted, but we're doing another sort here to pick the selected items
    // out of `input`, which means we're also looping over the inItems an extra time.
    const auto zoneItems = task.addJob<render::SelectSortItems>("FilterZones", input, ZONES_SELECTION);

    // just setup the current zone env
    task.addJob<SetupZones>("SetupZones", zoneItems);

    output = zoneItems;
}

void SetupZones::run(const RenderContextPointer& context, const Input& input) {
    // Grab light, background, haze, and bloom stages and clear them
    auto lightStage = context->_scene->getStage<LightStage>();
    assert(lightStage);
    lightStage->_currentFrame.clear();

    auto backgroundStage = context->_scene->getStage<BackgroundStage>();
    assert(backgroundStage);
    backgroundStage->_currentFrame.clear();

    auto hazeStage = context->_scene->getStage<HazeStage>();
    assert(hazeStage);
    hazeStage->_currentFrame.clear();

    auto bloomStage = context->_scene->getStage<BloomStage>();
    assert(bloomStage);
    bloomStage->_currentFrame.clear();

    // call render over the zones to grab their components in the correct order first...
    render::renderItems(context, input);

    // Finally add the default lights and background:
    lightStage->_currentFrame.pushSunLight(lightStage->getDefaultLight());
    lightStage->_currentFrame.pushAmbientLight(lightStage->getDefaultLight());
    backgroundStage->_currentFrame.pushBackground(0);
    hazeStage->_currentFrame.pushHaze(0);
    bloomStage->_currentFrame.pushBloom(INVALID_INDEX);
}

const gpu::PipelinePointer& DebugZoneLighting::getKeyLightPipeline() {
    if (!_keyLightPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::zone_drawKeyLight);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        PrepareStencil::testMaskResetNoAA(*state);
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        _keyLightPipeline = gpu::Pipeline::create(program, state);
    }
    return _keyLightPipeline;
}

const gpu::PipelinePointer& DebugZoneLighting::getAmbientPipeline() {
    if (!_ambientPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::zone_drawAmbient);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        PrepareStencil::testMaskResetNoAA(*state);
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        _ambientPipeline = gpu::Pipeline::create(program, state);
    }
    return _ambientPipeline;
}
const gpu::PipelinePointer& DebugZoneLighting::getBackgroundPipeline() {
    if (!_backgroundPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::zone_drawSkybox);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        PrepareStencil::testMaskResetNoAA(*state);
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        _backgroundPipeline = gpu::Pipeline::create(program, state);
    }
    return _backgroundPipeline;
}

void DebugZoneLighting::run(const render::RenderContextPointer& context, const Inputs& inputs) {
    RenderArgs* args = context->args;

    auto deferredTransform = inputs.get0();
    auto lightFrame = inputs.get1();
    auto backgroundFrame = inputs.get2();

    auto lightStage = context->_scene->getStage<LightStage>(LightStage::getName());
    std::vector<graphics::LightPointer> keyLightStack;
    if (lightStage && lightFrame->_sunLights.size()) {
        for (auto index : lightFrame->_sunLights) {
            keyLightStack.push_back(lightStage->getLight(index));
        }
    }

    std::vector<graphics::LightPointer> ambientLightStack;
    if (lightStage && lightFrame->_ambientLights.size()) {
        for (auto index : lightFrame->_ambientLights) {
            ambientLightStack.push_back(lightStage->getLight(index));
        }
    }

    auto backgroundStage = context->_scene->getStage<BackgroundStage>(BackgroundStage::getName());
    std::vector<graphics::SkyboxPointer> skyboxStack;
    if (backgroundStage && backgroundFrame->_backgrounds.size()) {
        for (auto index : backgroundFrame->_backgrounds) {
            auto background = backgroundStage->getBackground(index);
            if (background) {
                skyboxStack.push_back(background->getSkybox());
            }
        }
    }

    gpu::doInBatch("DebugZoneLighting::run", args->_context, [=](gpu::Batch& batch) {

        batch.setViewportTransform(args->_viewport);
        auto viewFrustum = args->getViewFrustum();
        batch.setProjectionTransform(viewFrustum.getProjection());
        batch.resetViewTransform();

        Transform model;

        batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, deferredTransform->getFrameTransformBuffer());

        batch.setPipeline(getKeyLightPipeline());
        auto numKeys = (int) keyLightStack.size();
        for (int i = numKeys - 1; i >= 0; i--) {
            model.setTranslation(glm::vec3(-4.0, -3.0 + (i * 1.0), -10.0 - (i * 3.0)));
            batch.setModelTransform(model);
            if (keyLightStack[i]) {
                batch.setUniformBuffer(gr::Buffer::KeyLight, keyLightStack[i]->getLightSchemaBuffer());
                batch.draw(gpu::TRIANGLE_STRIP, 4);
            }
        }

        batch.setPipeline(getAmbientPipeline());
        auto numAmbients = (int) ambientLightStack.size();
        for (int i = numAmbients - 1; i >= 0; i--) {
            model.setTranslation(glm::vec3(0.0, -3.0 + (i * 1.0), -10.0 - (i * 3.0)));
            batch.setModelTransform(model);
            if (ambientLightStack[i]) {
                batch.setUniformBuffer(gr::Buffer::AmbientLight, ambientLightStack[i]->getAmbientSchemaBuffer());
                if (ambientLightStack[i]->getAmbientMap()) {
                    batch.setResourceTexture(ru::Texture::Skybox, ambientLightStack[i]->getAmbientMap());
                }
                batch.draw(gpu::TRIANGLE_STRIP, 4);
            }
        }

        batch.setPipeline(getBackgroundPipeline());
        auto numBackgrounds = (int) skyboxStack.size();
        for (int i = numBackgrounds - 1; i >= 0; i--) {
            model.setTranslation(glm::vec3(4.0, -3.0 + (i * 1.0), -10.0 - (i * 3.0)));
            batch.setModelTransform(model);
            if (skyboxStack[i]) {
                batch.setResourceTexture(ru::Texture::Skybox, skyboxStack[i]->getCubemap());
                batch.setUniformBuffer(ru::Buffer::DebugSkyboxParams, skyboxStack[i]->getSchemaBuffer());
                batch.draw(gpu::TRIANGLE_STRIP, 4);
            }
        }
    }); 
}
