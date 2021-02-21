//
//  Batch.h
//  interface/src/gpu
//
//  Created by Sam Gateau on 10/14/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_Batch_h
#define hifi_gpu_Batch_h

#include <vector>
#include <mutex>
#include <functional>
#include <glm/gtc/type_ptr.hpp>

#include <shared/NsightHelpers.h>

#include "Framebuffer.h"
#include "Pipeline.h"
#include "Query.h"
#include "Stream.h"
#include "Texture.h"
#include "Transform.h"
#include "ShaderConstants.h"

class QDebug;
#define BATCH_PREALLOCATE_MIN 128
namespace gpu {

// The named batch data provides a mechanism for accumulating data into buffers over the course 
// of many independent calls.  For instance, two objects in the scene might both want to render 
// a simple box, but are otherwise unaware of each other.  The common code that they call to render
// the box can create buffers to store the rendering parameters for each box and register a function 
// that will be called with the accumulated buffer data when the batch commands are finally 
// executed against the backend


class Batch {
public:
    typedef Stream::Slot Slot;

    enum {
        MAX_TRANSFORM_SAVE_SLOT_COUNT = 6
    };

    class DrawCallInfo {
    public:
        using Index = uint16_t;

        DrawCallInfo(Index idx) : index(idx) {}
        DrawCallInfo(Index idx, Index user) : index(idx), unused(user) {}

        Index index { 0 };
        uint16_t unused { 0 }; // Reserved space for later

    };
    // Make sure DrawCallInfo has no extra padding
    static_assert(sizeof(DrawCallInfo) == 4, "DrawCallInfo size is incorrect.");

    using DrawCallInfoBuffer = std::vector<DrawCallInfo>;

    struct NamedBatchData {
        using BufferPointers = std::vector<BufferPointer>;
        using Function = std::function<void(gpu::Batch&, NamedBatchData&)>;

        BufferPointers buffers;
        Function function;
        DrawCallInfoBuffer drawCallInfos;

        size_t count() const { return drawCallInfos.size(); }

        void process(Batch& batch) {
            if (function) {
                function(batch, *this);
            }
        }
    };

    using NamedBatchDataMap = std::map<std::string, NamedBatchData>;

    DrawCallInfoBuffer _drawCallInfos;
    static size_t _drawCallInfosMax;

    mutable std::string _currentNamedCall;

    const DrawCallInfoBuffer& getDrawCallInfoBuffer() const;
    DrawCallInfoBuffer& getDrawCallInfoBuffer();

    void captureDrawCallInfo();
    void captureNamedDrawCallInfo(std::string name);

    Batch(const std::string& name = "");
    // Disallow copy construction and assignement of batches
    Batch(const Batch& batch) = delete;
    Batch& operator=(const Batch& batch) = delete;
    ~Batch();

    void setName(const std::string& name);
    const std::string& getName() const { return _name; }
    void clear();

    // Batches may need to override the context level stereo settings
    // if they're performing framebuffer copy operations, like the 
    // deferred lighting resolution mechanism
    void enableStereo(bool enable = true);
    bool isStereoEnabled() const;

    // Stereo batches will pre-translate the view matrix, but this isn't 
    // appropriate for skyboxes or other things intended to be drawn at 
    // infinite distance, so provide a mechanism to render in stereo 
    // without the pre-translation of the view.  
    void enableSkybox(bool enable = true);
    bool isSkyboxEnabled() const;

    // Drawcall Uniform value
    // One 16bit word uniform value is available during the drawcall
    // its value must be set before each drawcall
    void setDrawcallUniform(uint16 uniform);
    // It is reset to the reset value between each drawcalls
    // The reset value is 0 by default and can be changed as a batch state with this call
    void setDrawcallUniformReset(uint16 resetUniform);

    // Drawcalls
    void draw(Primitive primitiveType, uint32 numVertices, uint32 startVertex = 0);
    void drawIndexed(Primitive primitiveType, uint32 numIndices, uint32 startIndex = 0);
    void drawInstanced(uint32 numInstances, Primitive primitiveType, uint32 numVertices, uint32 startVertex = 0, uint32 startInstance = 0);
    void drawIndexedInstanced(uint32 numInstances, Primitive primitiveType, uint32 numIndices, uint32 startIndex = 0, uint32 startInstance = 0);
    void multiDrawIndirect(uint32 numCommands, Primitive primitiveType);
    void multiDrawIndexedIndirect(uint32 numCommands, Primitive primitiveType);

    void setupNamedCalls(const std::string& instanceName, NamedBatchData::Function function);
    const BufferPointer& getNamedBuffer(const std::string& instanceName, uint8_t index = 0);

    // Input Stage
    // InputFormat
    // InputBuffers
    // IndexBuffer
    void setInputFormat(const Stream::FormatPointer& format);

    void setInputBuffer(Slot channel, const BufferPointer& buffer, Offset offset, Offset stride);
    void setInputBuffer(Slot channel, const BufferView& buffer); // not a command, just a shortcut from a BufferView
    void setInputStream(Slot startChannel, const BufferStream& stream); // not a command, just unroll into a loop of setInputBuffer

    void setIndexBuffer(Type type, const BufferPointer& buffer, Offset offset);
    void setIndexBuffer(const BufferView& buffer); // not a command, just a shortcut from a BufferView

    // Indirect buffer is used by the multiDrawXXXIndirect calls
    // The indirect buffer contains the command descriptions to execute multiple drawcalls in a single call
    void setIndirectBuffer(const BufferPointer& buffer, Offset offset = 0, Offset stride = 0);
    
    // multi command desctription for multiDrawIndexedIndirect
    class DrawIndirectCommand {
    public:
        uint  _count { 0 };
        uint  _instanceCount { 0 };
        uint  _firstIndex { 0 };
        uint  _baseInstance { 0 };
    };

    // multi command desctription for multiDrawIndexedIndirect
    class DrawIndexedIndirectCommand {
    public:
        uint  _count { 0 };
        uint  _instanceCount { 0 };
        uint  _firstIndex { 0 };
        uint  _baseVertex { 0 };
        uint  _baseInstance { 0 };
    };

    // Transform Stage
    // Vertex position is transformed by ModelTransform from object space to world space
    // Then by the inverse of the ViewTransform from world space to eye space
    // finaly projected into the clip space by the projection transform
    // WARNING: ViewTransform transform from eye space to world space, its inverse is composed
    // with the ModelTransform to create the equivalent of the gl ModelViewMatrix
    void setModelTransform(const Transform& model);
    void setModelTransform(const Transform& model, const Transform& previousModel);
    void resetViewTransform() { setViewTransform(Transform(), false); }
    void setViewTransform(const Transform& view, bool camera = true);
    void setProjectionTransform(const Mat4& proj);
    void setProjectionJitterEnabled(bool isProjectionEnabled);
    void setProjectionJitterSequence(const Vec2* sequence, size_t count);
    void setProjectionJitterScale(float scale);
    // Very simple 1 level stack management of jitter.
    void pushProjectionJitterEnabled(bool isProjectionEnabled);
    void popProjectionJitterEnabled();
    // Viewport is xy = low left corner in framebuffer, zw = width height of the viewport, expressed in pixels
    void setViewportTransform(const Vec4i& viewport);
    void setDepthRangeTransform(float nearDepth, float farDepth);

    void saveViewProjectionTransform(uint32 saveSlot);
    void setSavedViewProjectionTransform(uint32 saveSlot);
    void copySavedViewProjectionTransformToBuffer(uint32 saveSlot, const BufferPointer& buffer, Offset offset);

    // Pipeline Stage
    void setPipeline(const PipelinePointer& pipeline);

    void setStateBlendFactor(const Vec4& factor);

    // Set the Scissor rect
    // the rect coordinates are xy for the low left corner of the rect and zw for the width and height of the rect, expressed in pixels
    void setStateScissorRect(const Vec4i& rect);

    void setUniformBuffer(uint32 slot, const BufferPointer& buffer, Offset offset, Offset size);
    void setUniformBuffer(uint32 slot, const BufferView& view); // not a command, just a shortcut from a BufferView

    void setResourceBuffer(uint32 slot, const BufferPointer& buffer);

    void setResourceTexture(uint32 slot, const TexturePointer& texture);
    void setResourceTexture(uint32 slot, const TextureView& view); // not a command, just a shortcut from a TextureView
    void setResourceTextureTable(const TextureTablePointer& table, uint32 slot = 0);
    void setResourceFramebufferSwapChainTexture(uint32 slot, const FramebufferSwapChainPointer& framebuffer, unsigned int swapChainIndex, unsigned int renderBufferSlot = 0U); // not a command, just a shortcut from a TextureView

    // Ouput Stage
    void setFramebuffer(const FramebufferPointer& framebuffer);
    void setFramebufferSwapChain(const FramebufferSwapChainPointer& framebuffer, unsigned int swapChainIndex);

    void advance(const SwapChainPointer& swapChain);

    // Clear framebuffer layers
    // Targets can be any of the render buffers contained in the currnetly bound Framebuffer
    // Optionally the scissor test can be enabled locally for this command and to restrict the clearing command to the pixels contained in the scissor rectangle
    void clearFramebuffer(Framebuffer::Masks targets, const Vec4& color, float depth, int stencil, bool enableScissor = false);
    void clearColorFramebuffer(Framebuffer::Masks targets, const Vec4& color, bool enableScissor = false); // not a command, just a shortcut for clearFramebuffer, mask out targets to make sure it touches only color targets
    void clearDepthFramebuffer(float depth, bool enableScissor = false); // not a command, just a shortcut for clearFramebuffer, it touches only depth target
    void clearStencilFramebuffer(int stencil, bool enableScissor = false); // not a command, just a shortcut for clearFramebuffer, it touches only stencil target
    void clearDepthStencilFramebuffer(float depth, int stencil, bool enableScissor = false); // not a command, just a shortcut for clearFramebuffer, it touches depth and stencil target

    // Blit src framebuffer to destination
    // the srcRect and dstRect are the rect region in source and destination framebuffers expressed in pixel space
    // with xy and zw the bounding corners of the rect region.
    void blit(const FramebufferPointer& src, const Vec4i& srcRect, const FramebufferPointer& dst, const Vec4i& dstRect);

    // Generate the mips for a texture
    void generateTextureMips(const TexturePointer& texture);
    // Generate the mips for a texture using the current pipeline
    void generateTextureMipsWithPipeline(const TexturePointer& destTexture, int numMips = -1);

    // Query Section
    void beginQuery(const QueryPointer& query);
    void endQuery(const QueryPointer& query);
    void getQuery(const QueryPointer& query);

    // Reset the stage caches and states
    void resetStages();

    void disableContextViewCorrection();
    void restoreContextViewCorrection();

    void disableContextStereo();
    void restoreContextStereo();

    // Debugging
    void pushProfileRange(const char* name);
    void popProfileRange();

    // TODO: As long as we have gl calls explicitely issued from interface
    // code, we need to be able to record and batch these calls. THe long 
    // term strategy is to get rid of any GL calls in favor of the HIFI GPU API
    // For now, instead of calling the raw gl Call, use the equivalent call on the batch so the call is beeing recorded
    // THe implementation of these functions is in GLBackend.cpp
    void _glUniform1i(int location, int v0);
    void _glUniform1f(int location, float v0);
    void _glUniform2f(int location, float v0, float v1);
    void _glUniform3f(int location, float v0, float v1, float v2);
    void _glUniform4f(int location, float v0, float v1, float v2, float v3);
    void _glUniform3fv(int location, int count, const float* value);
    void _glUniform4fv(int location, int count, const float* value);
    void _glUniform4iv(int location, int count, const int* value);
    void _glUniformMatrix3fv(int location, int count, unsigned char transpose, const float* value);
    void _glUniformMatrix4fv(int location, int count, unsigned char transpose, const float* value);

    void _glUniform(int location, int v0) {
        _glUniform1i(location, v0);
    }

    void _glUniform(int location, float v0) {
        _glUniform1f(location, v0);
    }

    void _glUniform(int location, const glm::vec2& v) {
        _glUniform2f(location, v.x, v.y);
    }

    void _glUniform(int location, const glm::vec3& v) {
        _glUniform3f(location, v.x, v.y, v.z);
    }

    void _glUniform(int location, const glm::vec4& v) {
        _glUniform4f(location, v.x, v.y, v.z, v.w);
    }

    void _glUniform(int location, const glm::mat3& v) {
        _glUniformMatrix3fv(location, 1, false, glm::value_ptr(v));
    }

    void _glColor4f(float red, float green, float blue, float alpha);

    // Maybe useful but shoudln't be public. Please convince me otherwise
    // Well porting to gles i need it...
    void runLambda(std::function<void()> f);

    enum Command {
        COMMAND_draw = 0,
        COMMAND_drawIndexed,
        COMMAND_drawInstanced,
        COMMAND_drawIndexedInstanced,
        COMMAND_multiDrawIndirect,
        COMMAND_multiDrawIndexedIndirect,

        COMMAND_setInputFormat,
        COMMAND_setInputBuffer,
        COMMAND_setIndexBuffer,
        COMMAND_setIndirectBuffer,

        COMMAND_setModelTransform,
        COMMAND_setViewTransform,
        COMMAND_setProjectionTransform,
        COMMAND_setProjectionJitterEnabled,
        COMMAND_setProjectionJitterSequence,
        COMMAND_setProjectionJitterScale,
        COMMAND_setViewportTransform,
        COMMAND_setDepthRangeTransform,

        COMMAND_saveViewProjectionTransform,
        COMMAND_setSavedViewProjectionTransform,
        COMMAND_copySavedViewProjectionTransformToBuffer,

        COMMAND_setPipeline,
        COMMAND_setStateBlendFactor,
        COMMAND_setStateScissorRect,

        COMMAND_setUniformBuffer,
        COMMAND_setResourceBuffer,
        COMMAND_setResourceTexture,
        COMMAND_setResourceTextureTable,
        COMMAND_setResourceFramebufferSwapChainTexture,

        COMMAND_setFramebuffer,
        COMMAND_setFramebufferSwapChain,
        COMMAND_clearFramebuffer,
        COMMAND_blit,
        COMMAND_generateTextureMips,
        COMMAND_generateTextureMipsWithPipeline,

        COMMAND_advance,

        COMMAND_beginQuery,
        COMMAND_endQuery,
        COMMAND_getQuery,

        COMMAND_resetStages,

        COMMAND_disableContextViewCorrection,
        COMMAND_restoreContextViewCorrection,

        COMMAND_disableContextStereo,
        COMMAND_restoreContextStereo,

        COMMAND_runLambda,

        COMMAND_startNamedCall,
        COMMAND_stopNamedCall,

        // TODO: As long as we have gl calls explicitely issued from interface
        // code, we need to be able to record and batch these calls. THe long 
        // term strategy is to get rid of any GL calls in favor of the HIFI GPU API
        COMMAND_glUniform1i,
        COMMAND_glUniform1f,
        COMMAND_glUniform2f,
        COMMAND_glUniform3f,
        COMMAND_glUniform4f,
        COMMAND_glUniform3fv,
        COMMAND_glUniform4fv,
        COMMAND_glUniform4iv,
        COMMAND_glUniformMatrix3fv,
        COMMAND_glUniformMatrix4fv,

        COMMAND_glColor4f,

        COMMAND_pushProfileRange,
        COMMAND_popProfileRange,

        NUM_COMMANDS,
    };
    typedef std::vector<Command> Commands;
    typedef std::vector<size_t> CommandOffsets;

    const Commands& getCommands() const { return _commands; }
    const CommandOffsets& getCommandOffsets() const { return _commandOffsets; }

    class Param {
    public:
        union {
#if (QT_POINTER_SIZE == 8)
            size_t _size;
#endif            
            int32 _int;
            uint32 _uint;
            float _float;
            char _chars[sizeof(size_t)];
        };
#if (QT_POINTER_SIZE == 8)
        Param(size_t val) : _size(val) {}
#endif            
        Param(int32 val) : _int(val) {}
        Param(uint32 val) : _uint(val) {}
        Param(float val) : _float(val) {}
    };
    typedef std::vector<Param> Params;

    const Params& getParams() const { return _params; }

    // The template cache mechanism for the gpu::Object passed to the gpu::Batch
    // this allow us to have one cache container for each different types and eventually
    // be smarter how we manage them
    template <typename T>
    class Cache {
    public:
        typedef T Data;
        Data _data;
        Cache<T>(const Data& data) : _data(data) {}
        static size_t _max;

        class Vector {
        public:
            std::vector< Cache<T> > _items;

            Vector() {
                _items.reserve(_max);
            }

            ~Vector() {
                _max = std::max(_items.size(), _max);
            }


            size_t size() const { return _items.size(); }
            size_t cache(const Data& data) {
                size_t offset = _items.size();
                _items.emplace_back(data);
                return offset;
            }

            const Data& get(uint32 offset) const {
                assert((offset < _items.size()));
                return (_items.data() + offset)->_data;
            }

            void clear() {
                _items.clear();
            }
        };
    };

    using CommandHandler = std::function<void(Command, const Param*)>;

    void forEachCommand(const CommandHandler& handler) const {
        size_t count = _commands.size();
        for (size_t i = 0; i < count; ++i) {
            const auto command = _commands[i];
            const auto offset = _commandOffsets[i];
            const Param* params = _params.data() + offset;
            handler(command, params);
        }
    }

    typedef Cache<BufferPointer>::Vector BufferCaches;
    typedef Cache<TexturePointer>::Vector TextureCaches;
    typedef Cache<TextureTablePointer>::Vector TextureTableCaches;
    typedef Cache<Stream::FormatPointer>::Vector StreamFormatCaches;
    typedef Cache<Transform>::Vector TransformCaches;
    typedef Cache<PipelinePointer>::Vector PipelineCaches;
    typedef Cache<FramebufferPointer>::Vector FramebufferCaches;
    typedef Cache<SwapChainPointer>::Vector SwapChainCaches;
    typedef Cache<QueryPointer>::Vector QueryCaches;
    typedef Cache<std::string>::Vector StringCaches;
    typedef Cache<std::function<void()>>::Vector LambdaCache;

    // Cache Data in a byte array if too big to fit in Param
    // FOr example Mat4s are going there
    typedef unsigned char Byte;
    typedef std::vector<Byte> Bytes;
    size_t cacheData(size_t size, const void* data);
    Byte* editData(size_t offset) {
        if (offset >= _data.size()) {
            return 0;
        }
        return (_data.data() + offset);
    }

    const Byte* readData(size_t offset) const {
        if (offset >= _data.size()) {
            return 0;
        }
        return (_data.data() + offset);
    }

    Commands _commands;
    static size_t _commandsMax;

    CommandOffsets _commandOffsets;
    static size_t _commandOffsetsMax;

    Params _params;
    static size_t _paramsMax;

    Bytes _data;
    static size_t _dataMax;

#include "TransformObject_shared.slh"

    using TransformObjects = std::vector<TransformObject>;
    bool _invalidModel { true };
    Transform _currentModel;
    Transform _previousModel;
    mutable bool _mustUpdatePreviousModels;
    mutable TransformObjects _objects;
    static size_t _objectsMax;

    BufferCaches _buffers;
    TextureCaches _textures;
    TextureTableCaches _textureTables;
    StreamFormatCaches _streamFormats;
    TransformCaches _transforms;
    PipelineCaches _pipelines;
    FramebufferCaches _framebuffers;
    SwapChainCaches _swapChains;
    QueryCaches _queries;
    LambdaCache _lambdas;
    StringCaches _profileRanges;
    StringCaches _names;

    NamedBatchDataMap _namedData;

    bool _isJitterOnProjectionEnabled { false };

    uint16_t _drawcallUniform { 0 };
    uint16_t _drawcallUniformReset { 0 };

    bool _enableStereo { true };
    bool _enableSkybox { false };

protected:
    std::string _name;

    friend class Context;
    friend class Frame;

    // Apply all the named calls to the end of the batch
    // and prepare updates for the render shadow copies of the buffers
    void finishFrame(BufferUpdates& updates);

    // Directly copy from the main data to the render thread shadow copy
    // MUST only be called on the render thread
    // MUST only be called on batches created on the render thread
    void flush();

    void startNamedCall(const std::string& name);
    void stopNamedCall();



    void captureDrawCallInfoImpl();
};

template <typename T>
size_t Batch::Cache<T>::_max = BATCH_PREALLOCATE_MIN;

}  // namespace gpu

#if defined(NSIGHT_FOUND)

class ProfileRangeBatch {
public:
    ProfileRangeBatch(gpu::Batch& batch, const char *name);
    ~ProfileRangeBatch();

private:
    gpu::Batch& _batch;
};

#define PROFILE_RANGE_BATCH(batch, name) ProfileRangeBatch profileRangeThis(batch, name);

#else

#define PROFILE_RANGE_BATCH(batch, name) 

#endif

#endif
