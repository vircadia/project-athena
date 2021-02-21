//
//  GL45BackendTexture.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 1/19/2015.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GL45Backend.h"
#include <mutex>
#include <algorithm>
#include <condition_variable>
#include <unordered_set>
#include <unordered_map>
#include <glm/gtx/component_wise.hpp>

#include <QtCore/QDebug>

#include <NumericalConstants.h>
#include <gl/Context.h>
#include <gpu/TextureTable.h>
#include <gpu/gl/GLTexelFormat.h>

static const QString FORCE_MOBILE_TEXTURES_STRING{ "HIFI_FORCE_MOBILE_TEXTURES" };
static bool FORCE_MOBILE_TEXTURES = QProcessEnvironment::systemEnvironment().contains(FORCE_MOBILE_TEXTURES_STRING);


using namespace gpu;
using namespace gpu::gl;
using namespace gpu::gl45;

#define FORCE_STRICT_TEXTURE 0
#define ENABLE_SPARSE_TEXTURE 0

bool GL45Backend::supportedTextureFormat(const gpu::Element& format) {
    switch (format.getSemantic()) {
        // ETC textures are actually required by the OpenGL spec as of 4.3, but aren't always supported by hardware
        // They'll be recompressed by OpenGL, which will be slow or have poor quality, so disable them for now
        case gpu::Semantic::COMPRESSED_ETC2_RGB:
        case gpu::Semantic::COMPRESSED_ETC2_SRGB:
        case gpu::Semantic::COMPRESSED_ETC2_RGB_PUNCHTHROUGH_ALPHA:
        case gpu::Semantic::COMPRESSED_ETC2_SRGB_PUNCHTHROUGH_ALPHA:
        case gpu::Semantic::COMPRESSED_ETC2_RGBA:
        case gpu::Semantic::COMPRESSED_ETC2_SRGBA:
        case gpu::Semantic::COMPRESSED_EAC_RED:
        case gpu::Semantic::COMPRESSED_EAC_RED_SIGNED:
        case gpu::Semantic::COMPRESSED_EAC_XY:
        case gpu::Semantic::COMPRESSED_EAC_XY_SIGNED:
            return FORCE_MOBILE_TEXTURES;

        default:
            return FORCE_MOBILE_TEXTURES ? !format.isCompressed() : true;
    }
}

GLTexture* GL45Backend::syncGPUObject(const TexturePointer& texturePointer) {
    if (!texturePointer) {
        return nullptr;
    }

    const Texture& texture = *texturePointer;
    if (TextureUsageType::EXTERNAL == texture.getUsageType()) {
        return Parent::syncGPUObject(texturePointer);
    }

    if (!texture.isDefined()) {
        // NO texture definition yet so let's avoid thinking
        return nullptr;
    }

    GL45Texture* object = Backend::getGPUObject<GL45Texture>(texture);
    if (!object) {
        switch (texture.getUsageType()) {
            case TextureUsageType::RENDERBUFFER:
                object = new GL45AttachmentTexture(shared_from_this(), texture);
                break;

#if FORCE_STRICT_TEXTURE
            case TextureUsageType::RESOURCE:
#endif
            case TextureUsageType::STRICT_RESOURCE:
                qCDebug(gpugllogging) << "Strict texture " << texture.source().c_str();
                object = new GL45StrictResourceTexture(shared_from_this(), texture);
                break;

#if !FORCE_STRICT_TEXTURE
            case TextureUsageType::RESOURCE: {
                auto& transferEngine  = _textureManagement._transferEngine;
                if (transferEngine->allowCreate()) {
#if ENABLE_SPARSE_TEXTURE
                    if (isTextureManagementSparseEnabled() && GL45Texture::isSparseEligible(texture)) {
                        object = new GL45SparseResourceTexture(shared_from_this(), texture);
                    } else {
                        object = new GL45ResourceTexture(shared_from_this(), texture);
                    }
#else 
                    object = new GL45ResourceTexture(shared_from_this(), texture);
#endif
                    transferEngine->addMemoryManagedTexture(texturePointer);
                } else {
                    auto fallback = texturePointer->getFallbackTexture();
                    if (fallback) {
                        object = static_cast<GL45Texture*>(syncGPUObject(fallback));
                    }
                }
                break;
            }
#endif
            default:
                Q_UNREACHABLE();
        }
    } else {

        if (texture.getUsageType() == TextureUsageType::RESOURCE) {
            auto varTex = static_cast<GL45VariableAllocationTexture*> (object);

            if (varTex->_minAllocatedMip > 0) {
                auto minAvailableMip = texture.minAvailableMipLevel();
                if (minAvailableMip < varTex->_minAllocatedMip) {
                    varTex->_minAllocatedMip = minAvailableMip;
                }
            }
        }
    }

    return object;
}

void GL45Backend::initTextureManagementStage() {
    GLBackend::initTextureManagementStage();
    // enable the Sparse Texture on gl45
    _textureManagement._sparseCapable = true;

    // But now let s refine the behavior based on vendor
    std::string vendor { (const char*)glGetString(GL_VENDOR) };
    if ((vendor.find("AMD") != std::string::npos) || (vendor.find("ATI") != std::string::npos) || (vendor.find("INTEL") != std::string::npos)) {
        qCDebug(gpugllogging) << "GPU is sparse capable but force it off, vendor = " << vendor.c_str();
        _textureManagement._sparseCapable = false;
    } else {
        qCDebug(gpugllogging) << "GPU is sparse capable, vendor = " << vendor.c_str();
    }
}

using GL45Texture = GL45Backend::GL45Texture;


class GLSamplerCache {
public:
    GLuint getGLSampler(const Sampler& sampler) {
        if (0 == _samplerCache.count(sampler)) {
            GLuint result = 0;
            glGenSamplers(1, &result);
            const auto& fm = GLTexture::FILTER_MODES[sampler.getFilter()];
            glSamplerParameteri(result, GL_TEXTURE_MIN_FILTER, fm.minFilter);
            glSamplerParameteri(result, GL_TEXTURE_MAG_FILTER, fm.magFilter);
            if (sampler.doComparison()) {
                glSamplerParameteri(result, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glSamplerParameteri(result, GL_TEXTURE_COMPARE_FUNC, COMPARISON_TO_GL[sampler.getComparisonFunction()]);
            } else {
                glSamplerParameteri(result, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            }

            glSamplerParameteri(result, GL_TEXTURE_WRAP_S, GLTexture::WRAP_MODES[sampler.getWrapModeU()]);
            glSamplerParameteri(result, GL_TEXTURE_WRAP_T, GLTexture::WRAP_MODES[sampler.getWrapModeV()]);
            glSamplerParameteri(result, GL_TEXTURE_WRAP_R, GLTexture::WRAP_MODES[sampler.getWrapModeW()]);

            glSamplerParameterf(result, GL_TEXTURE_MAX_ANISOTROPY, sampler.getMaxAnisotropy());
            glSamplerParameterfv(result, GL_TEXTURE_BORDER_COLOR, (const float*)&sampler.getBorderColor());

            glSamplerParameterf(result, GL_TEXTURE_MIN_LOD, sampler.getMinMip());
            glSamplerParameterf(result, GL_TEXTURE_MAX_LOD, (sampler.getMaxMip() == Sampler::MAX_MIP_LEVEL ? 1000.f : sampler.getMaxMip()));
            _samplerCache[sampler] = result;
            return result;
        }

        return _samplerCache[sampler];
    }

    void releaseGLSampler(GLuint sampler) {
        // NO OP
    }

private:
    std::unordered_map<Sampler, GLuint> _samplerCache;
};

static GLSamplerCache SAMPLER_CACHE;


Sampler GL45Texture::getInvalidSampler() {
    static Sampler INVALID_SAMPLER;
    static std::once_flag once;
    std::call_once(once, [] {
        Sampler::Desc invalidDesc;
        invalidDesc._borderColor = vec4(-1.0f);
        INVALID_SAMPLER = Sampler(invalidDesc);
    });
    return INVALID_SAMPLER;
}

GL45Texture::GL45Texture(const std::weak_ptr<GLBackend>& backend, const Texture& texture)
    : GLTexture(backend, texture, allocate(texture)) {
}

GLuint GL45Texture::allocate(const Texture& texture) {
    GLuint result;
    glCreateTextures(getGLTextureType(texture), 1, &result);
    if (::gl::Context::enableDebugLogger()) {
        auto source = texture.source();
        glObjectLabel(GL_TEXTURE, result, (GLsizei)source.length(), source.data());
    }
    return result;
}

void GL45Texture::generateMips() const {
    glGenerateTextureMipmap(_id);
    (void)CHECK_GL_ERROR();
}

// (NOTE: it seems to work now, but for posterity:) DSA ARB does not work on AMD, so use EXT
// unless EXT is not available on the driver
#define AMD_CUBE_MAP_EXT_WORKAROUND 0

Size GL45Texture::copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const {
    Size amountCopied = sourceSize;
    bool compressed = GLTexelFormat::isCompressed(internalFormat);
    if (GL_TEXTURE_2D == _target) {
        if (compressed) {
            glCompressedTextureSubImage2D(_id, mip, 0, yOffset, size.x, size.y, internalFormat,
                                            static_cast<GLsizei>(sourceSize), sourcePointer);
        } else {
            glTextureSubImage2D(_id, mip, 0, yOffset, size.x, size.y, format, type, sourcePointer);
        }
    } else if (GL_TEXTURE_CUBE_MAP == _target) {
        // DSA and cubemap functions are notoriously buggy. use the 4.1 compatible pathway
        glActiveTexture(GL_TEXTURE0 + GL45Backend::RESOURCE_TRANSFER_TEX_UNIT);
        glBindTexture(_target, _texture);
        auto target = GLTexture::CUBE_FACE_LAYOUT[face];
        if (compressed) {
            glCompressedTexSubImage2D(target, mip, 0, yOffset, size.x, size.y, internalFormat,
                                        static_cast<GLsizei>(sourceSize), sourcePointer);
        } else {
            glTexSubImage2D(target, mip, 0, yOffset, size.x, size.y, format, type, sourcePointer);
        }
        glBindTexture(_target, 0);
    } else {
        assert(false);
        amountCopied = 0;
    }
    (void)CHECK_GL_ERROR();
    return amountCopied;
}

#if GPU_BINDLESS_TEXTURES
void GL45Texture::releaseBindless() const {
    // Release the old handler
    SAMPLER_CACHE.releaseGLSampler(_bindless.sampler);
    glMakeTextureHandleNonResidentARB(_bindless.handle);
    _bindless = Bindless();
}

void GL45Texture::recreateBindless() const {
    if (isBindless()) {
        releaseBindless();
    } else {
        // Once a texture is about to become bindless, it's base mip level MUST be set to 0
        glTextureParameteri(_id, GL_TEXTURE_BASE_LEVEL, 0);
    }

    _bindless.sampler = SAMPLER_CACHE.getGLSampler(_cachedSampler);
    _bindless.handle = glGetTextureSamplerHandleARB(_id, _bindless.sampler);
    glMakeTextureHandleResidentARB(_bindless.handle);
}

const GL45Texture::Bindless& GL45Texture::getBindless() const {
    if (!_bindless) {
        recreateBindless();
    }
    return _bindless;
}
#endif


void GL45Texture::syncSampler() const {
    const Sampler& sampler = _gpuObject.getSampler();
    if (_cachedSampler == sampler) {
        return;
    } 

    _cachedSampler = sampler;

#if GPU_BINDLESS_TEXTURES
    if (isBindless()) {
        recreateBindless();
        return;
    } 
#endif

    const auto& fm = FILTER_MODES[sampler.getFilter()];
    glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, fm.minFilter);
    glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, fm.magFilter);

    if (sampler.doComparison()) {
        glTextureParameteri(_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTextureParameteri(_id, GL_TEXTURE_COMPARE_FUNC, COMPARISON_TO_GL[sampler.getComparisonFunction()]);
    } else {
        glTextureParameteri(_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

    glTextureParameteri(_id, GL_TEXTURE_WRAP_S, WRAP_MODES[sampler.getWrapModeU()]);
    glTextureParameteri(_id, GL_TEXTURE_WRAP_T, WRAP_MODES[sampler.getWrapModeV()]);
    glTextureParameteri(_id, GL_TEXTURE_WRAP_R, WRAP_MODES[sampler.getWrapModeW()]);

    glTextureParameterf(_id, GL_TEXTURE_MAX_ANISOTROPY, sampler.getMaxAnisotropy());
    glTextureParameterfv(_id, GL_TEXTURE_BORDER_COLOR, (const float*)&sampler.getBorderColor());

    glTextureParameterf(_id, GL_TEXTURE_MIN_LOD, sampler.getMinMip());
    glTextureParameterf(_id, GL_TEXTURE_MAX_LOD, (sampler.getMaxMip() == Sampler::MAX_MIP_LEVEL ? 1000.f : sampler.getMaxMip()));
}

// Fixed allocation textures, used for strict resources & framebuffer attachments

using GL45FixedAllocationTexture = GL45Backend::GL45FixedAllocationTexture;

GL45FixedAllocationTexture::GL45FixedAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture) : GL45Texture(backend, texture), _size(texture.evalTotalSize()) {
    allocateStorage();
    syncSampler();
}

GL45FixedAllocationTexture::~GL45FixedAllocationTexture() {
}

void GL45FixedAllocationTexture::allocateStorage() const {
    const GLTexelFormat texelFormat = GLTexelFormat::evalGLTexelFormat(_gpuObject.getTexelFormat());
    const auto dimensions = _gpuObject.getDimensions();
    const auto mips = _gpuObject.getNumMips();
    const auto numSlices = _gpuObject.getNumSlices();
    const auto numSamples = _gpuObject.getNumSamples();


    if (!_gpuObject.isMultisample()) {
        if (!_gpuObject.isArray()) {
            glTextureStorage2D(_id, mips, texelFormat.internalFormat, dimensions.x, dimensions.y);
        } else {
            glTextureStorage3D(_id, mips, texelFormat.internalFormat, dimensions.x, dimensions.y, numSlices);
        }
    } else {
        if (!_gpuObject.isArray()) {
            glTextureStorage2DMultisample(_id, numSamples, texelFormat.internalFormat, dimensions.x, dimensions.y, GL_FALSE);
        }
        else {
            glTextureStorage3DMultisample(_id, numSamples, texelFormat.internalFormat, dimensions.x, dimensions.y, numSlices, GL_FALSE);
        }
    }

    glTextureParameteri(_id, GL_TEXTURE_BASE_LEVEL, 0);
    glTextureParameteri(_id, GL_TEXTURE_MAX_LEVEL, mips - 1);
}

void GL45FixedAllocationTexture::syncSampler() const {
    Parent::syncSampler();
    const Sampler& sampler = _gpuObject.getSampler();
    glTextureParameterf(_id, GL_TEXTURE_MIN_LOD, (float)sampler.getMinMip());
    glTextureParameterf(_id, GL_TEXTURE_MAX_LOD, (sampler.getMaxMip() == Sampler::MAX_MIP_LEVEL ? 1000.f : sampler.getMaxMip()));
}

// Renderbuffer attachment textures
using GL45AttachmentTexture = GL45Backend::GL45AttachmentTexture;

GL45AttachmentTexture::GL45AttachmentTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture) : GL45FixedAllocationTexture(backend, texture) {
    Backend::textureFramebufferCount.increment();
    Backend::textureFramebufferGPUMemSize.update(0, size());
}

GL45AttachmentTexture::~GL45AttachmentTexture() {
    Backend::textureFramebufferCount.decrement();
    Backend::textureFramebufferGPUMemSize.update(size(), 0);
}

// Strict resource textures
using GL45StrictResourceTexture = GL45Backend::GL45StrictResourceTexture;

GL45StrictResourceTexture::GL45StrictResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture) : GL45FixedAllocationTexture(backend, texture) {
    Backend::textureResidentCount.increment();
    Backend::textureResidentGPUMemSize.update(0, size());

    auto mipLevels = _gpuObject.getNumMips();
    for (uint16_t sourceMip = 0; sourceMip < mipLevels; ++sourceMip) {
        uint16_t targetMip = sourceMip;
        size_t maxFace = GLTexture::getFaceCount(_target);
        for (uint8_t face = 0; face < maxFace; ++face) {
            copyMipFaceFromTexture(sourceMip, targetMip, face);
        }
    }
    if (texture.isAutogenerateMips()) {
        generateMips();
    }

    // Re-sync the sampler to force access to the new mip level
    syncSampler();
}

GL45StrictResourceTexture::~GL45StrictResourceTexture() {
    Backend::textureResidentCount.decrement();
    Backend::textureResidentGPUMemSize.update(size(), 0);
}

// Encapsulate bindless textures
#if GPU_BINDLESS_TEXTURES
using GL45TextureTable = GL45Backend::GL45TextureTable;

GLuint GL45TextureTable::allocate() {
    GLuint result;
    glCreateBuffers(1, &result);
    return result;
}

GL45TextureTable::GL45TextureTable(const std::weak_ptr<GLBackend>& backend, const TextureTable& textureTable)
    : Parent(backend, textureTable, allocate()){
    Backend::setGPUObject(textureTable, this);
    // FIXME include these in overall buffer storage reporting
    glNamedBufferStorage(_id, sizeof(uvec4) * TextureTable::COUNT, nullptr, GL_DYNAMIC_STORAGE_BIT);
}

void GL45TextureTable::update(const BindlessArray& handles) {
    if (_handles != handles) {
        _handles = handles;
        // FIXME include these in overall buffer storage reporting
        // FIXME use a single shared buffer for bindless data
        glNamedBufferSubData(_id, 0, sizeof(GL45Texture::Bindless) * TextureTable::COUNT, &_handles[0]);
    }
}

GL45TextureTable::~GL45TextureTable() {
    if (_id) {
        auto backend = _backend.lock();
        if (backend) {
            // FIXME include these in overall buffer storage reporting
            backend->releaseBuffer(_id, 0);
        }
    }
}

GL45TextureTable* GL45Backend::syncGPUObject(const TextureTablePointer& textureTablePointer) {
    const auto& textureTable = *textureTablePointer;

    // Find the target handles
    auto defaultTextures = gpu::TextureTable::getDefault()->getTextures();
    auto textures = textureTable.getTextures();
    GL45TextureTable::BindlessArray handles{};
    for (size_t i = 0; i < textures.size(); ++i) {
        auto texture = textures[i];
        if (!texture) {
            texture = defaultTextures[i];
        }
        if (!texture) {
            continue;
        }
        // FIXME what if we have a non-transferrable texture here?
        auto gltexture = (GL45Texture*)syncGPUObject(texture);
        if (!gltexture) {
            continue;
        }
        handles[i] = gltexture->getBindless();
    }

    // If the object hasn't been created, or the object definition is out of date, drop and re-create
    GL45TextureTable* object = Backend::getGPUObject<GL45TextureTable>(textureTable);

    if (!object) {
        object = new GL45TextureTable(shared_from_this(), textureTable);
    }

    object->update(handles);

    return object;
}

void GL45Backend::do_setResourceTextureTable(const Batch& batch, size_t paramOffset) {
    auto textureTablePointer = batch._textureTables.get(batch._params[paramOffset]._uint);
    if (!textureTablePointer) {
        return;
    }

    auto slot = batch._params[paramOffset + 1]._uint;
    GL45TextureTable* glTextureTable = syncGPUObject(textureTablePointer);
    if (glTextureTable) {
        glBindBufferBase(GL_UNIFORM_BUFFER, slot + GLBackend::RESOURCE_TABLE_TEXTURE_SLOT_OFFSET, glTextureTable->_id);
    }
}
#endif
