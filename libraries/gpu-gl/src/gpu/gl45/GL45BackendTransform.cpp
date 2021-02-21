//
//  GL45BackendTransform.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 3/8/2015.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GL45Backend.h"

#include "gpu/gl/GLBuffer.h"

using namespace gpu;
using namespace gpu::gl45;

void GL45Backend::initTransform() {
    GLuint transformBuffers[3];
    glCreateBuffers(3, transformBuffers);
    _transform._objectBuffer = transformBuffers[0];
    _transform._cameraBuffer = transformBuffers[1];
    _transform._drawCallInfoBuffer = transformBuffers[2];
#ifdef GPU_SSBO_TRANSFORM_OBJECT
#else
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &_transform._objectBufferTexture);
#endif
    size_t cameraSize = sizeof(TransformStageState::CameraBufferElement);
    while (_transform._cameraUboSize < cameraSize) {
        _transform._cameraUboSize += UNIFORM_BUFFER_OFFSET_ALIGNMENT;
    }
}

void GL45Backend::transferTransformState(const Batch& batch) const {
    // FIXME not thread safe
    static std::vector<uint8_t> bufferData;
    if (!_transform._cameras.empty()) {
        bufferData.resize(_transform._cameraUboSize * _transform._cameras.size());
        for (size_t i = 0; i < _transform._cameras.size(); ++i) {
            memcpy(bufferData.data() + (_transform._cameraUboSize * i), &_transform._cameras[i], sizeof(TransformStageState::CameraBufferElement));
        }
        glNamedBufferData(_transform._cameraBuffer, bufferData.size(), bufferData.data(), GL_STREAM_DRAW);
    }

    if (!batch._objects.empty()) {
        glNamedBufferData(_transform._objectBuffer, batch._objects.size() * sizeof(Batch::TransformObject), batch._objects.data(), GL_STREAM_DRAW);
    }

    if (!batch._namedData.empty()) {
        bufferData.clear();
        for (auto& data : batch._namedData) {
            auto currentSize = bufferData.size();
            auto bytesToCopy = data.second.drawCallInfos.size() * sizeof(Batch::DrawCallInfo);
            bufferData.resize(currentSize + bytesToCopy);
            memcpy(bufferData.data() + currentSize, data.second.drawCallInfos.data(), bytesToCopy);
            _transform._drawCallInfoOffsets[data.first] = (GLvoid*)currentSize;
        }
        glNamedBufferData(_transform._drawCallInfoBuffer, bufferData.size(), bufferData.data(), GL_STREAM_DRAW);
    }

#ifdef GPU_SSBO_TRANSFORM_OBJECT
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot::storage::ObjectTransforms, _transform._objectBuffer);
#else
    glActiveTexture(GL_TEXTURE0 + slot::texture::ObjectTransforms);
    glBindTexture(GL_TEXTURE_BUFFER, _transform._objectBufferTexture);
    glTextureBuffer(_transform._objectBufferTexture, GL_RGBA32F, _transform._objectBuffer);
#endif

    CHECK_GL_ERROR();

    // Make sure the current Camera offset is unknown before render Draw
    _transform._currentCameraOffset = INVALID_OFFSET;
}


void GL45Backend::updateTransform(const Batch& batch) {
    _transform.update(_commandIndex, _stereo);

    auto& drawCallInfoBuffer = batch.getDrawCallInfoBuffer();
    if (batch._currentNamedCall.empty()) {
        auto& drawCallInfo = drawCallInfoBuffer[_currentDraw];
        if (_transform._enabledDrawcallInfoBuffer) {
            glDisableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO); // Make sure attrib array is disabled
            _transform._enabledDrawcallInfoBuffer = false;
        }
        glVertexAttribI2i(gpu::Stream::DRAW_CALL_INFO, drawCallInfo.index, drawCallInfo.unused);
    } else {
        if (!_transform._enabledDrawcallInfoBuffer) {
            glEnableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO); // Make sure attrib array is enabled
            glVertexAttribIFormat(gpu::Stream::DRAW_CALL_INFO, 2, GL_UNSIGNED_SHORT, 0);
            glVertexAttribBinding(gpu::Stream::DRAW_CALL_INFO, gpu::Stream::DRAW_CALL_INFO);
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
            glVertexBindingDivisor(gpu::Stream::DRAW_CALL_INFO, (isStereo() ? 2 : 1));
#else
            glVertexBindingDivisor(gpu::Stream::DRAW_CALL_INFO, 1);
#endif
            _transform._enabledDrawcallInfoBuffer = true;
        }
        // NOTE: A stride of zero in BindVertexBuffer signifies that all elements are sourced from the same location,
        //       so we must provide a stride.
        //       This is in contrast to VertexAttrib*Pointer, where a zero signifies tightly-packed elements.
        glBindVertexBuffer(gpu::Stream::DRAW_CALL_INFO, _transform._drawCallInfoBuffer, (GLintptr)_transform._drawCallInfoOffsets[batch._currentNamedCall], 2 * sizeof(GLushort));
    }

    (void)CHECK_GL_ERROR();
}

void GL45Backend::do_copySavedViewProjectionTransformToBuffer(const Batch& batch, size_t paramOffset) {
    auto slotId = batch._params[paramOffset + 0]._uint;
    BufferPointer buffer = batch._buffers.get(batch._params[paramOffset + 1]._uint);
    auto dstOffset = batch._params[paramOffset + 2]._uint;
    size_t size = _transform._cameraUboSize;

    slotId = std::min<gpu::uint32>(slotId, gpu::Batch::MAX_TRANSFORM_SAVE_SLOT_COUNT);
    const auto& savedTransform = _transform._savedTransforms[slotId];

    if ((dstOffset + size) > buffer->getBufferCPUMemSize()) {
        qCWarning(gpugllogging) << "Copying saved TransformCamera data out of bounds of uniform buffer";
        size = (size_t)std::max<ptrdiff_t>((ptrdiff_t)buffer->getBufferCPUMemSize() - (ptrdiff_t)dstOffset, 0);
    }
    if (savedTransform._cameraOffset == INVALID_OFFSET) {
        qCWarning(gpugllogging) << "Saved TransformCamera data has an invalid transform offset. Copy aborted.";
        return;
    }

    // Sync BufferObject
    auto* object = syncGPUObject(*buffer);
    if (object) {
        glCopyNamedBufferSubData(_transform._cameraBuffer, object->_buffer, savedTransform._cameraOffset, dstOffset, size);
        (void)CHECK_GL_ERROR();
    }
}
