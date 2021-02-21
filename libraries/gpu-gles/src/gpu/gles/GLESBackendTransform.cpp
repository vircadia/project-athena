//
//  GLESBackendTransform.cpp
//  libraries/gpu-gl-android/src/gpu/gles
//
//  Created by Gabriel Calero & Cristian Duarte on 9/28/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GLESBackend.h"

#include "gpu/gl/GLBuffer.h"

using namespace gpu;
using namespace gpu::gles;

void GLESBackend::initTransform() {
    glGenBuffers(1, &_transform._objectBuffer);
    glGenBuffers(1, &_transform._cameraBuffer);
    glGenBuffers(1, &_transform._drawCallInfoBuffer);
    glGenTextures(1, &_transform._objectBufferTexture);
    size_t cameraSize = sizeof(TransformStageState::CameraBufferElement);
    if (UNIFORM_BUFFER_OFFSET_ALIGNMENT > 0) {
        while (_transform._cameraUboSize < cameraSize) {
            _transform._cameraUboSize += UNIFORM_BUFFER_OFFSET_ALIGNMENT;
        }
    }
}

void GLESBackend::transferTransformState(const Batch& batch) const {
    // FIXME not thread safe
    static std::vector<uint8_t> bufferData;
    if (!_transform._cameras.empty()) {
        bufferData.resize(_transform._cameraUboSize * _transform._cameras.size());
        for (size_t i = 0; i < _transform._cameras.size(); ++i) {
            memcpy(bufferData.data() + (_transform._cameraUboSize * i), &_transform._cameras[i], sizeof(TransformStageState::CameraBufferElement));
        }
        glBindBuffer(GL_UNIFORM_BUFFER, _transform._cameraBuffer);
        glBufferData(GL_UNIFORM_BUFFER, bufferData.size(), bufferData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    if (!batch._objects.empty()) {
        glBindBuffer(GL_TEXTURE_BUFFER, _transform._objectBuffer);
        glBufferData(GL_TEXTURE_BUFFER, batch._objects.size() * sizeof(Batch::TransformObject), batch._objects.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
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

        glBindBuffer(GL_ARRAY_BUFFER, _transform._drawCallInfoBuffer);
        glBufferData(GL_ARRAY_BUFFER, bufferData.size(), bufferData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glActiveTexture(GL_TEXTURE0 +  slot::texture::ObjectTransforms);
    glBindTexture(GL_TEXTURE_BUFFER, _transform._objectBufferTexture);
    if (!batch._objects.empty()) {
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, _transform._objectBuffer);
    }
    CHECK_GL_ERROR();

    // Make sure the current Camera offset is unknown before render Draw
    _transform._currentCameraOffset = INVALID_OFFSET;
}


void GLESBackend::updateTransform(const Batch& batch) {
    _transform.update(_commandIndex, _stereo);

    auto& drawCallInfoBuffer = batch.getDrawCallInfoBuffer();
    if (batch._currentNamedCall.empty()) {
        auto& drawCallInfo = drawCallInfoBuffer[_currentDraw];
        if (_transform._enabledDrawcallInfoBuffer) {
            glDisableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO); // Make sure attrib array is disabled
            _transform._enabledDrawcallInfoBuffer = false;
        }
        glVertexAttribI4i(gpu::Stream::DRAW_CALL_INFO, drawCallInfo.index, drawCallInfo.unused, 0, 0);
        //glVertexAttribI2i(gpu::Stream::DRAW_CALL_INFO, drawCallInfo.index, drawCallInfo.unused);
    } else {
        if (!_transform._enabledDrawcallInfoBuffer) {
            glEnableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO); // Make sure attrib array is enabled
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
            glVertexAttribDivisor(gpu::Stream::DRAW_CALL_INFO, (isStereo() ? 2 : 1));
#else
            glVertexAttribDivisor(gpu::Stream::DRAW_CALL_INFO, 1);
#endif
            _transform._enabledDrawcallInfoBuffer = true;
        }
        glBindBuffer(GL_ARRAY_BUFFER, _transform._drawCallInfoBuffer);
        glVertexAttribIPointer(gpu::Stream::DRAW_CALL_INFO, 2, GL_UNSIGNED_SHORT, 0, _transform._drawCallInfoOffsets[batch._currentNamedCall]);
    }

    (void)CHECK_GL_ERROR();
}

void GLESBackend::do_copySavedViewProjectionTransformToBuffer(const Batch& batch, size_t paramOffset) {
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
        glBindBuffer(GL_COPY_READ_BUFFER, _transform._cameraBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, object->_buffer);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, savedTransform._cameraOffset, dstOffset, size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        (void)CHECK_GL_ERROR();
    }
}
