//
//  GLBackendPipeline.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 3/8/2015.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GLBackend.h"
#include <gpu/TextureTable.h>
#include <gpu/ShaderConstants.h>

#include "GLShared.h"
#include "GLPipeline.h"
#include "GLShader.h"
#include "GLState.h"
#include "GLBuffer.h"
#include "GLTexture.h"

using namespace gpu;
using namespace gpu::gl;

void GLBackend::do_setPipeline(const Batch& batch, size_t paramOffset) {
    const auto& pipeline = batch._pipelines.get(batch._params[paramOffset + 0]._uint);

    if (compare(_pipeline._pipeline, pipeline)) {
        return;
    }

    // A true new Pipeline
    _stats._PSNumSetPipelines++;

    // null pipeline == reset
    if (!pipeline) {
        reset(_pipeline._pipeline);

        _pipeline._program = 0;
        _pipeline._programShader = nullptr;
        _pipeline._invalidProgram = true;

        _pipeline._state = nullptr;
        _pipeline._invalidState = true;
    } else {
        auto pipelineObject = GLPipeline::sync(*this, *pipeline);
        if (!pipelineObject) {
            return;
        }

            // check the program cache
            // pick the program version
            // check the program cache
            // pick the program version
#ifdef GPU_STEREO_CAMERA_BUFFER
        GLuint glprogram = pipelineObject->_program->getProgram(getShaderVariant());
#else
        GLuint glprogram = pipelineObject->_program->getProgram();
#endif

        if (_pipeline._program != glprogram) {
            _pipeline._program = glprogram;
            _pipeline._programShader = pipelineObject->_program;
            _pipeline._invalidProgram = true;
        }

        // Now for the state
        if (_pipeline._state != pipelineObject->_state) {
            _pipeline._state = pipelineObject->_state;
            _pipeline._invalidState = true;
        }

        // Remember the new pipeline
        assign(_pipeline._pipeline, pipeline);
    }

    // THis should be done on Pipeline::update...
    if (_pipeline._invalidProgram) {
        glUseProgram(_pipeline._program);
        (void)CHECK_GL_ERROR();
        _pipeline._invalidProgram = false;
    }
}

void GLBackend::updatePipeline() {
    if (_pipeline._invalidProgram) {
        // doing it here is aproblem for calls to glUniform.... so will do it on assing...
        glUseProgram(_pipeline._program);
        (void)CHECK_GL_ERROR();
        _pipeline._invalidProgram = false;
    }

    if (_pipeline._invalidState) {
        if (_pipeline._state) {
            // first reset to default what should be
            // the fields which were not to default and are default now
            resetPipelineState(_pipeline._state->_signature);

            // Update the signature cache with what's going to be touched
            _pipeline._stateSignatureCache |= _pipeline._state->_signature;

            // And perform
            for (const auto& command : _pipeline._state->_commands) {
                command->run(this);
            }
        } else {
            // No state ? anyway just reset everything
            resetPipelineState(0);
        }
        _pipeline._invalidState = false;
    }
}

void GLBackend::resetPipelineStage() {
    // First reset State to default
    State::Signature resetSignature(0);
    resetPipelineState(resetSignature);
    _pipeline._state = nullptr;
    _pipeline._invalidState = false;

    // Second the shader side
    _pipeline._invalidProgram = false;
    _pipeline._program = 0;
    _pipeline._programShader = nullptr;
    reset(_pipeline._pipeline);
    glUseProgram(0);
}

void GLBackend::releaseUniformBuffer(uint32_t slot) {
    auto& bufferState = _uniform._buffers[slot];
    if (valid(bufferState.buffer)) {
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, 0);  // RELEASE
        (void)CHECK_GL_ERROR();
    }
    bufferState.reset();
}

void GLBackend::resetUniformStage() {
    for (uint32_t i = 0; i < _uniform._buffers.size(); i++) {
        releaseUniformBuffer(i);
    }
}

void GLBackend::bindUniformBuffer(uint32_t slot, const BufferPointer& buffer, GLintptr offset, GLsizeiptr size) {
    if (!buffer) {
        releaseUniformBuffer(slot);
        return;
    }


    auto& currentBufferState = _uniform._buffers[slot];
    // check cache before thinking
    if (currentBufferState.compare(buffer, offset, size)) {
        return;
    }

    // Grab the true gl Buffer object
    auto glBO = getBufferIDUnsynced(*buffer);
    if (glBO) {
        glBindBufferRange(GL_UNIFORM_BUFFER, slot, glBO, offset, size);
        assign(currentBufferState.buffer, buffer);
        currentBufferState.offset = offset;
        currentBufferState.size = size;
        (void)CHECK_GL_ERROR();
    } else {
        releaseUniformBuffer(slot);
        return;
    }

}

void GLBackend::do_setUniformBuffer(const Batch& batch, size_t paramOffset) {
    GLuint slot = batch._params[paramOffset + 3]._uint;
    if (slot > (GLuint)MAX_NUM_UNIFORM_BUFFERS) {
        qCDebug(gpugllogging) << "GLBackend::do_setUniformBuffer: Trying to set a uniform Buffer at slot #" << slot
                              << " which doesn't exist. MaxNumUniformBuffers = " << getMaxNumUniformBuffers();
        return;
    }

    const auto& uniformBuffer = batch._buffers.get(batch._params[paramOffset + 2]._uint);
    GLintptr rangeStart = batch._params[paramOffset + 1]._uint;
    GLsizeiptr rangeSize = batch._params[paramOffset + 0]._uint;

    bindUniformBuffer(slot, uniformBuffer, rangeStart, rangeSize);
}

void GLBackend::releaseResourceTexture(uint32_t slot) {
    auto& textureState = _resource._textures[slot];
    if (valid(textureState._texture)) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(textureState._target, 0);  // RELEASE
        (void)CHECK_GL_ERROR();
        reset(textureState._texture);
    }
}

void GLBackend::resetResourceStage() {
    uint32_t i;
    for (i = 0; i < _resource._buffers.size(); i++) {
        releaseResourceBuffer(i);
    }
    for (i = 0; i < _resource._textures.size(); i++) {
        releaseResourceTexture(i);
    }
}

void GLBackend::do_setResourceBuffer(const Batch& batch, size_t paramOffset) {
    GLuint slot = batch._params[paramOffset + 1]._uint;
    if (slot >= (GLuint)MAX_NUM_RESOURCE_BUFFERS) {
        qCDebug(gpugllogging) << "GLBackend::do_setResourceBuffer: Trying to set a resource Buffer at slot #" << slot
                              << " which doesn't exist. MaxNumResourceBuffers = " << getMaxNumResourceBuffers();
        return;
    }

    const auto& resourceBuffer = batch._buffers.get(batch._params[paramOffset + 0]._uint);

    if (!resourceBuffer) {
        releaseResourceBuffer(slot);
        return;
    }
    // check cache before thinking
    if (compare(_resource._buffers[slot], resourceBuffer)) {
        return;
    }

    // One more True Buffer bound
    _stats._RSNumResourceBufferBounded++;

    // If successful bind then cache it
    if (bindResourceBuffer(slot, resourceBuffer)) {
        assign(_resource._buffers[slot], resourceBuffer);
    } else {  // else clear slot and cache
        releaseResourceBuffer(slot);
        return;
    }
}

void GLBackend::do_setResourceTexture(const Batch& batch, size_t paramOffset) {
    GLuint slot = batch._params[paramOffset + 1]._uint;
    if (slot >= (GLuint)MAX_NUM_RESOURCE_TEXTURES) {
        qCDebug(gpugllogging) << "GLBackend::do_setResourceTexture: Trying to set a resource Texture at slot #" << slot
                              << " which doesn't exist. MaxNumResourceTextures = " << getMaxNumResourceTextures();
        return;
    }

    const auto& resourceTexture = batch._textures.get(batch._params[paramOffset + 0]._uint);
    bindResourceTexture(slot, resourceTexture);
}

void GLBackend::bindResourceTexture(uint32_t slot, const TexturePointer& resourceTexture) {
    if (!resourceTexture) {
        releaseResourceTexture(slot);
        return;
    }
    setResourceTexture(slot, resourceTexture);
}

void GLBackend::do_setResourceFramebufferSwapChainTexture(const Batch& batch, size_t paramOffset) {
    GLuint slot = batch._params[paramOffset + 1]._uint;
    if (slot >= (GLuint)MAX_NUM_RESOURCE_TEXTURES) {
        qCDebug(gpugllogging)
            << "GLBackend::do_setResourceFramebufferSwapChainTexture: Trying to set a resource Texture at slot #" << slot
            << " which doesn't exist. MaxNumResourceTextures = " << getMaxNumResourceTextures();
        return;
    }

    auto swapChain =
        std::static_pointer_cast<FramebufferSwapChain>(batch._swapChains.get(batch._params[paramOffset + 0]._uint));

    if (!swapChain) {
        releaseResourceTexture(slot);
        return;
    }
    auto index = batch._params[paramOffset + 2]._uint;
    auto renderBufferSlot = batch._params[paramOffset + 3]._uint;
    const auto& resourceFramebuffer = swapChain->get(index);
    const auto& resourceTexture = resourceFramebuffer->getRenderBuffer(renderBufferSlot);
    setResourceTexture(slot, resourceTexture);
}

void GLBackend::setResourceTexture(unsigned int slot, const TexturePointer& resourceTexture) {
    auto& textureState = _resource._textures[slot];
    // check cache before thinking
    if (compare(textureState._texture, resourceTexture)) {
        return;
    }

    // One more True texture bound
    _stats._RSNumTextureBounded++;

    // Always make sure the GLObject is in sync
    GLTexture* object = syncGPUObject(resourceTexture);
    if (object) {
        assign(textureState._texture, resourceTexture);
        GLuint to = object->_texture;
        textureState._target = object->_target;
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(textureState._target, to);
        (void)CHECK_GL_ERROR();
        _stats._RSAmountTextureMemoryBounded += (uint64_t)object->size();

    } else {
        releaseResourceTexture(slot);
        return;
    }
}

void GLBackend::do_setResourceTextureTable(const Batch& batch, size_t paramOffset) {
    const auto& textureTablePointer = batch._textureTables.get(batch._params[paramOffset]._uint);
    if (!textureTablePointer) {
        return;
    }

    const auto& textureTable = *textureTablePointer;
    const auto& textures = textureTable.getTextures();
    for (GLuint slot = 0; slot < textures.size(); ++slot) {
        bindResourceTexture(slot, textures[slot]);
    }
}

int GLBackend::ResourceStageState::findEmptyTextureSlot() const {
    // start from the end of the slots, try to find an empty one that can be used
    for (auto i = MAX_NUM_RESOURCE_TEXTURES - 1; i > 0; i--) {
        if (!valid(_textures[i]._texture)) {
            return i;
        }
    }
    return -1;
}
