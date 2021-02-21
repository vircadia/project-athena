//
//  GLBackendTransform.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 3/8/2015.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GLBackend.h"

using namespace gpu;
using namespace gpu::gl;

// Transform Stage
void GLBackend::do_setModelTransform(const Batch& batch, size_t paramOffset) {
}

void GLBackend::do_setViewTransform(const Batch& batch, size_t paramOffset) {
    _transform._viewProjectionState._view = batch._transforms.get(batch._params[paramOffset]._uint); 
    // View history is only supported with saved transforms and if setViewTransform is called (and not setSavedViewProjectionTransform)
    // then, in consequence, the view will NOT be corrected in the present thread. In which case
    // the previousCorrectedView should be the same as the view.
    _transform._viewProjectionState._previousCorrectedView = _transform._viewProjectionState._view;
    _transform._viewProjectionState._previousProjection = _transform._viewProjectionState._projection;
    _transform._viewProjectionState._viewIsCamera = batch._params[paramOffset + 1]._uint != 0;
    _transform._invalidView = true;
    // The current view / proj doesn't correspond to a saved camera slot
    _transform._currentSavedTransformSlot = INVALID_SAVED_CAMERA_SLOT;
}

void GLBackend::do_setProjectionTransform(const Batch& batch, size_t paramOffset) {
     memcpy(glm::value_ptr(_transform._viewProjectionState._projection), batch.readData(batch._params[paramOffset]._uint), sizeof(Mat4)); 
     _transform._invalidProj = true;
    // The current view / proj doesn't correspond to a saved camera slot
    _transform._currentSavedTransformSlot = INVALID_SAVED_CAMERA_SLOT;
}

void GLBackend::do_setProjectionJitterEnabled(const Batch& batch, size_t paramOffset) {
    _transform._projectionJitter._isEnabled = (batch._params[paramOffset]._int & 1) != 0;
    _transform._invalidProj = true;
    // The current view / proj doesn't correspond to a saved camera slot
    _transform._currentSavedTransformSlot = INVALID_SAVED_CAMERA_SLOT;
}

void GLBackend::do_setProjectionJitterSequence(const Batch& batch, size_t paramOffset) {
    auto count = batch._params[paramOffset + 0]._uint;
    auto& projectionJitter = _transform._projectionJitter;
    projectionJitter._offsetSequence.resize(count);
    if (count) {
        memcpy(projectionJitter._offsetSequence.data(), batch.readData(batch._params[paramOffset + 1]._uint), sizeof(Vec2) * count);
        projectionJitter._offset = projectionJitter._offsetSequence[projectionJitter._currentSampleIndex  % count];
    } else {
        projectionJitter._offset = Vec2(0.0f);
    }
}

void GLBackend::do_setProjectionJitterScale(const Batch& batch, size_t paramOffset) {
    // Should be 2 for one pixel amplitude as clip space is between -1 and 1, but lower values give less blur
    // but more aliasing...
    _transform._projectionJitter._scale = 2.0f * batch._params[paramOffset + 0]._float;
}

void GLBackend::do_setViewportTransform(const Batch& batch, size_t paramOffset) {
    memcpy(glm::value_ptr(_transform._viewport), batch.readData(batch._params[paramOffset]._uint), sizeof(Vec4i));

#ifdef GPU_STEREO_DRAWCALL_INSTANCED
    {
        ivec4& vp = _transform._viewport;
        glViewport(vp.x, vp.y, vp.z, vp.w);

        // Where we assign the GL viewport
        if (_stereo.isStereo()) {
            vp.z /= 2;
            if (_stereo._pass) {
                vp.x += vp.z;
            }
        }
    }
#else
    if (!_inRenderTransferPass && !isStereo()) {
        ivec4& vp = _transform._viewport;
        glViewport(vp.x, vp.y, vp.z, vp.w);
    }
#endif

    // The Viewport is tagged invalid because the CameraTransformUBO is not up to date and will need update on next drawcall
    _transform._invalidViewport = true;
}

void GLBackend::do_setDepthRangeTransform(const Batch& batch, size_t paramOffset) {

    Vec2 depthRange(batch._params[paramOffset + 1]._float, batch._params[paramOffset + 0]._float);

    if ((depthRange.x != _transform._depthRange.x) || (depthRange.y != _transform._depthRange.y)) {
        _transform._depthRange = depthRange;
        
        glDepthRangef(depthRange.x, depthRange.y);
    }
}

void GLBackend::killTransform() {
    glDeleteBuffers(1, &_transform._objectBuffer);
    glDeleteBuffers(1, &_transform._cameraBuffer);
    glDeleteBuffers(1, &_transform._drawCallInfoBuffer);
    glDeleteTextures(1, &_transform._objectBufferTexture);
}

void GLBackend::syncTransformStateCache() {
    _transform._invalidViewport = true;
    _transform._invalidProj = true;
    _transform._invalidView = true;

    glGetIntegerv(GL_VIEWPORT, (GLint*) &_transform._viewport);

    glGetFloatv(GL_DEPTH_RANGE, (GLfloat*)&_transform._depthRange);

    Mat4 modelView;
    auto modelViewInv = glm::inverse(modelView);
    _transform._viewProjectionState._view.evalFromRawMatrix(modelViewInv);

    glDisableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO);
    _transform._enabledDrawcallInfoBuffer = false;
}

void GLBackend::TransformStageState::pushCameraBufferElement(const StereoState& stereo, const StereoState& prevStereo, TransformCameras& cameras) const {
    const float jitterAmplitude = _projectionJitter._scale;
    const Vec2 jitterScale = Vec2(jitterAmplitude * float(_projectionJitter._isEnabled & 1)) / Vec2(_viewport.z, _viewport.w);
    const Vec2 jitter = jitterScale * _projectionJitter._offset;

    if (stereo.isStereo()) {
#ifdef GPU_STEREO_CAMERA_BUFFER
        cameras.push_back(CameraBufferElement(_camera.getEyeCamera(0, stereo, prevStereo, _viewProjectionState._correctedView,
                                                                   _viewProjectionState._previousCorrectedView, jitter),
                                              _camera.getEyeCamera(1, stereo, prevStereo, _viewProjectionState._correctedView,
                                                                   _viewProjectionState._previousCorrectedView, jitter)));
#else
        cameras.push_back((_camera.getEyeCamera(0, stereo, prevStereo, _viewProjectionState._correctedView,
                                                _viewProjectionState._previousCorrectedView, jitter)));
        cameras.push_back((_camera.getEyeCamera(1, stereo, prevStereo, _viewProjectionState._correctedView,
                                                _viewProjectionState._previousCorrectedView, jitter)));
#endif
    } else {
#ifdef GPU_STEREO_CAMERA_BUFFER
        cameras.push_back(CameraBufferElement(
            _camera.getMonoCamera(_skybox, _viewProjectionState._correctedView, _viewProjectionState._previousCorrectedView,
                                  _viewProjectionState._previousProjection, jitter)));
#else
        cameras.push_back((_camera.getMonoCamera(_skybox, _viewProjectionState._correctedView,
                                                 _viewProjectionState._previousCorrectedView, _viewProjectionState._previousProjection,
                                                 jitter)));
#endif
    }
}

void GLBackend::preUpdateTransform() {
    _transform.preUpdate(_commandIndex, _stereo, _prevStereo);
}

void GLBackend::TransformStageState::preUpdate(size_t commandIndex, const StereoState& stereo, const StereoState& prevStereo) {
    // Check all the dirty flags and update the state accordingly
    if (_invalidViewport) {
        _camera._viewport = glm::vec4(_viewport);
    }

    if (_invalidProj) {
        _camera._projection = _viewProjectionState._projection;
    }

    if (_invalidView) {
        // Apply the correction
        if (_viewProjectionState._viewIsCamera && (_viewCorrectionEnabled && _presentFrame.correction != glm::mat4())) {
            Transform::mult(_viewProjectionState._correctedView, _viewProjectionState._view, _presentFrame.correctionInverse);
        } else {
            _viewProjectionState._correctedView = _viewProjectionState._view;
        }

        if (_skybox) {
            _viewProjectionState._correctedView.setTranslation(vec3());
        }
        // This is when the _view matrix gets assigned
        _viewProjectionState._correctedView.getInverseMatrix(_camera._view);
    }

    if (_invalidView || _invalidProj || _invalidViewport) {
        size_t offset = _cameraUboSize * _cameras.size();
        _cameraOffsets.push_back(TransformStageState::Pair(commandIndex, offset));

        pushCameraBufferElement(stereo, prevStereo, _cameras);
        if (_currentSavedTransformSlot != INVALID_SAVED_CAMERA_SLOT) {
            // Save the offset of the saved camera slot in the camera buffer. Can be used to copy
            // that data, or (in the future) to reuse the offset.
            _savedTransforms[_currentSavedTransformSlot]._cameraOffset = offset;
        }
    }

    // Flags are clean
    _invalidView = _invalidProj = _invalidViewport = false;
}

void GLBackend::TransformStageState::update(size_t commandIndex, const StereoState& stereo) const {
    size_t offset = INVALID_OFFSET;
    while ((_camerasItr != _cameraOffsets.end()) && (commandIndex >= (*_camerasItr).first)) {
        offset = (*_camerasItr).second;
        _currentCameraOffset = offset;
        ++_camerasItr;
    }

    if (offset != INVALID_OFFSET) {
#ifdef GPU_STEREO_CAMERA_BUFFER
        bindCurrentCamera(0);
#else 
        if (!stereo.isStereo()) {
            bindCurrentCamera(0);
        }
#endif
    }
    (void)CHECK_GL_ERROR();
}

void GLBackend::TransformStageState::bindCurrentCamera(int eye) const {
    if (_currentCameraOffset != INVALID_OFFSET) {
        static_assert(slot::buffer::Buffer::CameraTransform >= MAX_NUM_UNIFORM_BUFFERS, "TransformCamera may overlap pipeline uniform buffer slots. Invalidate uniform buffer slot cache for safety (call _uniform._buffers[TRANSFORM_CAMERA_SLOT].reset()).");
        glBindBufferRange(GL_UNIFORM_BUFFER, slot::buffer::Buffer::CameraTransform, _cameraBuffer, _currentCameraOffset + eye * _cameraUboSize, sizeof(CameraBufferElement));
    }
}

void GLBackend::resetTransformStage() {
    glDisableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO);
    _transform._enabledDrawcallInfoBuffer = false;
}

void GLBackend::do_saveViewProjectionTransform(const Batch& batch, size_t paramOffset) {
    auto slotId = batch._params[paramOffset + 0]._uint;
    slotId = std::min<gpu::uint32>(slotId, gpu::Batch::MAX_TRANSFORM_SAVE_SLOT_COUNT);

    auto& savedTransform = _transform._savedTransforms[slotId];
    savedTransform._cameraOffset = INVALID_OFFSET;
    _transform._currentSavedTransformSlot = slotId;
    // If we are saving this transform to a save slot, then it means we are tracking the history of the view
    // so copy the previous corrected view to the transform state.
    _transform._viewProjectionState._previousCorrectedView = savedTransform._state._previousCorrectedView;
    _transform._viewProjectionState._previousProjection = savedTransform._state._previousProjection;
    preUpdateTransform();
    savedTransform._state.copyExceptPrevious(_transform._viewProjectionState);
}

void GLBackend::do_setSavedViewProjectionTransform(const Batch& batch, size_t paramOffset) {
    auto slotId = batch._params[paramOffset + 0]._uint;
    slotId = std::min<gpu::uint32>(slotId, gpu::Batch::MAX_TRANSFORM_SAVE_SLOT_COUNT);

    _transform._viewProjectionState = _transform._savedTransforms[slotId]._state;
    _transform._invalidView = true;
    _transform._invalidProj = true;
    _transform._currentSavedTransformSlot = slotId;
}
