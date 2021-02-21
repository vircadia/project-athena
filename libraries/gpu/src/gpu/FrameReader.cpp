//
//  Created by Bradley Austin Davis on 2018/10/14
//  Copyright 2013-2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "FrameIO.h"

#include <nlohmann/json.hpp>
#include <unordered_map>

#include <shared/FileUtils.h>
#include <ktx/KTX.h>
#include "Frame.h"
#include "Batch.h"
#include "TextureTable.h"

#include "FrameIOKeys.h"

namespace gpu {
using json = nlohmann::json;
using StoragePointer = storage::StoragePointer;
using FileStorage = storage::FileStorage;

class Deserializer {
public:
    static std::string getBaseDir(const std::string& filename) {
        std::string result;
        if (0 == filename.find("assets:")) {
            auto lastSlash = filename.rfind('/');
            result = filename.substr(0, lastSlash + 1);
        } else {
            result = FileUtils::getParentPath(filename.c_str()).toStdString();
            if (*result.rbegin() != '/') {
                result += '/';
            }
        }
        return result;
    }

    Deserializer(const std::string& filename_, uint32_t externalTexture = 0) :
        filename(filename_), basedir(getBaseDir(filename_)), mappedFile(std::make_shared<FileStorage>(filename.c_str())),
        externalTexture(externalTexture) {
        descriptor = std::make_shared<hfb::Descriptor>(mappedFile);
    }

    const std::string filename;
    const std::string basedir;
    const StoragePointer mappedFile;
    const uint32_t externalTexture;
    hfb::Descriptor::Pointer descriptor;
    std::vector<ShaderPointer> shaders;
    std::vector<ShaderPointer> programs;
    std::vector<TexturePointer> textures;
    std::vector<TextureTablePointer> textureTables;
    std::vector<BufferPointer> buffers;
    std::unordered_map<BufferPointer, size_t> bufferOffsets;
    std::vector<Stream::FormatPointer> formats;
    std::vector<PipelinePointer> pipelines;
    std::vector<FramebufferPointer> framebuffers;
    std::vector<SwapChainPointer> swapchains;
    std::vector<QueryPointer> queries;
    json frameNode;
    FramePointer readFrame();

    FramePointer deserializeFrame();

    std::string getStringChunk(size_t chunkIndex) {
        auto storage = descriptor->getChunk((uint32_t)chunkIndex);
        return std::string{ (const char*)storage->data(), storage->size() };
    }

    void readBuffers(const json& node);

    template <typename T>
    static std::vector<T> readArray(const json& node, const std::string& name, std::function<T(const json& node)> parser) {
        std::vector<T> result;
        if (node.count(name)) {
            const auto& sourceArrayNode = node[name];
            result.reserve(sourceArrayNode.size());
            for (const auto& sourceNode : sourceArrayNode) {
                if (sourceNode.is_null()) {
                    result.push_back(nullptr);
                    continue;
                }
                result.push_back(parser(sourceNode));
            }
        }
        return result;
    }

    template <typename T>
    static std::vector<T> readNumericVector(const json& node) {
        auto count = node.size();
        std::vector<T> result;
        result.resize(count);
        for (size_t i = 0; i < count; ++i) {
            result[i] = node[i];
        }
        return result;
    }

    template <size_t N>
    static void readFloatArray(const json& node, float* out) {
        for (size_t i = 0; i < N; ++i) {
            out[i] = node[i].operator float();
        }
    }

    template <typename T>
    static bool readOptionalTransformed(T& dest, const json& node, const std::string& name, std::function<T(const json&)> f) {
        if (node.count(name)) {
            dest = f(node[name]);
            return true;
        }
        return false;
    }

    template <typename T>
    static bool readOptionalVectorTransformed(std::vector<T>& dest,
                                              const json& node,
                                              const std::string& name,
                                              std::function<T(const json&)> f) {
        if (node.count(name)) {
            const auto& arrayNode = node[name];
            const auto count = arrayNode.size();
            dest.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                dest.emplace_back(f(arrayNode[i]));
            }
            return true;
        }
        return false;
    }

    template <typename T>
    static bool readOptionalVector(std::vector<T>& dest, const json& node, const std::string& name) {
        return readOptionalVectorTransformed(dest, node, name, [](const json& node) { return node.get<T>(); });
    }

    template <typename T>
    static T defaultNodeTransform(const json& node) {
        return node.get<T>();
    }

    template <typename T, typename TT = T>
    static bool readBatchCacheTransformed(
        typename Batch::Cache<T>::Vector& dest,
        const json& node,
        const std::string& name,
        std::function<TT(const json&)> f = [](const json& node) -> TT { return node.get<TT>(); }) {
        if (node.count(name)) {
            const auto& arrayNode = node[name];
            for (const auto& entry : arrayNode) {
                dest.cache(f(entry));
            }
            return true;
        }
        return false;
    }

    template <typename T>
    static bool readPointerCache(typename Batch::Cache<T>::Vector& dest,
                                 const json& node,
                                 const std::string& name,
                                 std::vector<T>& global) {
        auto transform = [&](const json& node) -> const T& { return global[node.get<uint32_t>()]; };
        return readBatchCacheTransformed<T, const T&>(dest, node, name, transform);
    }

    template <typename T>
    static bool readOptional(T& dest, const json& node, const std::string& name) {
        return readOptionalTransformed<T>(dest, node, name, [](const json& child) {
            T result = child;
            return result;
        });
    }

    SwapChainPointer readSwapchain(const json& node);
    ShaderPointer readProgram(const json& node);
    PipelinePointer readPipeline(const json& node);
    TextureTablePointer readTextureTable(const json& node);
    TextureView readTextureView(const json& node);
    FramebufferPointer readFramebuffer(const json& node);
    BatchPointer readBatch(const json& node);
    Batch::NamedBatchData readNamedBatchData(const json& node);

    //static StatePointer readState(const json& node);
    static QueryPointer readQuery(const json& node);
    TexturePointer readTexture(const json& node, uint32_t externalTexture);
    static ShaderPointer readShader(const json& node);
    static Stream::FormatPointer readFormat(const json& node);
    static Element readElement(const json& node);
    static Sampler readSampler(const json& node);
    static glm::mat4 readMat4(const json& node) {
        glm::mat4 m;
        if (!node.is_null()) {
            readFloatArray<16>(node, &m[0][0]);
        }
        return m;
    }
    static glm::vec4 readVec4(const json& node) {
        glm::vec4 v;
        if (!node.is_null()) {
            readFloatArray<4>(node, &v[0]);
        }
        return v;
    }
    static glm::vec3 readVec3(const json& node) {
        glm::vec3 v;
        if (!node.is_null()) {
            readFloatArray<3>(node, &v[0]);
        }
        return v;
    }
    static glm::vec2 readVec2(const json& node) {
        glm::vec2 v;
        if (!node.is_null()) {
            readFloatArray<2>(node, &v[0]);
        }
        return v;
    }
    static Transform readTransform(const json& node) { return Transform{ readMat4(node) }; }
    static std::vector<uint8_t> fromBase64(const json& node);
    static void readCommand(const json& node, Batch& batch);
};

FramePointer readFrame(const std::string& filename, uint32_t externalTexture) {
    return Deserializer(filename, externalTexture).readFrame();
}

}  // namespace gpu

using namespace gpu;

void Deserializer::readBuffers(const json& buffersNode) {
    const auto& binaryChunk = descriptor->chunks[1];
    const auto* mapped = mappedFile->data() + binaryChunk.offset;
    const auto mappedSize = binaryChunk.length;
    size_t bufferCount = buffersNode.size();
    buffers.reserve(buffersNode.size());
    size_t offset = 0;
    for (size_t i = 0; i < bufferCount; ++i) {
        const auto& bufferNode = buffersNode[i];
        if (bufferNode.is_null()) {
            buffers.push_back(nullptr);
            continue;
        }

        size_t size = bufferNode;
        if (offset + size > mappedSize) {
            throw std::runtime_error("read buffer error");
        }
        buffers.push_back(std::make_shared<Buffer>(size, mapped + offset));
        bufferOffsets[buffers.back()] = offset;
        offset += size;
    }
}

Element Deserializer::readElement(const json& node) {
    Element result;
    if (!node.is_null()) {
        *((uint16*)&result) = node;
    }
    return result;
}

Sampler Deserializer::readSampler(const json& node) {
    Sampler result;
    if (!node.is_null()) {
        if (node.count(keys::borderColor)) {
            result._desc._borderColor = readVec4(node[keys::borderColor]);
        }
        if (node.count(keys::maxAnisotropy)) {
            result._desc._maxAnisotropy = node[keys::maxAnisotropy];
        }
        if (node.count(keys::wrapModeU)) {
            result._desc._wrapModeU = node[keys::wrapModeU];
        }
        if (node.count(keys::wrapModeV)) {
            result._desc._wrapModeV = node[keys::wrapModeV];
        }
        if (node.count(keys::wrapModeW)) {
            result._desc._wrapModeW = node[keys::wrapModeW];
        }
        if (node.count(keys::filter)) {
            result._desc._filter = node[keys::filter];
        }
        if (node.count(keys::comparisonFunction)) {
            result._desc._comparisonFunc = node[keys::comparisonFunction];
        }
        if (node.count(keys::minMip)) {
            result._desc._minMip = node[keys::minMip];
        }
        if (node.count(keys::maxMip)) {
            result._desc._maxMip = node[keys::maxMip];
        }
        if (node.count(keys::mipOffset)) {
            result._desc._mipOffset = node[keys::mipOffset];
        }
    }
    return result;
}

constexpr uint32_t INVALID_CHUNK_INDEX{ (uint32_t)-1 };

TexturePointer Deserializer::readTexture(const json& node, uint32_t external) {
    if (node.is_null()) {
        return nullptr;
    }

    std::string source;
    readOptional(source, node, keys::source);

    uint32_t chunkIndex = INVALID_CHUNK_INDEX;
    readOptional(chunkIndex, node, keys::chunk);

    std::string ktxFile;
    readOptional(ktxFile, node, keys::ktxFile);
    if (!ktxFile.empty()) {
        if (!FileUtils::exists(ktxFile.c_str())) {
            qDebug() << "Warning" << ktxFile.c_str() << " not found, ignoring";
            ktxFile = {};
        }
    }
    Element ktxTexelFormat, ktxMipFormat;
    if (!ktxFile.empty()) {
        // If we get a texture that starts with ":" we need to re-route it to the resources directory
        if (ktxFile.at(0) == ':') {
            QString frameReaderPath = __FILE__;
            frameReaderPath.replace("\\", "/");
            frameReaderPath.replace("libraries/gpu/src/gpu/framereader.cpp", "interface/resources", Qt::CaseInsensitive);
            ktxFile.replace(0, 1, frameReaderPath.toStdString());
        }
        if (FileUtils::isRelative(ktxFile.c_str())) {
            ktxFile = basedir + "/" + ktxFile;
        }
        ktx::StoragePointer ktxStorage{ new storage::FileStorage(ktxFile.c_str()) };
        auto ktxObject = ktx::KTX::create(ktxStorage);
        Texture::evalTextureFormat(ktxObject->getHeader(), ktxTexelFormat, ktxMipFormat);
    }

    TextureUsageType usageType = node[keys::usageType];
    Texture::Type type = node[keys::type];
    glm::u16vec4 dims;
    dims.x = node[keys::width];
    dims.y = node[keys::height];
    dims.z = node[keys::depth];
    dims.w = node[keys::layers];
    uint16 mips = node[keys::mips];
    uint16 samples = node[keys::samples];
    Element texelFormat = readElement(node[keys::texelFormat]);
    if (!ktxFile.empty() && (ktxMipFormat.isCompressed() != texelFormat.isCompressed())) {
        texelFormat = ktxMipFormat;
    }
    Sampler sampler;
    readOptionalTransformed<Sampler>(sampler, node, keys::sampler, [](const json& node) { return readSampler(node); });
    TexturePointer result;
    if (usageType == TextureUsageType::EXTERNAL) {
        result = Texture::createExternal([](uint32_t, void*) {});
        result->setExternalTexture(external, nullptr);
    } else {
        result = Texture::create(usageType, type, texelFormat, dims.x, dims.y, dims.z, samples, dims.w, mips, sampler);
    }

    auto& texture = *result;
    readOptional(texture._source, node, keys::source);


    if (chunkIndex != INVALID_CHUNK_INDEX) {
        auto ktxChunk = descriptor->getChunk(chunkIndex);
        texture.setKtxBacking(ktxChunk);
    } else if (!ktxFile.empty()) {
        texture.setSource(ktxFile);
        texture.setKtxBacking(ktxFile);
    } 
    return result;
}

SwapChainPointer Deserializer::readSwapchain(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    uint8_t swapChainSize = node[keys::size];
    std::vector<FramebufferPointer> swapChainFramebuffers;
    const auto& framebuffersNode = node[keys::framebuffers];
    swapChainFramebuffers.resize(swapChainSize);
    for (uint8_t i = 0; i < swapChainSize; ++i) {
        auto index = framebuffersNode[i].get<uint32_t>();
        swapChainFramebuffers[i] = framebuffers[index];
    }
    return std::make_shared<FramebufferSwapChain>(swapChainFramebuffers);
}

ShaderPointer Deserializer::readShader(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    static std::map<std::string, uint32_t> shadersIdsByName;
    if (shadersIdsByName.empty()) {
        for (const auto id : shader::allShaders()) {
            const auto& shaderSource = shader::Source::get(id);
            shadersIdsByName[shaderSource.name] = id;
        }
    }

    // FIXME support procedural shaders
    Shader::Type type = node[keys::type];
    std::string name = node[keys::name];
    // Using the serialized ID is bad, because it's generated at
    // cmake time, and can change across platforms or when
    // shaders are added or removed
    // uint32_t id = node[keys::id];

    uint32_t id = shadersIdsByName[name];
    ShaderPointer result;
    switch (type) {
        //case Shader::Type::GEOMETRY:
        //    result = Shader::createGeometry(id);
        //    break;
        case Shader::Type::VERTEX:
            result = Shader::createVertex(id);
            break;
        case Shader::Type::FRAGMENT:
            result = Shader::createPixel(id);
            break;
        default:
            throw std::runtime_error("not implemented");
    }
    if (result->getSource().name != name) {
        throw std::runtime_error("Bad name match");
    }
    return result;
}

ShaderPointer Deserializer::readProgram(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    std::vector<ShaderPointer> programShaders;
    programShaders.reserve(node.size());
    for (const auto& shaderRef : node) {
        uint32_t shaderIndex = shaderRef;
        programShaders.push_back(this->shaders[shaderIndex]);
    }

    // FIXME account for geometry and compute shaders?
    return Shader::createProgram(programShaders[0], programShaders[1]);
}

static State::Flags readStateFlags(const json& node) {
    State::Flags result;
    // Hacky implementation because you can't pass boolean bitfields as references
    bool value;
    if (Deserializer::readOptional(value, node, keys::alphaToCoverageEnable)) {
        result.alphaToCoverageEnable = value;
    }
    if (Deserializer::readOptional(value, node, keys::frontFaceClockwise)) {
        result.frontFaceClockwise = value;
    }
    if (Deserializer::readOptional(value, node, keys::depthClampEnable)) {
        result.depthClampEnable = value;
    }
    if (Deserializer::readOptional(value, node, keys::scissorEnable)) {
        result.scissorEnable = value;
    }
    if (Deserializer::readOptional(value, node, keys::multisampleEnable)) {
        result.multisampleEnable = value;
    }
    if (Deserializer::readOptional(value, node, keys::antialisedLineEnable)) {
        result.antialisedLineEnable = value;
    }
    if (Deserializer::readOptional(value, node, keys::alphaToCoverageEnable)) {
        result.alphaToCoverageEnable = value;
    }
    return result;
}

static State::BlendFunction readBlendFunction(const json& node) {
    State::BlendFunction result;
    uint16 enabled;
    State::BlendArg blendArg;
    State::BlendOp blendOp;
    if (Deserializer::readOptional(enabled, node, keys::enabled)) {
        result.enabled = enabled;
    }
    if (Deserializer::readOptional(blendArg, node, keys::sourceColor)) {
        result.sourceColor = blendArg;
    }
    if (Deserializer::readOptional(blendArg, node, keys::sourceAlpha)) {
        result.sourceAlpha = blendArg;
    }
    if (Deserializer::readOptional(blendArg, node, keys::destColor)) {
        result.destColor = blendArg;
    }
    if (Deserializer::readOptional(blendArg, node, keys::destAlpha)) {
        result.destAlpha = blendArg;
    }
    if (Deserializer::readOptional(blendOp, node, keys::opAlpha)) {
        result.opAlpha = blendOp;
    }
    if (Deserializer::readOptional(blendOp, node, keys::opColor)) {
        result.opColor = blendOp;
    }
    return result;
}

static State::DepthTest readDepthTest(const json& node) {
    State::DepthTest result;
    Deserializer::readOptional(result.writeMask, node, keys::writeMask);
    Deserializer::readOptional(result.enabled, node, keys::enabled);
    Deserializer::readOptional(result.function, node, keys::function);
    return result;
}

static State::StencilTest readStencilTest(const json& node) {
    State::StencilTest result;
    State::ComparisonFunction compareOp;
    State::StencilOp stencilOp;
    if (Deserializer::readOptional(compareOp, node, keys::function)) {
        result.function = compareOp;
    }
    if (Deserializer::readOptional(stencilOp, node, keys::failOp)) {
        result.failOp = stencilOp;
    }
    if (Deserializer::readOptional(stencilOp, node, keys::depthFailOp)) {
        result.depthFailOp = stencilOp;
    }
    if (Deserializer::readOptional(stencilOp, node, keys::passOp)) {
        result.passOp = stencilOp;
    }
    if (Deserializer::readOptional(compareOp, node, keys::function)) {
        result.function = compareOp;
    }
    Deserializer::readOptional(result.reference, node, keys::reference);
    Deserializer::readOptional(result.readMask, node, keys::readMask);
    return result;
}

static State::StencilActivation readStencilActivation(const json& node) {
    auto jsonString = node.dump(2);
    State::StencilActivation result;
    bool enabled;
    if (Deserializer::readOptional(enabled, node, keys::enabled)) {
        result.enabled = enabled;
    }
    Deserializer::readOptional(result.frontWriteMask, node, keys::frontWriteMask);
    Deserializer::readOptional(result.backWriteMask, node, keys::backWriteMask);
    return result;
}

StatePointer readState(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    State::Data data;
    Deserializer::readOptionalTransformed<State::Flags>(data.flags, node, keys::flags, &readStateFlags);
    Deserializer::readOptionalTransformed<State::BlendFunction>(data.blendFunction, node, keys::blendFunction,
                                                                &readBlendFunction);
    Deserializer::readOptionalTransformed<State::DepthTest>(data.depthTest, node, keys::depthTest, &readDepthTest);
    Deserializer::readOptionalTransformed<State::StencilActivation>(data.stencilActivation, node, keys::stencilActivation,
                                                                    &readStencilActivation);
    Deserializer::readOptionalTransformed<State::StencilTest>(data.stencilTestFront, node, keys::stencilTestFront,
                                                              &readStencilTest);
    Deserializer::readOptionalTransformed<State::StencilTest>(data.stencilTestBack, node, keys::stencilTestBack,
                                                              &readStencilTest);
    Deserializer::readOptional(data.colorWriteMask, node, keys::colorWriteMask);
    Deserializer::readOptional(data.cullMode, node, keys::cullMode);
    Deserializer::readOptional(data.depthBias, node, keys::depthBias);
    Deserializer::readOptional(data.depthBiasSlopeScale, node, keys::depthBiasSlopeScale);
    Deserializer::readOptional(data.fillMode, node, keys::fillMode);
    Deserializer::readOptional(data.sampleMask, node, keys::sampleMask);
    return std::make_shared<State>(data);
}

PipelinePointer Deserializer::readPipeline(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    auto state = readState(node[keys::state]);
    uint32_t programIndex = node[keys::program];
    auto program = programs[programIndex];
    return Pipeline::create(program, state);
}

Stream::FormatPointer Deserializer::readFormat(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    auto result = std::make_shared<Stream::Format>();
    auto& format = *result;
    const auto& attributesNode = node[keys::attributes];
    for (const auto& attributeNode : attributesNode) {
        uint8_t slot = attributeNode[keys::slot];
        auto& attribute = format._attributes[slot];
        attribute._slot = slot;
        attribute._channel = attributeNode[keys::channel];
        readOptionalTransformed<Element>(attribute._element, attributeNode, keys::element,
                                         [](const json& node) { return readElement(node); });
        readOptional(attribute._frequency, attributeNode, keys::frequency);
        readOptional(attribute._offset, attributeNode, keys::offset);
    }
    format.evaluateCache();
    return result;
}

TextureTablePointer Deserializer::readTextureTable(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }
    TextureTablePointer result = std::make_shared<TextureTable>();
    auto& table = *result;
    auto count = node.size();
    for (size_t i = 0; i < count; ++i) {
        uint32_t index = node[i];
        table.setTexture(i, textures[index]);
    }
    return result;
}

TextureView Deserializer::readTextureView(const json& node) {
    TextureView result;
    auto texturePointerReader = [this](const json& node) {
        uint32_t textureIndex = node;
        return textures[textureIndex];
    };
    readOptionalTransformed<TexturePointer>(result._texture, node, keys::texture, texturePointerReader);
    readOptionalTransformed<Element>(result._element, node, keys::element, &readElement);
    readOptional(result._subresource, node, keys::subresource);
    return result;
}

FramebufferPointer Deserializer::readFramebuffer(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    FramebufferPointer result;
    {
        std::string name;
        readOptional(name, node, keys::name);
        result.reset(Framebuffer::create(name));
    }
    auto& framebuffer = *result;
    readOptional(framebuffer._bufferMask, node, keys::bufferMask);
    readOptional(framebuffer._height, node, keys::height);
    readOptional(framebuffer._width, node, keys::width);
    readOptional(framebuffer._numSamples, node, keys::sampleCount);
    auto textureViewReader = [this](const json& node) -> TextureView { return readTextureView(node); };
    readOptionalTransformed<TextureView>(framebuffer._depthStencilBuffer, node, keys::depthStencilAttachment,
                                         textureViewReader);
    if (framebuffer._depthStencilBuffer) {
        framebuffer._depthStamp++;
    }
    if (node.count(keys::colorAttachments)) {
        const auto& colorAttachmentsNode = node[keys::colorAttachments];
        size_t count = colorAttachmentsNode.size();
        for (size_t i = 0; i < count; ++i) {
            const auto& colorAttachmentNode = colorAttachmentsNode[i];
            if (colorAttachmentNode.is_null()) {
                continue;
            }
            framebuffer._renderBuffers[i] = readTextureView(colorAttachmentNode);
            framebuffer._colorStamps[i]++;
        }
    }

    return result;
}

QueryPointer Deserializer::readQuery(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }
    std::string name = node[keys::name];
    return std::make_shared<Query>([](const Query&) {}, name);
}

std::vector<uint8_t> Deserializer::fromBase64(const json& node) {
    std::vector<uint8_t> result;
    auto decoded = QByteArray::fromBase64(QByteArray{ node.get<std::string>().c_str() });
    result.resize(decoded.size());
    memcpy(result.data(), decoded.data(), decoded.size());
    return result;
}

static std::unordered_map<std::string, Batch::Command> getCommandNameMap() {
    static std::unordered_map<std::string, Batch::Command> result;
    if (result.empty()) {
        for (Batch::Command i = Batch::COMMAND_draw; i < Batch::NUM_COMMANDS; i = (Batch::Command)(i + 1)) {
            result[keys::COMMAND_NAMES[i]] = i;
        }
    }
    return result;
}

void Deserializer::readCommand(const json& commandNode, Batch& batch) {
    size_t count = commandNode.size();
    std::string commandName = commandNode[0];
    Batch::Command command = getCommandNameMap()[commandName];
    batch._commands.push_back(command);
    batch._commandOffsets.push_back(batch._params.size());
    for (size_t i = 1; i < count; ++i) {
        batch._params.emplace_back(commandNode[i].get<size_t>());
    }
}

Batch::NamedBatchData Deserializer::readNamedBatchData(const json& node) {
    Batch::NamedBatchData result;
    readOptionalVectorTransformed<BufferPointer>(result.buffers, node, keys::buffers, [this](const json& node) {
        uint32_t index = node;
        return buffers[index];
    });
    readOptionalVectorTransformed<Batch::DrawCallInfo>(result.drawCallInfos, node, keys::drawCallInfos,
                                                       [](const json& node) -> Batch::DrawCallInfo {
                                                           Batch::DrawCallInfo result{ 0 };
                                                           *((uint32_t*)&result) = node;
                                                           return result;
                                                       });
    return result;
}

BatchPointer Deserializer::readBatch(const json& node) {
    if (node.is_null()) {
        return nullptr;
    }

    std::string batchName;
    if (node.count(keys::name)) {
        batchName = node.value(keys::name, "");
    }
    BatchPointer result = std::make_shared<Batch>(batchName);
    auto& batch = *result;
    readOptional(batch._enableStereo, node, keys::stereo);
    readOptional(batch._enableSkybox, node, keys::skybox);
    readOptional(batch._isJitterOnProjectionEnabled, node, keys::isJitterOnProjectionEnabled);
    readOptional(batch._drawcallUniform, node, keys::drawcallUniform);
    readOptional(batch._drawcallUniformReset, node, keys::drawcallUniformReset);
    readPointerCache(batch._textures, node, keys::textures, textures);
    readPointerCache(batch._textureTables, node, keys::textureTables, textureTables);
    readPointerCache(batch._buffers, node, keys::buffers, buffers);
    readPointerCache(batch._pipelines, node, keys::pipelines, pipelines);
    readPointerCache(batch._streamFormats, node, keys::formats, formats);
    readPointerCache(batch._framebuffers, node, keys::framebuffers, framebuffers);
    readPointerCache(batch._swapChains, node, keys::swapchains, swapchains);
    readPointerCache(batch._queries, node, keys::queries, queries);

    readOptionalVectorTransformed<Batch::DrawCallInfo>(batch._drawCallInfos, node, keys::drawCallInfos,
                                                       [](const json& node) -> Batch::DrawCallInfo {
                                                           Batch::DrawCallInfo result{ 0 };
                                                           *((uint32_t*)&result) = node;
                                                           return result;
                                                       });

    readOptionalTransformed<std::vector<uint8_t>>(batch._data, node, keys::data,
                                                  [](const json& node) { return fromBase64(node); });

    for (const auto& commandNode : node[keys::commands]) {
        readCommand(commandNode, batch);
    }
    readBatchCacheTransformed<Transform, Transform>(batch._transforms, node, keys::transforms, &readTransform);
    readBatchCacheTransformed<std::string>(batch._profileRanges, node, keys::profileRanges);
    readBatchCacheTransformed<std::string>(batch._names, node, keys::names);

    auto objectTransformReader = [](const json& node) -> Batch::TransformObject {
        Batch::TransformObject result;
        result._model = readMat4(node);
        result._modelInverse = glm::inverse(result._model);
        return result;
    };
    readOptionalVectorTransformed<Batch::TransformObject>(batch._objects, node, keys::objects, objectTransformReader);

    if (node.count(keys::namedData)) {
        const auto& namedDataNode = node[keys::namedData];
        for (auto itr = namedDataNode.begin(); itr != namedDataNode.end(); ++itr) {
            auto name = itr.key();
            batch._namedData[name] = readNamedBatchData(itr.value());
        }
    }

    return result;
}

StereoState readStereoState(const json& node) {
    StereoState result;
    Deserializer::readOptional(result._enable, node, keys::enable);
    Deserializer::readOptional(result._contextDisable, node, keys::contextDisable);
    Deserializer::readOptional(result._skybox, node, keys::skybox);
    if (node.count(keys::eyeProjections)) {
        auto projections = node[keys::eyeProjections];
        result._eyeProjections[0] = Deserializer::readMat4(projections[0]);
        result._eyeProjections[1] = Deserializer::readMat4(projections[1]);
    }
    if (node.count(keys::eyeViews)) {
        auto views = node[keys::eyeViews];
        result._eyeViews[0] = Deserializer::readMat4(views[0]);
        result._eyeViews[1] = Deserializer::readMat4(views[1]);
    }
    return result;
}


FramePointer Deserializer::deserializeFrame() {
    if (!descriptor.operator bool()) {
        return {};
    }

    frameNode = json::parse(getStringChunk(0));

    FramePointer result = std::make_shared<Frame>();
    auto& frame = *result;

    if (frameNode.count(keys::buffers)) {
        readBuffers(frameNode[keys::buffers]);
    }

    shaders = readArray<ShaderPointer>(frameNode, keys::shaders, [](const json& node) { return readShader(node); });
    // Must come after shaders
    programs = readArray<ShaderPointer>(frameNode, keys::programs, [this](const json& node) { return readProgram(node); });
    // Must come after programs
    pipelines = readArray<PipelinePointer>(frameNode, keys::pipelines, [this](const json& node) { return readPipeline(node); });

    formats = readArray<Stream::FormatPointer>(frameNode, keys::formats, [](const json& node) { return readFormat(node); });

    textures = readArray<TexturePointer>(frameNode, keys::textures, [this](const json& node) { return readTexture(node, externalTexture); });

    // Must come after textures
    auto textureTableReader = [this](const json& node) { return readTextureTable(node); };
    textureTables = readArray<TextureTablePointer>(frameNode, keys::textureTables, textureTableReader);

    // Must come after textures
    auto framebufferReader = [this](const json& node) { return readFramebuffer(node); };
    framebuffers = readArray<FramebufferPointer>(frameNode, keys::framebuffers, framebufferReader);

    // Must come after textures & framebuffers
    swapchains =
        readArray<SwapChainPointer>(frameNode, keys::swapchains, [this](const json& node) { return readSwapchain(node); });

    queries = readArray<QueryPointer>(frameNode, keys::queries, [](const json& node) { return readQuery(node); });
    frame.framebuffer = framebuffers[frameNode[keys::framebuffer].get<uint32_t>()];
    frame.view = readMat4(frameNode[keys::view]);
    frame.pose = readMat4(frameNode[keys::pose]);
    frame.frameIndex = frameNode[keys::frameIndex];
    frame.stereoState = readStereoState(frameNode[keys::stereo]);
    if (frameNode.count(keys::batches)) {
        for (const auto& batchNode : frameNode[keys::batches]) {
            frame.batches.push_back(readBatch(batchNode));
        }
    }

    for (uint32_t i = 0; i < textures.size(); ++i) {
        const auto& texturePtr = textures[i];
        if (!texturePtr) {
            continue;
        }
        const auto& texture = *texturePtr;
        if (texture.getUsageType() == gpu::TextureUsageType::RESOURCE && texture.source().empty()) {
            qDebug() << "Empty source ";
        }
    }

    return result;
}

FramePointer Deserializer::readFrame() {
    auto result = deserializeFrame();
    result->finish();
    return result;
}
