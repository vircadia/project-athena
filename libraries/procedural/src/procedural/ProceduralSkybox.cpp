//
//  ProceduralSkybox.cpp
//  libraries/procedural/src/procedural
//
//  Created by Sam Gateau on 9/21/2015.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "ProceduralSkybox.h"


#include <gpu/Batch.h>
#include <gpu/Context.h>
#include <gpu/Shader.h>
#include <ViewFrustum.h>
#include <shaders/Shaders.h>

ProceduralSkybox::ProceduralSkybox(uint64_t created) : graphics::Skybox(), _created(created) {
    // FIXME: support forward rendering for procedural skyboxes (needs haze calculation)
    _procedural._vertexSource = shader::Source::get(shader::graphics::vertex::skybox);
    _procedural._opaqueFragmentSource = shader::Source::get(shader::procedural::fragment::proceduralSkybox);

    _procedural.setDoesFade(false);

    // Adjust the pipeline state for background using the stencil test
    // Must match PrepareStencil::STENCIL_BACKGROUND
    const int8_t STENCIL_BACKGROUND = 0;
    _procedural._opaqueState->setStencilTest(true, 0xFF, gpu::State::StencilTest(STENCIL_BACKGROUND, 0xFF, gpu::EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP));
    _procedural._opaqueState->setDepthTest(gpu::State::DepthTest(false));
}

bool ProceduralSkybox::empty() {
    return !_procedural.isEnabled() && Skybox::empty();
}

void ProceduralSkybox::clear() {
    // Parse and prepare a procedural with no shaders to release textures
    parse(QString());

    Skybox::clear();
}

void ProceduralSkybox::render(gpu::Batch& batch, const ViewFrustum& frustum, bool forward, uint transformSlot) const {
    if (_procedural.isReady()) {
        ProceduralSkybox::render(batch, frustum, (*this), forward, transformSlot);
    } else {
        Skybox::render(batch, frustum, forward, transformSlot);
    }
}

void ProceduralSkybox::render(gpu::Batch& batch, const ViewFrustum& viewFrustum, const ProceduralSkybox& skybox, bool forward, uint transformSlot) {
    glm::mat4 projMat;
    viewFrustum.evalProjectionMatrix(projMat);

    Transform viewTransform;
    viewFrustum.evalViewTransform(viewTransform);
    batch.setProjectionTransform(projMat);
    batch.setViewTransform(viewTransform);
    // This is needed if we want to have motion vectors on the sky
    batch.saveViewProjectionTransform(transformSlot);
    batch.setModelTransform(Transform()); // only for Mac

    auto& procedural = skybox._procedural;
    procedural.prepare(batch, glm::vec3(0), glm::vec3(1), glm::quat(), skybox.getCreated());
    skybox.prepare(batch);
    batch.draw(gpu::TRIANGLE_STRIP, 4);
}

