//
//  GL41Backend.h
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 10/27/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_41_GL41Backend_h
#define hifi_gpu_41_GL41Backend_h

#include <gl/Config.h>

#include <gpu/gl/GLBackend.h>
#include <gpu/gl/GLTexture.h>

#define GPU_CORE_41 410
#define GPU_CORE_43 430

#define GPU_INPUT_PROFILE GPU_CORE_41

namespace gpu { namespace gl41 {

using namespace gpu::gl;

class GL41Backend : public GLBackend {
    using Parent = GLBackend;
    // Context Backend static interface required
    friend class Context;

public:
    static const GLint RESOURCE_TRANSFER_TEX_UNIT { 32 };
    static const GLint RESOURCE_TRANSFER_EXTRA_TEX_UNIT { 33 };
    static const GLint RESOURCE_BUFFER_TEXBUF_TEX_UNIT { 34 };
    static const GLint RESOURCE_BUFFER_SLOT0_TEX_UNIT { 35 };

    explicit GL41Backend(bool syncCache) : Parent(syncCache) {}
    GL41Backend() : Parent() {}
    virtual ~GL41Backend() {
        // call resetStages here rather than in ~GLBackend dtor because it will call releaseResourceBuffer
        // which is pure virtual from GLBackend's dtor.
        resetStages();
    }

    static const std::string GL41_VERSION;
    const std::string& getVersion() const override { return GL41_VERSION; }

    bool supportedTextureFormat(const gpu::Element& format) override;

    class GL41Texture : public GLTexture {
        using Parent = GLTexture;
        friend class GL41Backend;
        static GLuint allocate(const Texture& texture);
    protected:
        GL41Texture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        void generateMips() const override;
        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        void syncSampler() const override;

        void withPreservedTexture(std::function<void()> f) const;
    };

    //
    // Textures that have fixed allocation sizes and cannot be managed at runtime
    //

    class GL41FixedAllocationTexture : public GL41Texture {
        using Parent = GL41Texture;
        friend class GL41Backend;

    public:
        GL41FixedAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL41FixedAllocationTexture();

    protected:
        Size size() const override { return _size; }
        void allocateStorage() const;
        void syncSampler() const override;
        const Size _size { 0 };
    };

    class GL41AttachmentTexture : public GL41FixedAllocationTexture {
        using Parent = GL41FixedAllocationTexture;
        friend class GL41Backend;
    protected:
        GL41AttachmentTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL41AttachmentTexture();
    };

    class GL41StrictResourceTexture : public GL41FixedAllocationTexture {
        using Parent = GL41FixedAllocationTexture;
        friend class GL41Backend;
    protected:
        GL41StrictResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL41StrictResourceTexture();
    };

    class GL41VariableAllocationTexture : public GL41Texture, public GLVariableAllocationSupport {
        using Parent = GL41Texture;
        friend class GL41Backend;
        using PromoteLambda = std::function<void()>;


    protected:
        GL41VariableAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL41VariableAllocationTexture();

        void allocateStorage(uint16 allocatedMip);
        void syncSampler() const override;
        size_t promote() override;
        size_t demote() override;
        void populateTransferQueue(TransferQueue& pendingTransfers) override;

        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        Size copyMipsFromTexture();

        void copyTextureMipsInGPUMem(GLuint srcId, GLuint destId, uint16_t srcMipOffset, uint16_t destMipOffset, uint16_t populatedMips) override;

        Size size() const override { return _size; }
    };

    class GL41ResourceTexture : public GL41VariableAllocationTexture {
        using Parent = GL41VariableAllocationTexture;
        friend class GL41Backend;
    protected:
        GL41ResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GL41ResourceTexture();
    };

protected:

    void draw(GLenum mode, uint32 numVertices, uint32 startVertex) override;

    GLuint getFramebufferID(const FramebufferPointer& framebuffer) override;
    GLFramebuffer* syncGPUObject(const Framebuffer& framebuffer) override;

    GLuint getBufferID(const Buffer& buffer) override;
    GLuint getBufferIDUnsynced(const Buffer& buffer) override;
    GLuint getResourceBufferID(const Buffer& buffer);
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

    shader::Dialect getShaderDialect() const override { return shader::Dialect::glsl410; }
    void postLinkProgram(ShaderObject& programObject, const Shader& program) const override;
};

} }

Q_DECLARE_LOGGING_CATEGORY(gpugl41logging)


#endif
