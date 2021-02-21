//
//  Batch.cpp
//  interface/src/gpu
//
//  Created by Sam Gateau on 10/14/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "Batch.h"

#include <string.h>

#include <QDebug>
#include "ShaderConstants.h"

#include "GPULogging.h"

#if defined(NSIGHT_FOUND)
#include "nvToolsExt.h"

ProfileRangeBatch::ProfileRangeBatch(gpu::Batch& batch, const char* name) : _batch(batch) {
    _batch.pushProfileRange(name);
}

ProfileRangeBatch::~ProfileRangeBatch() {
    _batch.popProfileRange();
}
#endif

#define ADD_COMMAND(call) _commands.emplace_back(COMMAND_##call); _commandOffsets.emplace_back(_params.size());

using namespace gpu;

// FIXME make these backend / pipeline dependent.
static const int MAX_NUM_UNIFORM_BUFFERS = 14;
static const int MAX_NUM_RESOURCE_BUFFERS = 16;
static const int MAX_NUM_RESOURCE_TEXTURES = 16;

size_t Batch::_commandsMax{ BATCH_PREALLOCATE_MIN };
size_t Batch::_commandOffsetsMax{ BATCH_PREALLOCATE_MIN };
size_t Batch::_paramsMax{ BATCH_PREALLOCATE_MIN };
size_t Batch::_dataMax{ BATCH_PREALLOCATE_MIN };
size_t Batch::_objectsMax{ BATCH_PREALLOCATE_MIN };
size_t Batch::_drawCallInfosMax{ BATCH_PREALLOCATE_MIN };

Batch::Batch(const std::string& name) {
    _name = name;
    _commands.reserve(_commandsMax);
    _commandOffsets.reserve(_commandOffsetsMax);
    _params.reserve(_paramsMax);
    _data.reserve(_dataMax);
    _objects.reserve(_objectsMax);
    _drawCallInfos.reserve(_drawCallInfosMax);
    _mustUpdatePreviousModels = true;
}

Batch::~Batch() {
    _commandsMax = std::max(_commands.size(), _commandsMax);
    _commandOffsetsMax = std::max(_commandOffsets.size(), _commandOffsetsMax);
    _paramsMax = std::max(_params.size(), _paramsMax);
    _dataMax = std::max(_data.size(), _dataMax);
    _objectsMax = std::max(_objects.size(), _objectsMax);
    _drawCallInfosMax = std::max(_drawCallInfos.size(), _drawCallInfosMax);
}

void Batch::setName(const std::string& name) {
    _name = name;
}

void Batch::clear() {
    _commandsMax = std::max(_commands.size(), _commandsMax);
    _commandOffsetsMax = std::max(_commandOffsets.size(), _commandOffsetsMax);
    _paramsMax = std::max(_params.size(), _paramsMax);
    _dataMax = std::max(_data.size(), _dataMax);
    _objectsMax = std::max(_objects.size(), _objectsMax);
    _drawCallInfosMax = std::max(_drawCallInfos.size(), _drawCallInfosMax);

    _commands.clear();
    _commandOffsets.clear();
    _data.clear();
    _drawCallInfos.clear();
    _buffers.clear();
    _framebuffers.clear();
    _lambdas.clear();
    _names.clear();
    _namedData.clear();
    _objects.clear();
    _params.clear();
    _pipelines.clear();
    _profileRanges.clear();
    _queries.clear();
    _swapChains.clear();
    _streamFormats.clear();
    _textures.clear();
    _textureTables.clear();
    _transforms.clear();

    _name.clear();
    _invalidModel = true;
    _currentModel = Transform();
    _drawcallUniform = 0;
    _drawcallUniformReset = 0;
    _enableStereo = true;
    _enableSkybox = false;
    _mustUpdatePreviousModels = true; 
}

size_t Batch::cacheData(size_t size, const void* data) {
    size_t offset = _data.size();
    size_t numBytes = size;
    _data.resize(offset + numBytes);
    if (data) {
        memcpy(_data.data() + offset, data, size);
    }
    return offset;
}

void Batch::setDrawcallUniform(uint16_t uniform) {
    _drawcallUniform = uniform;
}
void Batch::setDrawcallUniformReset(uint16_t uniformReset) {
    _drawcallUniformReset = uniformReset;
}

void Batch::draw(Primitive primitiveType, uint32 numVertices, uint32 startVertex) {
    ADD_COMMAND(draw);

    _params.emplace_back(startVertex);
    _params.emplace_back(numVertices);
    _params.emplace_back(primitiveType);

    captureDrawCallInfo();
}

void Batch::drawIndexed(Primitive primitiveType, uint32 numIndices, uint32 startIndex) {
    ADD_COMMAND(drawIndexed);

    _params.emplace_back(startIndex);
    _params.emplace_back(numIndices);
    _params.emplace_back(primitiveType);

    captureDrawCallInfo();
}

void Batch::drawInstanced(uint32 numInstances, Primitive primitiveType, uint32 numVertices, uint32 startVertex, uint32 startInstance) {
    ADD_COMMAND(drawInstanced);

    _params.emplace_back(startInstance);
    _params.emplace_back(startVertex);
    _params.emplace_back(numVertices);
    _params.emplace_back(primitiveType);
    _params.emplace_back(numInstances);

    captureDrawCallInfo();
}

void Batch::drawIndexedInstanced(uint32 numInstances, Primitive primitiveType, uint32 numIndices, uint32 startIndex, uint32 startInstance) {
    ADD_COMMAND(drawIndexedInstanced);

    _params.emplace_back(startInstance);
    _params.emplace_back(startIndex);
    _params.emplace_back(numIndices);
    _params.emplace_back(primitiveType);
    _params.emplace_back(numInstances);

    captureDrawCallInfo();
}

void Batch::multiDrawIndirect(uint32 numCommands, Primitive primitiveType) {
    ADD_COMMAND(multiDrawIndirect);
    _params.emplace_back(numCommands);
    _params.emplace_back(primitiveType);

    captureDrawCallInfo();
}

void Batch::multiDrawIndexedIndirect(uint32 nbCommands, Primitive primitiveType) {
    ADD_COMMAND(multiDrawIndexedIndirect);
    _params.emplace_back(nbCommands);
    _params.emplace_back(primitiveType);

    captureDrawCallInfo();
}

void Batch::setInputFormat(const Stream::FormatPointer& format) {
    ADD_COMMAND(setInputFormat);

    _params.emplace_back(_streamFormats.cache(format));
}

void Batch::setInputBuffer(Slot channel, const BufferPointer& buffer, Offset offset, Offset stride) {
    ADD_COMMAND(setInputBuffer);

    _params.emplace_back(stride);
    _params.emplace_back(offset);
    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(channel);
}

void Batch::setInputBuffer(Slot channel, const BufferView& view) {
    setInputBuffer(channel, view._buffer, view._offset, Offset(view._stride));
}

void Batch::setInputStream(Slot startChannel, const BufferStream& stream) {
    if (stream.getNumBuffers()) {
        const Buffers& buffers = stream.getBuffers();
        const Offsets& offsets = stream.getOffsets();
        const Offsets& strides = stream.getStrides();
        for (unsigned int i = 0; i < buffers.size(); i++) {
            setInputBuffer(startChannel + i, buffers[i], offsets[i], strides[i]);
        }
    }
}

void Batch::setIndexBuffer(Type type, const BufferPointer& buffer, Offset offset) {
    ADD_COMMAND(setIndexBuffer);

    _params.emplace_back(offset);
    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(type);
}

void Batch::setIndexBuffer(const BufferView& buffer) {
    setIndexBuffer(buffer._element.getType(), buffer._buffer, buffer._offset);
}

void Batch::setIndirectBuffer(const BufferPointer& buffer, Offset offset, Offset stride) {
    ADD_COMMAND(setIndirectBuffer);

    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(offset);
    _params.emplace_back(stride);
}

void Batch::setModelTransform(const Transform& model) {
    ADD_COMMAND(setModelTransform);

    _currentModel = model;
    _previousModel = model;
    _invalidModel = true;
}

void Batch::setModelTransform(const Transform& model, const Transform& previousModel) {
    ADD_COMMAND(setModelTransform);

    _currentModel = model;
    _previousModel = previousModel;
    _invalidModel = true;
}

void Batch::setViewTransform(const Transform& view, bool camera) {
    ADD_COMMAND(setViewTransform);
    uint cameraFlag = camera ? 1 : 0;
    _params.emplace_back(_transforms.cache(view));
    _params.emplace_back(cameraFlag);
}

void Batch::setProjectionTransform(const Mat4& proj) {
    ADD_COMMAND(setProjectionTransform);

    _params.emplace_back(cacheData(sizeof(Mat4), &proj));
}

void Batch::setProjectionJitterEnabled(bool isProjectionEnabled) {
    _isJitterOnProjectionEnabled = isProjectionEnabled;
    pushProjectionJitterEnabled(_isJitterOnProjectionEnabled);
}

void Batch::pushProjectionJitterEnabled(bool isProjectionEnabled) {
    ADD_COMMAND(setProjectionJitterEnabled);
    _params.emplace_back(isProjectionEnabled & 1);
}

void Batch::popProjectionJitterEnabled() { 
    pushProjectionJitterEnabled(_isJitterOnProjectionEnabled);
}

void Batch::setProjectionJitterSequence(const Vec2* sequence, size_t count) {
    ADD_COMMAND(setProjectionJitterSequence);
    _params.emplace_back((uint)count);
    _params.emplace_back(cacheData(sizeof(Vec2) * count, sequence));
}

void Batch::setProjectionJitterScale(float scale) {
    ADD_COMMAND(setProjectionJitterScale);
    _params.emplace_back(scale);
}

void Batch::setViewportTransform(const Vec4i& viewport) {
    ADD_COMMAND(setViewportTransform);

    _params.emplace_back(cacheData(sizeof(Vec4i), &viewport));
}

void Batch::setDepthRangeTransform(float nearDepth, float farDepth) {
    ADD_COMMAND(setDepthRangeTransform);

    _params.emplace_back(farDepth);
    _params.emplace_back(nearDepth);
}

void Batch::saveViewProjectionTransform(uint32 saveSlot) {
    ADD_COMMAND(saveViewProjectionTransform);
    if (saveSlot >= MAX_TRANSFORM_SAVE_SLOT_COUNT) {
        qCWarning(gpulogging) << "Transform save slot" << saveSlot << "exceeds max save slot count of" << MAX_TRANSFORM_SAVE_SLOT_COUNT;
    }
    _params.emplace_back(saveSlot);
}

void Batch::setSavedViewProjectionTransform(uint32 saveSlot) {
    ADD_COMMAND(setSavedViewProjectionTransform);
    if (saveSlot >= MAX_TRANSFORM_SAVE_SLOT_COUNT) {
        qCWarning(gpulogging) << "Transform save slot" << saveSlot << "exceeds max save slot count of"
                              << MAX_TRANSFORM_SAVE_SLOT_COUNT;
    }
    _params.emplace_back(saveSlot);
}

void Batch::copySavedViewProjectionTransformToBuffer(uint32 saveSlot, const BufferPointer& buffer, Offset offset) {
    ADD_COMMAND(copySavedViewProjectionTransformToBuffer);
    if (saveSlot >= MAX_TRANSFORM_SAVE_SLOT_COUNT) {
        qCWarning(gpulogging) << "Transform save slot" << saveSlot << "exceeds max save slot count of"
                              << MAX_TRANSFORM_SAVE_SLOT_COUNT;
    }
    _params.emplace_back(saveSlot);
    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(offset);
}

void Batch::setPipeline(const PipelinePointer& pipeline) {
    ADD_COMMAND(setPipeline);

    _params.emplace_back(_pipelines.cache(pipeline));
}

void Batch::setStateBlendFactor(const Vec4& factor) {
    ADD_COMMAND(setStateBlendFactor);

    _params.emplace_back(factor.x);
    _params.emplace_back(factor.y);
    _params.emplace_back(factor.z);
    _params.emplace_back(factor.w);
}

void Batch::setStateScissorRect(const Vec4i& rect) {
    ADD_COMMAND(setStateScissorRect);

    _params.emplace_back(cacheData(sizeof(Vec4i), &rect));
}

void Batch::setUniformBuffer(uint32 slot, const BufferPointer& buffer, Offset offset, Offset size) {
    ADD_COMMAND(setUniformBuffer);
    if (slot >= MAX_NUM_UNIFORM_BUFFERS) {
        qCWarning(gpulogging) << "Slot" << slot << "exceeds max uniform buffer count of" << MAX_NUM_UNIFORM_BUFFERS;
    }
    _params.emplace_back(size);
    _params.emplace_back(offset);
    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(slot);
}

void Batch::setUniformBuffer(uint32 slot, const BufferView& view) {
    setUniformBuffer(slot, view._buffer, view._offset, view._size);
}

void Batch::setResourceBuffer(uint32 slot, const BufferPointer& buffer) {
    ADD_COMMAND(setResourceBuffer);
    if (slot >= MAX_NUM_RESOURCE_BUFFERS) {
        qCWarning(gpulogging) << "Slot" << slot << "exceeds max resources buffer count of" << MAX_NUM_RESOURCE_BUFFERS;
    }

    _params.emplace_back(_buffers.cache(buffer));
    _params.emplace_back(slot);
}

void Batch::setResourceTexture(uint32 slot, const TexturePointer& texture) {
    ADD_COMMAND(setResourceTexture);

    if (slot >= MAX_NUM_RESOURCE_TEXTURES) {
        qCWarning(gpulogging) << "Slot" << slot << "exceeds max texture count of" << MAX_NUM_RESOURCE_TEXTURES;
    }

    _params.emplace_back(_textures.cache(texture));
    _params.emplace_back(slot);
}

void Batch::setResourceTexture(uint32 slot, const TextureView& view) {
    setResourceTexture(slot, view._texture);
}

void Batch::setResourceTextureTable(const TextureTablePointer& textureTable, uint32 slot) {
    ADD_COMMAND(setResourceTextureTable);
    _params.emplace_back(_textureTables.cache(textureTable));
    _params.emplace_back(slot);
}

void Batch::setResourceFramebufferSwapChainTexture(uint32 slot, const FramebufferSwapChainPointer& framebuffer, unsigned int swapChainIndex, unsigned int renderBufferSlot) {
    ADD_COMMAND(setResourceFramebufferSwapChainTexture);

    _params.emplace_back(_swapChains.cache(framebuffer));
    _params.emplace_back(slot);
    _params.emplace_back(swapChainIndex);
    _params.emplace_back(renderBufferSlot);
}

void Batch::setFramebuffer(const FramebufferPointer& framebuffer) {
    ADD_COMMAND(setFramebuffer);

    _params.emplace_back(_framebuffers.cache(framebuffer));
}

void Batch::setFramebufferSwapChain(const FramebufferSwapChainPointer& framebuffer, unsigned int swapChainIndex) {
    ADD_COMMAND(setFramebufferSwapChain);

    _params.emplace_back(_swapChains.cache(framebuffer));
    _params.emplace_back(swapChainIndex);
}

void Batch::advance(const SwapChainPointer& swapChain) {
    ADD_COMMAND(advance);

    _params.emplace_back(_swapChains.cache(swapChain));
}

void Batch::clearFramebuffer(Framebuffer::Masks targets, const Vec4& color, float depth, int stencil, bool enableScissor) {
    ADD_COMMAND(clearFramebuffer);

    _params.emplace_back(enableScissor);
    _params.emplace_back(stencil);
    _params.emplace_back(depth);
    _params.emplace_back(color.w);
    _params.emplace_back(color.z);
    _params.emplace_back(color.y);
    _params.emplace_back(color.x);
    _params.emplace_back(targets);
}

void Batch::clearColorFramebuffer(Framebuffer::Masks targets, const Vec4& color, bool enableScissor) {
    clearFramebuffer(targets & Framebuffer::BUFFER_COLORS, color, 1.0f, 0, enableScissor);
}

void Batch::clearDepthFramebuffer(float depth, bool enableScissor) {
    clearFramebuffer(Framebuffer::BUFFER_DEPTH, Vec4(0.0f), depth, 0, enableScissor);
}

void Batch::clearStencilFramebuffer(int stencil, bool enableScissor) {
    clearFramebuffer(Framebuffer::BUFFER_STENCIL, Vec4(0.0f), 1.0f, stencil, enableScissor);
}

void Batch::clearDepthStencilFramebuffer(float depth, int stencil, bool enableScissor) {
    clearFramebuffer(Framebuffer::BUFFER_DEPTHSTENCIL, Vec4(0.0f), depth, stencil, enableScissor);
}

void Batch::blit(const FramebufferPointer& src, const Vec4i& srcViewport,
    const FramebufferPointer& dst, const Vec4i& dstViewport) {
    ADD_COMMAND(blit);

    _params.emplace_back(_framebuffers.cache(src));
    _params.emplace_back(srcViewport.x);
    _params.emplace_back(srcViewport.y);
    _params.emplace_back(srcViewport.z);
    _params.emplace_back(srcViewport.w);
    _params.emplace_back(_framebuffers.cache(dst));
    _params.emplace_back(dstViewport.x);
    _params.emplace_back(dstViewport.y);
    _params.emplace_back(dstViewport.z);
    _params.emplace_back(dstViewport.w);
}

void Batch::generateTextureMips(const TexturePointer& texture) {
    ADD_COMMAND(generateTextureMips);

    _params.emplace_back(_textures.cache(texture));
}

void Batch::generateTextureMipsWithPipeline(const TexturePointer& texture, int numMips) {
    ADD_COMMAND(generateTextureMipsWithPipeline);

    _params.emplace_back(_textures.cache(texture));
    _params.emplace_back(numMips);
}

void Batch::beginQuery(const QueryPointer& query) {
    ADD_COMMAND(beginQuery);

    _params.emplace_back(_queries.cache(query));
}

void Batch::endQuery(const QueryPointer& query) {
    ADD_COMMAND(endQuery);

    _params.emplace_back(_queries.cache(query));
}

void Batch::getQuery(const QueryPointer& query) {
    ADD_COMMAND(getQuery);

    _params.emplace_back(_queries.cache(query));
}

void Batch::resetStages() {
    ADD_COMMAND(resetStages);
}

void Batch::disableContextViewCorrection() {
    ADD_COMMAND(disableContextViewCorrection);
}

void Batch::restoreContextViewCorrection() {
    ADD_COMMAND(restoreContextViewCorrection);
}

void Batch::disableContextStereo() {
    ADD_COMMAND(disableContextStereo);
}

void Batch::restoreContextStereo() {
    ADD_COMMAND(restoreContextStereo);
}

void Batch::runLambda(std::function<void()> f) {
    ADD_COMMAND(runLambda);
    _params.emplace_back(_lambdas.cache(f));
}

void Batch::startNamedCall(const std::string& name) {
    ADD_COMMAND(startNamedCall);
    _params.emplace_back(_names.cache(name));

    _currentNamedCall = name;
}

void Batch::stopNamedCall() {
    ADD_COMMAND(stopNamedCall);

    _currentNamedCall.clear();
}

void Batch::enableStereo(bool enable) {
    _enableStereo = enable;
}

bool Batch::isStereoEnabled() const {
    return _enableStereo;
}

void Batch::enableSkybox(bool enable) {
    _enableSkybox = enable;
}

bool Batch::isSkyboxEnabled() const {
    return _enableSkybox;
}

void Batch::setupNamedCalls(const std::string& instanceName, NamedBatchData::Function function) {
    NamedBatchData& instance = _namedData[instanceName];
    if (!instance.function) {
        instance.function = function;
    }

    captureNamedDrawCallInfo(instanceName);
}

const BufferPointer& Batch::getNamedBuffer(const std::string& instanceName, uint8_t index) {
    NamedBatchData& instance = _namedData[instanceName];
    if (instance.buffers.size() <= index) {
        instance.buffers.resize(index + 1);
    }
    if (!instance.buffers[index]) {
        instance.buffers[index] = std::make_shared<Buffer>();
    }
    return instance.buffers[index];
}

Batch::DrawCallInfoBuffer& Batch::getDrawCallInfoBuffer() {
    if (_currentNamedCall.empty()) {
        return _drawCallInfos;
    } else {
        return _namedData[_currentNamedCall].drawCallInfos;
    }
}

const Batch::DrawCallInfoBuffer& Batch::getDrawCallInfoBuffer() const {
    if (_currentNamedCall.empty()) {
        return _drawCallInfos;
    } else {
        auto it = _namedData.find(_currentNamedCall);
        Q_ASSERT_X(it != _namedData.end(), Q_FUNC_INFO, (_currentNamedCall + " not in _namedData.").data());
        return it->second.drawCallInfos;
    }
}

void Batch::captureDrawCallInfoImpl() {
    if (_invalidModel) {
        TransformObject object;
        _currentModel.getMatrix(object._model);
        _previousModel.getMatrix(object._previousModel);

        // FIXME - we don't want to be using glm::inverse() here but it fixes the flickering issue we are
        // seeing with planky blocks in toybox. Our implementation of getInverseMatrix() is buggy in cases
        // of non-uniform scale. We need to fix that. In the mean time, glm::inverse() works.
        //_model.getInverseMatrix(_object._modelInverse);
        //_previousModel.getInverseMatrix(_object._previousModelInverse);
        object._modelInverse = glm::inverse(object._model);
        object._previousModelInverse = glm::inverse(object._previousModel);

        _objects.emplace_back(object);

        // Flag is clean
        _invalidModel = false;
    }

    auto& drawCallInfos = getDrawCallInfoBuffer();
    drawCallInfos.emplace_back((uint16)_objects.size() - 1, _drawcallUniform);
    _drawcallUniform = _drawcallUniformReset;
}

void Batch::captureDrawCallInfo() {
    if (!_currentNamedCall.empty()) {
        // If we are processing a named call, we don't want to register the raw draw calls
        return;
    }

    captureDrawCallInfoImpl();
}

void Batch::captureNamedDrawCallInfo(std::string name) {
    std::swap(_currentNamedCall, name);  // Set and save _currentNamedCall
    captureDrawCallInfoImpl();
    std::swap(_currentNamedCall, name);  // Restore _currentNamedCall
}

// Debugging
void Batch::pushProfileRange(const char* name) {
#if defined(NSIGHT_FOUND)
    ADD_COMMAND(pushProfileRange);
    _params.emplace_back(_profileRanges.cache(name));
#endif
}

void Batch::popProfileRange() {
#if defined(NSIGHT_FOUND)
    ADD_COMMAND(popProfileRange);
#endif
}

void Batch::_glUniform1i(int32 location, int32 v0) {
    if (location < 0) {
        return;
    }
    ADD_COMMAND(glUniform1i);
    _params.emplace_back(v0);
    _params.emplace_back(location);
}

void Batch::_glUniform1f(int32 location, float v0) {
    if (location < 0) {
        return;
    }
    ADD_COMMAND(glUniform1f);
    _params.emplace_back(v0);
    _params.emplace_back(location);
}

void Batch::_glUniform2f(int32 location, float v0, float v1) {
    ADD_COMMAND(glUniform2f);

    _params.emplace_back(v1);
    _params.emplace_back(v0);
    _params.emplace_back(location);
}

void Batch::_glUniform3f(int32 location, float v0, float v1, float v2) {
    ADD_COMMAND(glUniform3f);

    _params.emplace_back(v2);
    _params.emplace_back(v1);
    _params.emplace_back(v0);
    _params.emplace_back(location);
}

void Batch::_glUniform4f(int32 location, float v0, float v1, float v2, float v3) {
    ADD_COMMAND(glUniform4f);

    _params.emplace_back(v3);
    _params.emplace_back(v2);
    _params.emplace_back(v1);
    _params.emplace_back(v0);
    _params.emplace_back(location);
}

void Batch::_glUniform3fv(int32 location, int count, const float* value) {
    ADD_COMMAND(glUniform3fv);

    const int VEC3_SIZE = 3 * sizeof(float);
    _params.emplace_back(cacheData(count * VEC3_SIZE, value));
    _params.emplace_back(count);
    _params.emplace_back(location);
}

void Batch::_glUniform4fv(int32 location, int count, const float* value) {
    ADD_COMMAND(glUniform4fv);

    const int VEC4_SIZE = 4 * sizeof(float);
    _params.emplace_back(cacheData(count * VEC4_SIZE, value));
    _params.emplace_back(count);
    _params.emplace_back(location);
}

void Batch::_glUniform4iv(int32 location, int count, const int32* value) {
    ADD_COMMAND(glUniform4iv);

    const int VEC4_SIZE = 4 * sizeof(int);
    _params.emplace_back(cacheData(count * VEC4_SIZE, value));
    _params.emplace_back(count);
    _params.emplace_back(location);
}

void Batch::_glUniformMatrix3fv(int32 location, int count, uint8 transpose, const float* value) {
    ADD_COMMAND(glUniformMatrix3fv);

    const int MATRIX3_SIZE = 9 * sizeof(float);
    _params.emplace_back(cacheData(count * MATRIX3_SIZE, value));
    _params.emplace_back(transpose);
    _params.emplace_back(count);
    _params.emplace_back(location);
}

void Batch::_glUniformMatrix4fv(int32 location, int count, uint8 transpose, const float* value) {
    ADD_COMMAND(glUniformMatrix4fv);

    const int MATRIX4_SIZE = 16 * sizeof(float);
    _params.emplace_back(cacheData(count * MATRIX4_SIZE, value));
    _params.emplace_back(transpose);
    _params.emplace_back(count);
    _params.emplace_back(location);
}

void Batch::_glColor4f(float red, float green, float blue, float alpha) {
    ADD_COMMAND(glColor4f);

    _params.emplace_back(alpha);
    _params.emplace_back(blue);
    _params.emplace_back(green);
    _params.emplace_back(red);
}

void Batch::finishFrame(BufferUpdates& updates) {
    PROFILE_RANGE(render_gpu, __FUNCTION__);

    for (auto& mapItem : _namedData) {
        auto& name = mapItem.first;
        auto& instance = mapItem.second;

        startNamedCall(name);
        instance.process(*this);
        stopNamedCall();
    }

    for (auto& namedCallData : _namedData) {
        for (auto& buffer : namedCallData.second.buffers) {
            if (!buffer || !buffer->isDirty()) {
                continue;
            }
            updates.emplace_back(buffer->getUpdate());
        }
    }

    for (auto& bufferCacheItem : _buffers._items) {
        const BufferPointer& buffer = bufferCacheItem._data;
        if (!buffer || !buffer->isDirty()) {
            continue;
        }
        updates.emplace_back(buffer->getUpdate());
    }
}

void Batch::flush() {
    PROFILE_RANGE(render_gpu, __FUNCTION__);
    for (auto& mapItem : _namedData) {
        auto& name = mapItem.first;
        auto& instance = mapItem.second;

        auto& self = const_cast<Batch&>(*this);
        self.startNamedCall(name);
        instance.process(self);
        self.stopNamedCall();
    }

    for (auto& namedCallData : _namedData) {
        for (auto& buffer : namedCallData.second.buffers) {
            if (!buffer) {
                continue;
            }
            if (!buffer->isDirty()) {
                continue;
            }
            buffer->flush();
        }
    }

    for (auto& bufferCacheItem : _buffers._items) {
        const BufferPointer& buffer = bufferCacheItem._data;
        if (!buffer) {
            continue;
        }
        if (!buffer->isDirty()) {
            continue;
        }
        buffer->flush();
    }
}
