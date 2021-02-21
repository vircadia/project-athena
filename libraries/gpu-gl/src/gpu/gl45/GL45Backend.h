//
//  GL45Backend.h
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 10/27/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_gpu_45_GL45Backend_h
#define hifi_gpu_45_GL45Backend_h

#include <gpu/gl/GLBackend.h>
#include <gpu/gl/GLTexture.h>

#include <thread>
#include <gpu/TextureTable.h>

#define INCREMENTAL_TRANSFER 0
#define GPU_SSBO_TRANSFORM_OBJECT 1
#define GPU_BINDLESS_TEXTURES 0

namespace gpu { namespace gl45 {

using namespace gpu::gl;
using TextureWeakPointer = std::weak_ptr<Texture>;

class GL45Backend : public GLBackend {
    using Parent = GLBackend;
    // Context Backend static interface required
    friend class Context;

public:
    static const GLint RESOURCE_TRANSFER_TEX_UNIT { 32 };
    static GLint MAX_COMBINED_SHADER_STORAGE_BLOCKS;
    static GLint MAX_UNIFORM_LOCATIONS;
#if GPU_BINDLESS_TEXTURES
    virtual bool supportsBindless() const override { return true; }
#endif

    explicit GL45Backend(bool syncCache);
    GL45Backend();
    virtual ~GL45Backend() {
        // call resetStages here rather than in ~GLBackend dtor because it will call releaseResourceBuffer
        // which is pure virtual from GLBackend's dtor.
        resetStages();
    }

    static const std::string GL45_VERSION;
    const std::string& getVersion() const override { return GL45_VERSION; }

    bool supportedTextureFormat(const gpu::Element& format) override;

    class GL45Texture : public GLTexture {
        using Parent = GLTexture;
        friend class GL45Backend;
        static GLuint allocate(const Texture& texture);

    protected:
        GL45Texture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        void generateMips() const override;
        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        void syncSampler() const override;

#if GPU_BINDLESS_TEXTURES
        bool isBindless() const {
            return _bindless.operator bool();
        }

        struct Bindless {
            uint64_t handle{ 0 };
            uint32_t minMip{ 0 };
            uint32_t sampler{ 0 };

            bool operator==(const Bindless& other) const {
                return handle == other.handle && minMip == other.minMip && sampler == other.sampler;
            }

            bool operator!=(const Bindless& other) const {
                return !(*this == other);
            }

            operator bool() const {
                return handle != 0;
            }
        };

        virtual const Bindless& getBindless() const;
        void releaseBindless() const;
        void recreateBindless() const;

    private:
        mutable Bindless _bindless;
#endif

        static Sampler getInvalidSampler();

        // This stores the texture handle (64 bits) in xy, the min mip available in z, and the sampler ID in w
        mutable Sampler _cachedSampler{ getInvalidSampler() };
    };

#if GPU_BINDLESS_TEXTURES
    class GL45TextureTable : public GLObject<TextureTable> {
        static GLuint allocate();
        using Parent = GLObject<TextureTable>;

    public:
        using BindlessArray = std::array<GL45Texture::Bindless, TextureTable::COUNT>;

        GL45TextureTable(const std::weak_ptr<GLBackend>& backend, const TextureTable& texture);
        ~GL45TextureTable();

        void update(const BindlessArray& newHandles);

        // FIXME instead of making a buffer for each table, there should be a global buffer of all materials
        // and we should store an offset into that buffer
        BindlessArray _handles;
    };
#endif

    //
    // Textures that have fixed allocation sizes and cannot be managed at runtime
    //

    class GL45FixedAllocationTexture : public GL45Texture {
        using Parent = GL45Texture;
        friend class GL45Backend;

    public:
        GL45FixedAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL45FixedAllocationTexture();

    protected:
        Size size() const override { return _size; }

        void allocateStorage() const;
        void syncSampler() const override;
        const Size _size{ 0 };
    };

    class GL45AttachmentTexture : public GL45FixedAllocationTexture {
        using Parent = GL45FixedAllocationTexture;
        friend class GL45Backend;

    protected:
        GL45AttachmentTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL45AttachmentTexture();
    };

    class GL45StrictResourceTexture : public GL45FixedAllocationTexture {
        using Parent = GL45FixedAllocationTexture;
        friend class GL45Backend;

    protected:
        GL45StrictResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL45StrictResourceTexture();
    };

    //
    // Textures that can be managed at runtime to increase or decrease their memory load
    //

    class GL45VariableAllocationTexture : public GL45Texture, public GLVariableAllocationSupport {
        using Parent = GL45Texture;
        friend class GL45Backend;
        using PromoteLambda = std::function<void()>;

    protected:
        GL45VariableAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL45VariableAllocationTexture();

        Size size() const override { return _size; }

        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        void copyTextureMipsInGPUMem(GLuint srcId, GLuint destId, uint16_t srcMipOffset, uint16_t destMipOffset, uint16_t populatedMips) override;

#if GPU_BINDLESS_TEXTURES
        virtual const Bindless& getBindless() const override;
#endif
    };

    class GL45ResourceTexture : public GL45VariableAllocationTexture {
        using Parent = GL45VariableAllocationTexture;
        friend class GL45Backend;

    protected:
        GL45ResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);

        void syncSampler() const override;
        size_t promote() override;
        size_t demote() override;
        void populateTransferQueue(TransferQueue& pendingTransfers) override;

        void allocateStorage(uint16 mip);
        Size copyMipsFromTexture();
    };

#if 0
    class GL45SparseResourceTexture : public GL45VariableAllocationTexture {
        using Parent = GL45VariableAllocationTexture;
        friend class GL45Backend;
        using TextureTypeFormat = std::pair<GLenum, GLenum>;
        using PageDimensions = std::vector<uvec3>;
        using PageDimensionsMap = std::map<TextureTypeFormat, PageDimensions>;
        static PageDimensionsMap pageDimensionsByFormat;
        static Mutex pageDimensionsMutex;

        static bool isSparseEligible(const Texture& texture);
        static PageDimensions getPageDimensionsForFormat(const TextureTypeFormat& typeFormat);
        static PageDimensions getPageDimensionsForFormat(GLenum type, GLenum format);
        static const uint32_t DEFAULT_PAGE_DIMENSION = 128;
        static const uint32_t DEFAULT_MAX_SPARSE_LEVEL = 0xFFFF;

    protected:
        GL45SparseResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL45SparseResourceTexture();
        uint32 size() const override { return _allocatedPages * _pageBytes; }
        void promote() override;
        void demote() override;

    private:
        uvec3 getPageCounts(const uvec3& dimensions) const;
        uint32_t getPageCount(const uvec3& dimensions) const;

        uint32_t _allocatedPages { 0 };
        uint32_t _pageBytes { 0 };
        uvec3 _pageDimensions { DEFAULT_PAGE_DIMENSION };
        GLuint _maxSparseLevel { DEFAULT_MAX_SPARSE_LEVEL };
    };
#endif

protected:

    void draw(GLenum mode, uint32 numVertices, uint32 startVertex) override;
    void recycle() const override;

    GLuint getFramebufferID(const FramebufferPointer& framebuffer) override;
    GLFramebuffer* syncGPUObject(const Framebuffer& framebuffer) override;

    GLuint getBufferID(const Buffer& buffer) override;
    GLuint getBufferIDUnsynced(const Buffer& buffer) override;
    GLBuffer* syncGPUObject(const Buffer& buffer) override;

    GLTexture* syncGPUObject(const TexturePointer& texture) override;

    GLuint getQueryID(const QueryPointer& query) override;
    GLQuery* syncGPUObject(const Query& query) override;

    // Draw Stage
    void do_draw(const Batch& batch, size_t paramOffset) override;
    void do_drawIndexed(const Batch& batch, size_t paramOffset) override;
    void do_drawInstanced(const Batch& batch, size_t paramOffset) override;
    void do_drawIndexedInstanced(const Batch& batch, size_t paramOffset) override;
    void do_multiDrawIndirect(const Batch& batch, size_t paramOffset) override;
    void do_multiDrawIndexedIndirect(const Batch& batch, size_t paramOffset) override;

    // Input Stage
    void resetInputStage() override;
    void updateInput() override;

    // Synchronize the state cache of this Backend with the actual real state of the GL Context
    void transferTransformState(const Batch& batch) const override;
    void initTransform() override;
    void updateTransform(const Batch& batch) override;

    // Resource Stage
    bool bindResourceBuffer(uint32_t slot, const BufferPointer& buffer) override;
    void releaseResourceBuffer(uint32_t slot) override;

    void do_copySavedViewProjectionTransformToBuffer(const Batch& batch, size_t paramOffset) override;

    // Output stage
    void do_blit(const Batch& batch, size_t paramOffset) override;

    // Shader Stage
    shader::Dialect getShaderDialect() const override;

    // Texture Management Stage
    void initTextureManagementStage() override;

#if GPU_BINDLESS_TEXTURES
    GL45TextureTable* syncGPUObject(const TextureTablePointer& textureTable);
    // Resource stage
    void do_setResourceTextureTable(const Batch& batch, size_t paramOffset) override;
#endif
};

}}  // namespace gpu::gl45

Q_DECLARE_LOGGING_CATEGORY(gpugl45logging)

#endif
