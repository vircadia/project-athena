//
//  GLESBackend.h
//  libraries/gpu-gl-android/src/gpu/gles
//
//  Created by Gabriel Calero & Cristian Duarte on 9/27/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_gles_GLESBackend_h
#define hifi_gpu_gles_GLESBackend_h

#include <gl/Config.h>

#include <gpu/gl/GLBackend.h>
#include <gpu/gl/GLTexture.h>


namespace gpu { namespace gles {

using namespace gpu::gl;

class GLESBackend : public GLBackend {
    using Parent = GLBackend;
    // Context Backend static interface required
    friend class Context;

public:
    static const GLint RESOURCE_TRANSFER_TEX_UNIT { 32 };
    static const GLint RESOURCE_TRANSFER_EXTRA_TEX_UNIT { 33 };
    static const GLint RESOURCE_BUFFER_TEXBUF_TEX_UNIT { 34 };
    static const GLint RESOURCE_BUFFER_SLOT0_TEX_UNIT { 35 };

    explicit GLESBackend(bool syncCache) : Parent(syncCache) {}
    GLESBackend() : Parent() {}
    virtual ~GLESBackend() {
        // call resetStages here rather than in ~GLBackend dtor because it will call releaseResourceBuffer
        // which is pure virtual from GLBackend's dtor.
        resetStages();
    }

    bool supportedTextureFormat(const gpu::Element& format) override;
    
    static const std::string GLES_VERSION;
    const std::string& getVersion() const override { return GLES_VERSION; }

    class GLESTexture : public GLTexture {
        using Parent = GLTexture;
        friend class GLESBackend;
        friend class GLESFramebuffer;
        GLuint allocate(const Texture& texture);
    protected:
        GLESTexture(const std::weak_ptr<GLBackend>& backend, const Texture& buffer);
        void generateMips() const override;
        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        void syncSampler() const override;

        void withPreservedTexture(std::function<void()> f) const;
    };

    //
    // Textures that have fixed allocation sizes and cannot be managed at runtime
    //

    class GLESFixedAllocationTexture : public GLESTexture {
        using Parent = GLESTexture;
        friend class GLESBackend;

    public:
        GLESFixedAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GLESFixedAllocationTexture();

    protected:
        Size size() const override { return _size; }
        void allocateStorage() const;
        void syncSampler() const override;
        const Size _size { 0 };
    };

    class GLESAttachmentTexture : public GLESFixedAllocationTexture {
        using Parent = GLESFixedAllocationTexture;
        friend class GLESBackend;
    protected:
        GLESAttachmentTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GLESAttachmentTexture();
    };

    class GLESStrictResourceTexture : public GLESFixedAllocationTexture {
        using Parent = GLESFixedAllocationTexture;
        friend class GLESBackend;
    protected:
        GLESStrictResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GLESStrictResourceTexture();
    };

    class GLESVariableAllocationTexture : public GLESTexture, public GLVariableAllocationSupport {
        using Parent = GLESTexture;
        friend class GLESBackend;
        using PromoteLambda = std::function<void()>;


    protected:
        GLESVariableAllocationTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GLESVariableAllocationTexture();

        void allocateStorage(uint16 allocatedMip);
        void syncSampler() const override;
        size_t promote() override;
        size_t demote() override;
        void populateTransferQueue(TransferJob::Queue& queue) override;

        Size copyMipFaceLinesFromTexture(uint16_t mip, uint8_t face, const uvec3& size, uint32_t yOffset, GLenum internalFormat, GLenum format, GLenum type, Size sourceSize, const void* sourcePointer) const override;
        Size copyMipsFromTexture();

        void copyTextureMipsInGPUMem(GLuint srcId, GLuint destId, uint16_t srcMipOffset, uint16_t destMipOffset, uint16_t populatedMips) override;

        Size size() const override { return _size; }
    };

    class GLESResourceTexture : public GLESVariableAllocationTexture {
        using Parent = GLESVariableAllocationTexture;
        friend class GLESBackend;
    protected:
        GLESResourceTexture(const std::weak_ptr<GLBackend>& backend, const Texture& texture);
        ~GLESResourceTexture();
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
    
    shader::Dialect getShaderDialect() const override { return shader::Dialect::glsl310es; }
};

} }

Q_DECLARE_LOGGING_CATEGORY(gpugleslogging)


#endif
