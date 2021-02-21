//
//  BackgroundStage.cpp
//
//  Created by Sam Gateau on 5/9/2017.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "BackgroundStage.h"

#include "DeferredLightingEffect.h"

#include <gpu/Context.h>

#include <graphics/ShaderConstants.h>

std::string BackgroundStage::_stageName { "BACKGROUND_STAGE"};
const BackgroundStage::Index BackgroundStage::INVALID_INDEX { render::indexed_container::INVALID_INDEX };

BackgroundStage::Index BackgroundStage::findBackground(const BackgroundPointer& background) const {
    auto found = _backgroundMap.find(background);
    if (found != _backgroundMap.end()) {
        return INVALID_INDEX;
    } else {
        return (*found).second;
    }

}

BackgroundStage::Index BackgroundStage::addBackground(const BackgroundPointer& background) {

    auto found = _backgroundMap.find(background);
    if (found == _backgroundMap.end()) {
        auto backgroundId = _backgrounds.newElement(background);
        // Avoid failing to allocate a background, just pass
        if (backgroundId != INVALID_INDEX) {

            // Insert the background and its index in the reverse map
            _backgroundMap.insert(BackgroundMap::value_type(background, backgroundId));
        }
        return backgroundId;
    } else {
        return (*found).second;
    }
}


BackgroundStage::BackgroundPointer BackgroundStage::removeBackground(Index index) {
    BackgroundPointer removed = _backgrounds.freeElement(index);
    
    if (removed) {
        _backgroundMap.erase(removed);
    }
    return removed;
}


void DrawBackgroundStage::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    const auto& lightingModel = inputs.get0();
    if (!lightingModel->isBackgroundEnabled()) {
        return;
    }

    // Background rendering decision
    const auto& backgroundStage = renderContext->_scene->getStage<BackgroundStage>();
    const auto& backgroundFrame = inputs.get1();
    graphics::SkyboxPointer skybox;
    if (backgroundStage && backgroundFrame->_backgrounds.size()) {
        const auto& background = backgroundStage->getBackground(backgroundFrame->_backgrounds.front());
        if (background) {
            skybox = background->getSkybox();
        }   
    }

    const auto& hazeFrame = inputs.get2();

    if (skybox && !skybox->empty()) {
        PerformanceTimer perfTimer("skybox");
        auto args = renderContext->args;

        gpu::doInBatch("DrawBackgroundStage::run", args->_context, [&](gpu::Batch& batch) {
            PROFILE_RANGE_BATCH(batch, "Background");
            args->_batch = &batch;

            batch.enableSkybox(true);

            batch.setViewportTransform(args->_viewport);
            batch.setStateScissorRect(args->_viewport);

            bool forward = args->_renderMethod == render::Args::RenderMethod::FORWARD;
            batch.setProjectionJitterEnabled(!forward);

            // If we're using forward rendering, we need to calculate haze
            if (forward) {
                const auto& hazeStage = args->_scene->getStage<HazeStage>();
                if (hazeStage && hazeFrame->_hazes.size() > 0) {
                    const auto& hazePointer = hazeStage->getHaze(hazeFrame->_hazes.front());
                    if (hazePointer) {
                        batch.setUniformBuffer(graphics::slot::buffer::Buffer::HazeParams, hazePointer->getHazeParametersBuffer());
                    }
                }
            }

            skybox->render(batch, args->getViewFrustum(), forward, _transformSlot);
        });
        args->_batch = nullptr;
    }
}

BackgroundStageSetup::BackgroundStageSetup() {
}

void BackgroundStageSetup::run(const render::RenderContextPointer& renderContext) {
    auto stage = renderContext->_scene->getStage(BackgroundStage::getName());
    if (!stage) {
        renderContext->_scene->resetStage(BackgroundStage::getName(), std::make_shared<BackgroundStage>());
    }
}

