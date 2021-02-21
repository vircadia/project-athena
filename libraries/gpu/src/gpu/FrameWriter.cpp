//
//  Created by Bradley Austin Davis on 2018/10/14
//  Copyright 2013-2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "FrameIO.h"
#include "Frame.h"
#include "Batch.h"
#include "TextureTable.h"
#include <nlohmann/json.hpp>
#include <unordered_map>

#include "FrameIOKeys.h"

namespace gpu {

using json = nlohmann::json;

class Serializer {
public:
    const std::string filename;
    const TextureCapturer textureCapturer;
    std::unordered_map<ShaderPointer, uint32_t> shaderMap;
    std::unordered_map<ShaderPointer, uint32_t> programMap;
    std::unordered_map<TexturePointer, uint32_t> textureMap;
    std::unordered_map<TextureTablePointer, uint32_t> textureTableMap;
    std::unordered_map<BufferPointer, uint32_t> bufferMap;
    std::unordered_map<Stream::FormatPointer, uint32_t> formatMap;
    std::unordered_map<PipelinePointer, uint32_t> pipelineMap;
    std::unordered_map<FramebufferPointer, uint32_t> framebufferMap;
    std::unordered_map<SwapChainPointer, uint32_t> swapchainMap;
    std::unordered_map<QueryPointer, uint32_t> queryMap;
    std::unordered_set<TexturePointer> captureTextures;
    hfb::Buffer binaryBuffer;
    hfb::StorageBuilders ktxBuilders;

    Serializer(const std::string& basename, const TextureCapturer& capturer) : filename(basename + hfb::EXTENSION), textureCapturer(capturer) {}

    template <typename T>
    static uint32_t getGlobalIndex(const T& value, std::unordered_map<T, uint32_t>& map) {
        if (map.count(value) == 0) {
            uint32_t result = (uint32_t)map.size();
            map[value] = result;
            return result;
        }
        return map[value];
    }

    template <typename T>
    static json serializePointerCache(const typename Batch::Cache<T>::Vector& cache, std::unordered_map<T, uint32_t>& map) {
        json result = json::array();
        const auto count = cache._items.size();
        for (uint32_t i = 0; i < count; ++i) {
            const auto& cacheEntry = cache._items[i];
            const auto& val = cacheEntry._data;
            result.push_back(getGlobalIndex(val, map));
        }
        return result;
    }

    template <typename T, typename TT = const T&>
    static json serializeDataCache(const typename Batch::Cache<T>::Vector& cache,
                                   std::function<TT(const T&)> f = [](const T& t) -> TT { return t; }) {
        json result = json::array();
        const auto count = cache._items.size();
        for (uint32_t i = 0; i < count; ++i) {
            const auto& cacheEntry = cache._items[i];
            const auto& val = cacheEntry._data;
            result.push_back(f(val));
        }
        return result;
    }

    template <typename T, typename TT = const T&>
    static json writeVector(const std::vector<T>& v,
                            const std::function<TT(const T&)>& f = [](const T& t) -> TT { return t; }) {
        auto node = json::array();
        for (const auto& e : v) {
            node.push_back(f(e));
        }
        return node;
    }

    template <typename T>
    static json writeNumericVector(const std::vector<T>& v) {
        return writeVector<T, const T&>(v);
    }

    template <typename T>
    static json writeUintVector(const std::vector<T>& v) {
        return writeVector<T, const uint32_t&>(v, [](const T& t) -> const uint32_t& {
            return reinterpret_cast<const uint32_t&>(t);
        });
    }

    template <size_t N = 1>
    static json writeFloatArray(const float* f) {
        json result = json::array();
        for (size_t i = 0; i < N; ++i) {
            result.push_back(f[i]);
        }
        return result;
    }

    template <typename T>
    static std::vector<T> mapToVector(const std::unordered_map<T, uint32_t>& map) {
        std::vector<T> result;
        result.resize(map.size());
        for (const auto& entry : map) {
            if (result[entry.second]) {
                throw std::runtime_error("Invalid map");
            }
            result[entry.second] = entry.first;
        }
        return result;
    }

    template <typename T, typename F>
    std::function<json(const T&)> memberWriter(F f) {
        return std::bind(f, this, std::placeholders::_1);
    }

    void writeFrame(const Frame& frame);
    json writeBatch(const Batch& batch);
    json writeTextureTable(const TextureTablePointer& textureTable);
    json writeTextureView(const TextureView& textureView);
    json writeFramebuffer(const FramebufferPointer& texture);
    json writePipeline(const PipelinePointer& pipeline);
    json writeSwapchain(const SwapChainPointer& swapchain);
    json writeProgram(const ShaderPointer& program);
    json writeNamedBatchData(const Batch::NamedBatchData& namedData);

    void findCapturableTextures(const Frame& frame);
    void writeBinaryBlob();
    static std::string toBase64(const std::vector<uint8_t>& v);
    static json writeIrradiance(const SHPointer& irradiance);
    static json writeMat4(const glm::mat4& m) {
        static const glm::mat4 IDENTITY;
        if (m == IDENTITY) {
            return json();
        }
        return writeFloatArray<16>(&m[0][0]);
    }
    static json writeVec4(const glm::vec4& v) { return writeFloatArray<4>(&v[0]); }
    static json writeVec3(const glm::vec3& v) { return writeFloatArray<3>(&v[0]); }
    static json writeVec2(const glm::vec2& v) { return writeFloatArray<2>(&v[0]); }
    static json writeTransform(const Transform& t) { return writeMat4(t.getMatrix()); }
    static json writeCommand(size_t index, const Batch& batch);
    static json writeSampler(const Sampler& sampler);
    json writeTexture(const TexturePointer& texture);
    static json writeFormat(const Stream::FormatPointer& format);
    static json writeQuery(const QueryPointer& query);
    static json writeShader(const ShaderPointer& shader);
    static json writeBuffer(const BufferPointer& bufferPointer);

    static const TextureView DEFAULT_TEXTURE_VIEW;
    static const Sampler DEFAULT_SAMPLER;

    template <typename T, typename F>
    void serializeMap(json& frameNode, const char* key, const std::unordered_map<T, uint32_t>& map, F f) {
        auto& node = frameNode[key] = json::array();
        for (const auto& item : mapToVector(map)) {
            node.push_back(f(item));
        }
    }
};

void writeFrame(const std::string& filename, const FramePointer& frame, const TextureCapturer& capturer) {
    Serializer(filename, capturer).writeFrame(*frame);
}

}  // namespace gpu

using namespace gpu;

const TextureView Serializer::DEFAULT_TEXTURE_VIEW = TextureView();
const Sampler Serializer::DEFAULT_SAMPLER = Sampler();

std::string Serializer::toBase64(const std::vector<uint8_t>& v) {
    return QByteArray((const char*)v.data(), (int)v.size()).toBase64().toStdString();
}

json Serializer::writeCommand(size_t index, const Batch& batch) {
    const auto& command = batch._commands[index];
    auto offset = batch._commandOffsets[index];
    auto endOffset = batch._params.size();
    if ((index + 1) < batch._commands.size()) {
        endOffset = batch._commandOffsets[index + 1];
    }

    json result = json::array();
    result.push_back(keys::COMMAND_NAMES[command]);
    while (offset < endOffset) {
        result.push_back(batch._params[offset]._size);
        ++offset;
    }
    return result;
}

json Serializer::writeNamedBatchData(const Batch::NamedBatchData& namedData) {
    json result = json::object();
    auto& buffersNode = result[keys::buffers] = json::array();
    for (const auto& buffer : namedData.buffers) {
        buffersNode.push_back(getGlobalIndex(buffer, bufferMap));
    }
    result[keys::drawCallInfos] = writeUintVector(namedData.drawCallInfos);
    return result;
}

json Serializer::writeBatch(const Batch& batch) {
    json batchNode;
    static const Batch DEFAULT_BATCH;

    batchNode[keys::name] = batch.getName();
    if (batch._enableSkybox != DEFAULT_BATCH._enableSkybox) {
        batchNode[keys::skybox] = batch._enableSkybox;
    }
    if (batch._enableStereo != DEFAULT_BATCH._enableStereo) {
        batchNode[keys::stereo] = batch._enableStereo;
    }
    if (batch._isJitterOnProjectionEnabled != DEFAULT_BATCH._isJitterOnProjectionEnabled) {
        batchNode[keys::isJitterOnProjectionEnabled] = batch._isJitterOnProjectionEnabled;
    }
    if (batch._drawcallUniform != DEFAULT_BATCH._drawcallUniform) {
        batchNode[keys::drawcallUniform] = batch._drawcallUniform;
    }
    if (batch._drawcallUniformReset != DEFAULT_BATCH._drawcallUniformReset) {
        batchNode[keys::drawcallUniformReset] = batch._drawcallUniformReset;
    }
    if (0 != batch._textures.size()) {
        batchNode[keys::textures] = serializePointerCache(batch._textures, textureMap);
    }
    if (0 != batch._textureTables.size()) {
        batchNode[keys::textureTables] = serializePointerCache(batch._textureTables, textureTableMap);
    }
    if (0 != batch._buffers.size()) {
        batchNode[keys::buffers] = serializePointerCache(batch._buffers, bufferMap);
    }
    if (0 != batch._pipelines.size()) {
        batchNode[keys::pipelines] = serializePointerCache(batch._pipelines, pipelineMap);
    }
    if (0 != batch._streamFormats.size()) {
        batchNode[keys::formats] = serializePointerCache(batch._streamFormats, formatMap);
    }
    if (0 != batch._framebuffers.size()) {
        batchNode[keys::framebuffers] = serializePointerCache(batch._framebuffers, framebufferMap);
    }
    if (0 != batch._swapChains.size()) {
        batchNode[keys::swapchains] = serializePointerCache(batch._swapChains, swapchainMap);
    }
    if (0 != batch._queries.size()) {
        batchNode[keys::queries] = serializePointerCache(batch._queries, queryMap);
    }
    if (!batch._drawCallInfos.empty()) {
        batchNode[keys::drawCallInfos] = writeUintVector(batch._drawCallInfos);
    }
    if (!batch._data.empty()) {
        batchNode[keys::data] = toBase64(batch._data);
    }

    {
        auto& node = batchNode[keys::commands] = json::array();
        size_t commandCount = batch._commands.size();
        for (size_t i = 0; i < commandCount; ++i) {
            node.push_back(writeCommand(i, batch));
        }
    }

    if (0 != batch._transforms.size()) {
        batchNode[keys::transforms] =
            serializeDataCache<Transform, json>(batch._transforms, [](const Transform& t) { return writeTransform(t); });
    }
    if (0 != batch._profileRanges.size()) {
        batchNode[keys::profileRanges] = serializeDataCache<std::string>(batch._profileRanges);
    }
    if (0 != batch._names.size()) {
        batchNode[keys::names] = serializeDataCache<std::string>(batch._names);
    }
    if (0 != batch._objects.size()) {
        auto transform = [](const Batch::TransformObject& object) -> json { return writeMat4(object._model); };
        batchNode[keys::objects] = writeVector<Batch::TransformObject, json>(batch._objects, transform);
    }

    if (!batch._namedData.empty()) {
        auto& namedDataNode = batchNode[keys::namedData] = json::object();
        for (const auto& entry : batch._namedData) {
            namedDataNode[entry.first] = writeNamedBatchData(entry.second);
        }
    }

    //    LambdaCache _lambdas;

    return batchNode;
}

json Serializer::writeSampler(const Sampler& sampler) {
    json result = json::object();
    if (sampler.getBorderColor() != DEFAULT_SAMPLER.getBorderColor()) {
        result[keys::borderColor] = writeVec4(sampler.getBorderColor());
    }
    if (sampler.getMaxAnisotropy() != DEFAULT_SAMPLER.getMaxAnisotropy()) {
        result[keys::maxAnisotropy] = sampler.getMaxAnisotropy();
    }
    if (sampler.getWrapModeU() != DEFAULT_SAMPLER.getWrapModeU()) {
        result[keys::wrapModeU] = sampler.getWrapModeU();
    }
    if (sampler.getWrapModeV() != DEFAULT_SAMPLER.getWrapModeV()) {
        result[keys::wrapModeV] = sampler.getWrapModeV();
    }
    if (sampler.getWrapModeW() != DEFAULT_SAMPLER.getWrapModeW()) {
        result[keys::wrapModeW] = sampler.getWrapModeW();
    }
    if (sampler.getFilter() != DEFAULT_SAMPLER.getFilter()) {
        result[keys::filter] = sampler.getFilter();
    }
    if (sampler.getComparisonFunction() != DEFAULT_SAMPLER.getComparisonFunction()) {
        result[keys::comparisonFunction] = sampler.getComparisonFunction();
    }
    if (sampler.getMinMip() != DEFAULT_SAMPLER.getMinMip()) {
        result[keys::minMip] = sampler.getMinMip();
    }
    if (sampler.getMaxMip() != DEFAULT_SAMPLER.getMaxMip()) {
        result[keys::maxMip] = sampler.getMaxMip();
    }
    if (sampler.getMipOffset() != DEFAULT_SAMPLER.getMipOffset()) {
        result[keys::mipOffset] = sampler.getMipOffset();
    }
    return result;
}

json Serializer::writeBuffer(const BufferPointer& bufferPointer) {
    if (!bufferPointer) {
        return json();
    }

    return json(bufferPointer->getSize());
}

json Serializer::writeIrradiance(const SHPointer& irradiancePointer) {
    if (!irradiancePointer) {
        return json();
    }

    json result = json::object();
    const auto& irradiance = *irradiancePointer;
    result[keys::L00] = writeVec3(irradiance.L00);
    result[keys::L1m1] = writeVec3(irradiance.L1m1);
    result[keys::L10] = writeVec3(irradiance.L10);
    result[keys::L11] = writeVec3(irradiance.L11);
    result[keys::L2m2] = writeVec3(irradiance.L2m2);
    result[keys::L2m1] = writeVec3(irradiance.L2m1);
    result[keys::L20] = writeVec3(irradiance.L20);
    result[keys::L21] = writeVec3(irradiance.L21);
    result[keys::L22] = writeVec3(irradiance.L22);
    return result;
}

json Serializer::writeTexture(const TexturePointer& texturePointer) {
    if (!texturePointer) {
        return json();
    }

    const auto& texture = *texturePointer;
    json result = json::object();
    if (!texture.source().empty()) {
        result[keys::source] = texture.source();
    }
    const auto usageType = texture.getUsageType();
    result[keys::usageType] = usageType;
    result[keys::type] = texture.getType();
    result[keys::width] = texture._width;
    result[keys::height] = texture._height;
    result[keys::depth] = texture._depth;
    result[keys::mips] = texture._maxMipLevel + 1;
    result[keys::samples] = texture._numSamples;
    result[keys::layers] = texture._numSlices;
    result[keys::texelFormat] = texture.getTexelFormat().getRaw();
    if (texture.isIrradianceValid()) {
        result["irradiance"] = writeIrradiance(texture._irradiance);
    }
    if (texture._sampler != DEFAULT_SAMPLER) {
        result[keys::sampler] = writeSampler(texture._sampler);
    }
    if (usageType == TextureUsageType::RENDERBUFFER) {
        // TODO figure out if the buffer contents need to be preserved (if it's used as an input before it's ever written to)
        // This might be the case for things like the TAA output attachments from the previous frame
    } else if (usageType == TextureUsageType::EXTERNAL) {
        // TODO serialize the current GL contents (if any) to the JSON
    } else {
        const auto* storage = texture._storage.get();
        const auto* ktxStorage = dynamic_cast<const Texture::KtxStorage*>(storage);
        if (ktxStorage) {
            result[keys::chunk] = 2 + ktxBuilders.size();
            auto filename = ktxStorage->_filename;
            ktxBuilders.push_back([=] {
                return std::make_shared<storage::FileStorage>(filename.c_str());
            });
        } else if (textureCapturer && captureTextures.count(texturePointer) != 0) {
            result[keys::chunk] = 2 + ktxBuilders.size();
            auto storage = textureCapturer(texturePointer);
            ktxBuilders.push_back([=] {
                return storage;
            });
        }
    }
    return result;
}

json Serializer::writeTextureView(const TextureView& textureView) {
    static const auto DEFAULT_TEXTURE_VIEW = TextureView();
    json result = json::object();
    if (textureView._texture) {
        result[keys::texture] = getGlobalIndex(textureView._texture, textureMap);
    }
    if (textureView._subresource != DEFAULT_TEXTURE_VIEW._subresource) {
        result[keys::subresource] = textureView._subresource;
    }
    if (textureView._element != DEFAULT_TEXTURE_VIEW._element) {
        result[keys::element] = textureView._element.getRaw();
    }
    return result;
}

json Serializer::writeFramebuffer(const FramebufferPointer& framebufferPointer) {
    if (!framebufferPointer) {
        return json();
    }

    auto result = json::object();
    const auto& framebuffer = *framebufferPointer;
    if (!framebuffer._name.empty()) {
        result[keys::name] = framebuffer._name;
    }
    if (framebuffer._bufferMask != 0) {
        result[keys::bufferMask] = framebuffer._bufferMask;
    }
    if (framebuffer._height != 0) {
        result[keys::height] = framebuffer._height;
    }
    if (framebuffer._width != 0) {
        result[keys::width] = framebuffer._width;
    }
    if (framebuffer._numSamples != 0 && framebuffer._numSamples != 1) {
        result[keys::sampleCount] = framebuffer._numSamples;
    }

    if (framebuffer._depthStencilBuffer.isValid()) {
        result[keys::depthStencilAttachment] = writeTextureView(framebuffer._depthStencilBuffer);
    }

    if (!framebuffer._renderBuffers.empty()) {
        size_t rendereBufferCount = 0;
        for (size_t i = 0; i < framebuffer._renderBuffers.size(); ++i) {
            if (framebuffer._renderBuffers[i].isValid()) {
                rendereBufferCount = i + 1;
            }
        }
        if (rendereBufferCount > 0) {
            auto& node = result[keys::colorAttachments] = json::array();
            for (size_t i = 0; i < rendereBufferCount; ++i) {
                node.push_back(writeTextureView(framebuffer._renderBuffers[i]));
            }
        }
    }

    //SwapchainPointer _swapchain;
    return result;
}

json Serializer::writeTextureTable(const TextureTablePointer& textureTablePointer) {
    auto tableNode = json::array();
    const auto& textureTable = *textureTablePointer;
    for (const auto& texture : textureTable.getTextures()) {
        tableNode.push_back(getGlobalIndex(texture, textureMap));
    }
    return tableNode;
}

json Serializer::writeFormat(const Stream::FormatPointer& formatPointer) {
    if (!formatPointer) {
        return json();
    }

    const auto& format = *formatPointer;
    json result = json::object();
    auto& attributesNode = result[keys::attributes] = json::array();
    static const auto DEFAULT_ATTRIBUTE = Stream::Attribute();
    for (const auto& entry : format._attributes) {
        const auto& attribute = entry.second;
        auto attributeNode = json::object();
        attributeNode[keys::slot] = attribute._slot;
        attributeNode[keys::channel] = attribute._channel;
        if (DEFAULT_ATTRIBUTE._element.getRaw() != attribute._element.getRaw()) {
            attributeNode[keys::element] = attribute._element.getRaw();
        }
        if (DEFAULT_ATTRIBUTE._frequency != attribute._frequency) {
            attributeNode[keys::frequency] = attribute._frequency;
        }
        if (DEFAULT_ATTRIBUTE._offset != attribute._offset) {
            attributeNode[keys::offset] = attribute._offset;
        }
        attributesNode.push_back(attributeNode);
    }
    return result;
}

#define SET_IF_NOT_DEFAULT(FIELD) \
    if (value.FIELD != DEFAULT.FIELD) { \
        result[keys::FIELD] = value.FIELD; \
    }

#define SET_IF_NOT_DEFAULT_(FIELD) \
    if (value._##FIELD != DEFAULT._##FIELD) { \
        result[keys::FIELD] = value._##FIELD; \
    }

#define SET_IF_NOT_DEFAULT_TRANSFORM(FIELD, TRANSFORM) \
    if (value.FIELD != DEFAULT.FIELD) { \
        result[keys::FIELD] = TRANSFORM(value.FIELD); \
    }

static json writeBlendFunction(const State::BlendFunction& value) {
    static const State::BlendFunction DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT(enabled);
    SET_IF_NOT_DEFAULT(sourceColor);
    SET_IF_NOT_DEFAULT(sourceAlpha);
    SET_IF_NOT_DEFAULT(destColor);
    SET_IF_NOT_DEFAULT(destAlpha);
    SET_IF_NOT_DEFAULT(destColor);
    SET_IF_NOT_DEFAULT(opAlpha);
    SET_IF_NOT_DEFAULT(opColor);
    return result;
}

static json writeStateFlags(const State::Flags& value) {
    static const State::Flags DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT(frontFaceClockwise);
    SET_IF_NOT_DEFAULT(depthClampEnable);
    SET_IF_NOT_DEFAULT(scissorEnable);
    SET_IF_NOT_DEFAULT(multisampleEnable);
    SET_IF_NOT_DEFAULT(antialisedLineEnable);
    SET_IF_NOT_DEFAULT(alphaToCoverageEnable);
    return result;
}

static json writeDepthTest(const State::DepthTest& value) {
    static const State::DepthTest DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT(writeMask);
    SET_IF_NOT_DEFAULT(enabled);
    SET_IF_NOT_DEFAULT(function);
    return result;
}


static json writeStereoState(const StereoState& value) {
    static const StereoState DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT_(enable);
    SET_IF_NOT_DEFAULT_(contextDisable);
    SET_IF_NOT_DEFAULT_(skybox);
    if ((value._eyeProjections[0] != DEFAULT._eyeProjections[0]) || (value._eyeProjections[1] != DEFAULT._eyeProjections[1])) {
        json projections = json::array();
        projections.push_back(Serializer::writeMat4(value._eyeProjections[0]));
        projections.push_back(Serializer::writeMat4(value._eyeProjections[1]));
        result[keys::eyeProjections] = projections;
    }
    if ((value._eyeViews[0] != DEFAULT._eyeViews[0]) || (value._eyeViews[1] != DEFAULT._eyeViews[1])) {
        json views = json::array();
        views.push_back(Serializer::writeMat4(value._eyeViews[0]));
        views.push_back(Serializer::writeMat4(value._eyeViews[1]));
        result[keys::eyeViews] = views;
    }
    return result;
}

static json writeStencilTest(const State::StencilTest& value) {
    static const State::StencilTest DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT(function);
    SET_IF_NOT_DEFAULT(failOp);
    SET_IF_NOT_DEFAULT(depthFailOp);
    SET_IF_NOT_DEFAULT(passOp);
    SET_IF_NOT_DEFAULT(reference);
    SET_IF_NOT_DEFAULT(readMask);
    return result;
}

static json writeStencilActivation(const State::StencilActivation& value) {
    static const State::StencilActivation DEFAULT;
    json result = json::object();
    SET_IF_NOT_DEFAULT(frontWriteMask);
    SET_IF_NOT_DEFAULT(backWriteMask);
    SET_IF_NOT_DEFAULT(enabled);
    return result;
}

json writeState(const StatePointer& statePointer) {
    if (!statePointer) {
        return json();
    }
    const auto& state = *statePointer;
    const auto& value = state.getValues();
    const auto& DEFAULT = State::DEFAULT;
    auto result = json::object();
    SET_IF_NOT_DEFAULT(colorWriteMask);
    SET_IF_NOT_DEFAULT(cullMode);
    SET_IF_NOT_DEFAULT(depthBias);
    SET_IF_NOT_DEFAULT(depthBiasSlopeScale);
    SET_IF_NOT_DEFAULT(fillMode);
    SET_IF_NOT_DEFAULT(sampleMask);
    SET_IF_NOT_DEFAULT_TRANSFORM(blendFunction, writeBlendFunction);
    SET_IF_NOT_DEFAULT_TRANSFORM(flags, writeStateFlags);
    SET_IF_NOT_DEFAULT_TRANSFORM(depthTest, writeDepthTest);
    SET_IF_NOT_DEFAULT_TRANSFORM(stencilActivation, writeStencilActivation);
    SET_IF_NOT_DEFAULT_TRANSFORM(stencilTestFront, writeStencilTest);
    SET_IF_NOT_DEFAULT_TRANSFORM(stencilTestBack, writeStencilTest);
    return result;
}

json Serializer::writePipeline(const PipelinePointer& pipelinePointer) {
    if (!pipelinePointer) {
        return json();
    }
    const auto& pipeline = *pipelinePointer;
    auto result = json::object();
    result[keys::state] = writeState(pipeline.getState());
    result[keys::program] = getGlobalIndex(pipeline.getProgram(), programMap);
    return result;
}

json Serializer::writeProgram(const ShaderPointer& programPointer) {
    if (!programPointer) {
        return json();
    }
    const auto& program = *programPointer;
    auto result = json::array();
    for (const auto& shader : program._shaders) {
        result.push_back(getGlobalIndex(shader, shaderMap));
    }
    return result;
}

json Serializer::writeShader(const ShaderPointer& shaderPointer) {
    if (!shaderPointer) {
        return json();
    }
    auto result = json::object();
    const auto& shader = *shaderPointer;
    result[keys::id] = shader._source.id;
    result[keys::name] = shader._source.name;
    result[keys::type] = shader._type;
    return result;
}

json Serializer::writeSwapchain(const SwapChainPointer& swapchainPointer) {
    auto framebufferSwapchainPointer = std::static_pointer_cast<FramebufferSwapChain>(swapchainPointer);
    if (!framebufferSwapchainPointer) {
        return json();
    }
    const FramebufferSwapChain& swapchain = *framebufferSwapchainPointer;
    auto result = json::object();
    result[keys::size] = swapchain.getSize();
    auto& framebuffersNode = result[keys::framebuffers] = json::array();
    for (uint32_t i = 0; i < swapchain.getSize(); ++i) {
        uint32_t index = getGlobalIndex(swapchain.get(i), framebufferMap);
        framebuffersNode.push_back(index);
    }
    return result;
}

json Serializer::writeQuery(const QueryPointer& queryPointer) {
    if (!queryPointer) {
        return json();
    }

    const auto& query = *queryPointer;
    auto result = json::object();
    result[keys::name] = query._name;
    return result;
}

void Serializer::findCapturableTextures(const Frame& frame) {
    std::unordered_set<TexturePointer> writtenRenderbuffers;
    auto maybeCaptureTexture = [&](const TexturePointer& texture) {
        // Not a valid texture
        if (!texture) {
            return;
        }

        // Not a renderbuffer
        if (texture->getUsageType() != TextureUsageType::RENDERBUFFER) {
            return;
        }

        // Already used in a target framebuffer
        if (writtenRenderbuffers.count(texture) > 0) {
            return;
        }

        captureTextures.insert(texture);
    };

    for (const auto& batchPtr : frame.batches) {
        const auto& batch = *batchPtr;
        batch.forEachCommand([&](Batch::Command command, const Batch::Param* params) {
            switch (command) {
                case Batch::COMMAND_setResourceTexture: {
                    const auto& texture = batch._textures.get(params[0]._uint);
                    maybeCaptureTexture(texture);
                } break;

                case Batch::COMMAND_setResourceTextureTable: {
                    const auto& textureTablePointer = batch._textureTables.get(params[0]._uint);
                    if (textureTablePointer) {
                        for (const auto& texture : textureTablePointer->getTextures()) {
                            maybeCaptureTexture(texture);
                        }
                    }
                } break;

                case Batch::COMMAND_setFramebuffer: {
                    const auto& framebufferPointer = batch._framebuffers.get(params[0]._uint);
                    if (framebufferPointer) {
                        const auto& framebuffer = *framebufferPointer;
                        for (const auto& colorAttachment : framebuffer._renderBuffers) {
                            if (colorAttachment._texture) {
                                writtenRenderbuffers.insert(colorAttachment._texture);
                            }
                        }
                        if (framebuffer._depthStencilBuffer._texture) {
                            writtenRenderbuffers.insert(framebuffer._depthStencilBuffer._texture);
                        }
                    }
                }

                case Batch::COMMAND_setResourceFramebufferSwapChainTexture:
                default:
                    break;
            }
        });  // for each command
    }        // for each batch

    for (const auto& entry : textureMap) {
        const auto& texturePointer = entry.first;
        if (!texturePointer) {
            continue;
        }
        const auto& texture = *texturePointer;
        auto usageType = texture.getUsageType();
        if (usageType == TextureUsageType::RESOURCE || usageType == TextureUsageType::STRICT_RESOURCE) {
            const auto* storage = texture._storage.get();
            const auto* ktxStorage = dynamic_cast<const Texture::KtxStorage*>(storage);
            if (!ktxStorage) {
                captureTextures.insert(texturePointer);
            }
        }
    }
}

void Serializer::writeFrame(const Frame& frame) {
    json frameNode = json::object();

    frameNode[keys::batches] = json::array();
    for (const auto& batchPointer : frame.batches) {
        frameNode[keys::batches].push_back(writeBatch(*batchPointer));
    }

    frameNode[keys::stereo] = writeStereoState(frame.stereoState);
    findCapturableTextures(frame);
    frameNode[keys::frameIndex] = frame.frameIndex;
    frameNode[keys::view] = writeMat4(frame.view);
    frameNode[keys::pose] = writeMat4(frame.pose);
    frameNode[keys::framebuffer] = getGlobalIndex(frame.framebuffer, framebufferMap);
    using namespace std::placeholders;
    serializeMap(frameNode, keys::swapchains, swapchainMap, std::bind(&Serializer::writeSwapchain, this, _1));
    serializeMap(frameNode, keys::framebuffers, framebufferMap, [this](const auto& t) { return writeFramebuffer(t); });
    serializeMap(frameNode, keys::textureTables, textureTableMap, [this](const auto& t) { return writeTextureTable(t); });
    serializeMap(frameNode, keys::pipelines, pipelineMap, [this](const auto& t) { return writePipeline(t); });
    serializeMap(frameNode, keys::programs, programMap, [this](const auto& t) { return writeProgram(t); });
    serializeMap(frameNode, keys::shaders, shaderMap, writeShader);
    serializeMap(frameNode, keys::queries, queryMap, writeQuery);
    serializeMap(frameNode, keys::formats, formatMap, writeFormat);

    // Serialize textures and buffers last, since the maps they use can be populated by some of the above code
    // Serialize textures
    serializeMap(frameNode, keys::textures, textureMap, std::bind(&Serializer::writeTexture, this, _1));
    // Serialize buffers
    serializeMap(frameNode, keys::buffers, bufferMap, writeBuffer);

    writeBinaryBlob();

    hfb::writeFrame(filename, frameNode.dump(), binaryBuffer, ktxBuilders);
}

void Serializer::writeBinaryBlob() {
    const auto buffers = mapToVector(bufferMap);
    auto accumulator = [](size_t total, const BufferPointer& buffer) { return total + (buffer ? buffer->getSize() : 0); };
    size_t totalSize = std::accumulate(buffers.begin(), buffers.end(), (size_t)0, accumulator);
    binaryBuffer.resize(totalSize);
    auto mapped = binaryBuffer.data();
    size_t offset = 0;

    for (const auto& bufferPointer : buffers) {
        if (!bufferPointer) {
            continue;
        }
        const auto& buffer = *bufferPointer;
        const auto bufferSize = buffer.getSize();
        const auto& bufferData = buffer._renderSysmem.readData();
        memcpy(mapped + offset, bufferData, bufferSize);
        offset += bufferSize;
    }
}
