//
//  StencilMaskPass.cpp
//  render-utils/src/
//
//  Created by Sam Gateau on 5/31/17.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "StencilMaskPass.h"

#include <ViewFrustum.h>
#include <gpu/Context.h>
#include <shaders/Shaders.h>

using namespace render;

void PrepareStencil::configure(const Config& config) {
    _maskMode = config.maskMode;
}

graphics::MeshPointer PrepareStencil::getMesh() {
    if (!_mesh) {

        std::vector<glm::vec3> vertices {
            { -1.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
            { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
            { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
            { 1.0f, -1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };

        std::vector<uint32_t> indices { 0, 7, 1, 1, 3, 2, 3, 5, 4, 5, 7, 6 };
        _mesh = graphics::Mesh::createIndexedTriangles_P3F((uint32_t) vertices.size(), (uint32_t) indices.size(), vertices.data(), indices.data());
    }
    return _mesh;
}

gpu::PipelinePointer PrepareStencil::getMeshStencilPipeline() {
    if (!_meshStencilPipeline) {
        auto program = gpu::Shader::createProgram(shader::gpu::program::drawNothing);
        auto state = std::make_shared<gpu::State>();
        drawMask(*state);
        state->setColorWriteMask(gpu::State::WRITE_NONE);
        state->setCullMode(gpu::State::CullMode::CULL_NONE);

        _meshStencilPipeline = gpu::Pipeline::create(program, state);
    }
    return _meshStencilPipeline;
}

gpu::PipelinePointer PrepareStencil::getPaintStencilPipeline() {
    if (!_paintStencilPipeline) {
        auto program = gpu::Shader::createProgram(shader::render_utils::program::stencil_drawMask);
        auto state = std::make_shared<gpu::State>();
        drawMask(*state);
        state->setColorWriteMask(gpu::State::WRITE_NONE);

        _paintStencilPipeline = gpu::Pipeline::create(program, state);
    }
    return _paintStencilPipeline;
}

void PrepareStencil::run(const RenderContextPointer& renderContext, const gpu::FramebufferPointer& srcFramebuffer) {
    RenderArgs* args = renderContext->args;

    if (args->_takingSnapshot) {
        return;
    }

    StencilMaskMode maskMode = _maskMode;
    std::function<void(gpu::Batch&)> maskOperator = [this](gpu::Batch& batch) {
        auto mesh = getMesh();
        batch.setIndexBuffer(mesh->getIndexBuffer());
        batch.setInputFormat((mesh->getVertexFormat()));
        batch.setInputStream(0, mesh->getVertexStream());

        // Draw
        auto part = mesh->getPartBuffer().get<graphics::Mesh::Part>(0);
        batch.drawIndexed(gpu::TRIANGLES, part._numIndices, part._startIndex);
    };

    if (maskMode == StencilMaskMode::NONE) {
        maskMode = args->_stencilMaskMode;
        maskOperator = args->_stencilMaskOperator;
    }

    if (maskMode == StencilMaskMode::NONE || (maskMode == StencilMaskMode::MESH && !maskOperator)) {
        return;
    }

    doInBatch("PrepareStencil::run", args->_context, [&](gpu::Batch& batch) {
        batch.enableStereo(false);

        batch.setViewportTransform(args->_viewport);

        if (maskMode == StencilMaskMode::PAINT) {
            batch.setPipeline(getPaintStencilPipeline());
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        } else if (maskMode == StencilMaskMode::MESH) {
            batch.setPipeline(getMeshStencilPipeline());
            maskOperator(batch);
        }
    });
}

// Always draw MASK to the stencil buffer (used to always prevent drawing in certain areas later)
void PrepareStencil::drawMask(gpu::State& state) {
    state.setStencilTest(true, 0xFF, gpu::State::StencilTest(STENCIL_MASK, 0xFF, gpu::ALWAYS,
        gpu::State::STENCIL_OP_REPLACE, gpu::State::STENCIL_OP_REPLACE, gpu::State::STENCIL_OP_REPLACE));
}

// Draw BACKGROUND to the stencil buffer behind everything else
void PrepareStencil::drawBackground(gpu::State& state) {
    state.setStencilTest(true, 0xFF, gpu::State::StencilTest(STENCIL_BACKGROUND, 0xFF, gpu::ALWAYS,
        gpu::State::STENCIL_OP_REPLACE, gpu::State::STENCIL_OP_REPLACE, gpu::State::STENCIL_OP_KEEP));
}

// Pass if this area has NOT been marked as MASK or anything containing MASK
void PrepareStencil::testMask(gpu::State& state) {
    state.setStencilTest(true, 0x00, gpu::State::StencilTest(STENCIL_MASK, STENCIL_MASK, gpu::NOT_EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP));
}

// Pass if this area has NOT been marked as MASK or anything containing MASK and reset NO_AA if it passes
void PrepareStencil::testMaskResetNoAA(gpu::State& state) {
    state.setStencilTest(true, STENCIL_NO_AA, gpu::State::StencilTest(STENCIL_MASK, STENCIL_MASK, gpu::NOT_EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_REPLACE));
}

// Pass if this area has NOT been marked as NO_AA or anything containing NO_AA
void PrepareStencil::testNoAA(gpu::State& state) {
    state.setStencilTest(true, 0x00, gpu::State::StencilTest(STENCIL_NO_AA, STENCIL_NO_AA, gpu::NOT_EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP));
}

// Pass if this area WAS marked as BACKGROUND
// (see: graphics/src/Skybox.cpp, procedural/src/ProceduralSkybox.cpp)
void PrepareStencil::testBackground(gpu::State& state) {
    state.setStencilTest(true, 0x00, gpu::State::StencilTest(STENCIL_BACKGROUND, 0xFF, gpu::EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP));
}

// Pass if this area WAS marked as SHAPE or anything containing SHAPE
void PrepareStencil::testShape(gpu::State& state) {
    state.setStencilTest(true, 0x00, gpu::State::StencilTest(STENCIL_SHAPE, STENCIL_SHAPE, gpu::EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP));
}

// Pass if this area was NOT marked as MASK, write to SHAPE if it passes
void PrepareStencil::testMaskDrawShape(gpu::State& state) {
    state.setStencilTest(true, STENCIL_SHAPE, gpu::State::StencilTest(STENCIL_MASK | STENCIL_SHAPE, STENCIL_MASK, gpu::NOT_EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_REPLACE));
}

// Pass if this area was NOT marked as MASK, write to SHAPE and NO_AA if it passes
void PrepareStencil::testMaskDrawShapeNoAA(gpu::State& state) {
    state.setStencilTest(true, STENCIL_SHAPE | STENCIL_NO_AA, gpu::State::StencilTest(STENCIL_MASK | STENCIL_SHAPE | STENCIL_NO_AA, STENCIL_MASK, gpu::NOT_EQUAL,
        gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_REPLACE));
}
