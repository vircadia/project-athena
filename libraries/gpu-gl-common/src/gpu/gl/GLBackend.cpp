//
//  GLBackend.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 10/27/2014.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GLBackend.h"


#include <mutex>
#include <queue>
#include <list>
#include <functional>
#include <glm/gtc/type_ptr.hpp>
#include "gl/Config.h"

#if defined(NSIGHT_FOUND)
#include "nvToolsExt.h"
#endif

// Define the GPU_BATCH_DETAILED_TRACING to get detailed tracing of the commands during the batch executions
// #define GPU_BATCH_DETAILED_TRACING

#include <GPUIdent.h>

#include "GLTexture.h"
#include "GLShader.h"

using namespace gpu;
using namespace gpu::gl;

GLBackend::CommandCall GLBackend::_commandCalls[Batch::NUM_COMMANDS] = 
{
    (&::gpu::gl::GLBackend::do_draw), 
    (&::gpu::gl::GLBackend::do_drawIndexed),
    (&::gpu::gl::GLBackend::do_drawInstanced),
    (&::gpu::gl::GLBackend::do_drawIndexedInstanced),
    (&::gpu::gl::GLBackend::do_multiDrawIndirect),
    (&::gpu::gl::GLBackend::do_multiDrawIndexedIndirect),

    (&::gpu::gl::GLBackend::do_setInputFormat),
    (&::gpu::gl::GLBackend::do_setInputBuffer),
    (&::gpu::gl::GLBackend::do_setIndexBuffer),
    (&::gpu::gl::GLBackend::do_setIndirectBuffer),

    (&::gpu::gl::GLBackend::do_setModelTransform),
    (&::gpu::gl::GLBackend::do_setViewTransform),
    (&::gpu::gl::GLBackend::do_setProjectionTransform),
    (&::gpu::gl::GLBackend::do_setProjectionJitterEnabled),
    (&::gpu::gl::GLBackend::do_setProjectionJitterSequence),
    (&::gpu::gl::GLBackend::do_setProjectionJitterScale),
    (&::gpu::gl::GLBackend::do_setViewportTransform),
    (&::gpu::gl::GLBackend::do_setDepthRangeTransform),

    (&::gpu::gl::GLBackend::do_saveViewProjectionTransform),
    (&::gpu::gl::GLBackend::do_setSavedViewProjectionTransform),
    (&::gpu::gl::GLBackend::do_copySavedViewProjectionTransformToBuffer),

    (&::gpu::gl::GLBackend::do_setPipeline),
    (&::gpu::gl::GLBackend::do_setStateBlendFactor),
    (&::gpu::gl::GLBackend::do_setStateScissorRect),

    (&::gpu::gl::GLBackend::do_setUniformBuffer),
    (&::gpu::gl::GLBackend::do_setResourceBuffer),
    (&::gpu::gl::GLBackend::do_setResourceTexture),
    (&::gpu::gl::GLBackend::do_setResourceTextureTable),
    (&::gpu::gl::GLBackend::do_setResourceFramebufferSwapChainTexture),

    (&::gpu::gl::GLBackend::do_setFramebuffer),
    (&::gpu::gl::GLBackend::do_setFramebufferSwapChain),
    (&::gpu::gl::GLBackend::do_clearFramebuffer),
    (&::gpu::gl::GLBackend::do_blit),
    (&::gpu::gl::GLBackend::do_generateTextureMips),
    (&::gpu::gl::GLBackend::do_generateTextureMipsWithPipeline),

    (&::gpu::gl::GLBackend::do_advance),

    (&::gpu::gl::GLBackend::do_beginQuery),
    (&::gpu::gl::GLBackend::do_endQuery),
    (&::gpu::gl::GLBackend::do_getQuery),

    (&::gpu::gl::GLBackend::do_resetStages),

    (&::gpu::gl::GLBackend::do_disableContextViewCorrection),
    (&::gpu::gl::GLBackend::do_restoreContextViewCorrection),
    (&::gpu::gl::GLBackend::do_disableContextStereo),
    (&::gpu::gl::GLBackend::do_restoreContextStereo),

    (&::gpu::gl::GLBackend::do_runLambda),

    (&::gpu::gl::GLBackend::do_startNamedCall),
    (&::gpu::gl::GLBackend::do_stopNamedCall),

    (&::gpu::gl::GLBackend::do_glUniform1i),
    (&::gpu::gl::GLBackend::do_glUniform1f),
    (&::gpu::gl::GLBackend::do_glUniform2f),
    (&::gpu::gl::GLBackend::do_glUniform3f),
    (&::gpu::gl::GLBackend::do_glUniform4f),
    (&::gpu::gl::GLBackend::do_glUniform3fv),
    (&::gpu::gl::GLBackend::do_glUniform4fv),
    (&::gpu::gl::GLBackend::do_glUniform4iv),
    (&::gpu::gl::GLBackend::do_glUniformMatrix3fv),
    (&::gpu::gl::GLBackend::do_glUniformMatrix4fv),

    (&::gpu::gl::GLBackend::do_glColor4f),

    (&::gpu::gl::GLBackend::do_pushProfileRange),
    (&::gpu::gl::GLBackend::do_popProfileRange),
};

#define GL_GET_INTEGER(NAME) glGetIntegerv(GL_##NAME, &const_cast<GLint&>(NAME)); 

#define BYTES_PER_KIB 1024L
#define BYTES_PER_MIB (1024L * BYTES_PER_KIB)

GLint GLBackend::MAX_TEXTURE_IMAGE_UNITS{ 0 };
GLint GLBackend::MAX_UNIFORM_BUFFER_BINDINGS{ 0 };
GLint GLBackend::MAX_COMBINED_UNIFORM_BLOCKS{ 0 };
GLint GLBackend::MAX_COMBINED_TEXTURE_IMAGE_UNITS{ 0 };
GLint GLBackend::MAX_UNIFORM_BLOCK_SIZE{ 0 };
GLint GLBackend::UNIFORM_BUFFER_OFFSET_ALIGNMENT{ 1 };
GLint GLBackend::GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX{ 0 };
GLint GLBackend::GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX{ 0 };
GLint GLBackend::GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX{ 0 };
GLint GLBackend::TEXTURE_FREE_MEMORY_ATI{ 0 };

size_t GLBackend::_totalMemory{ 0 };
size_t GLBackend::_dedicatedMemory{ 0 };
GLBackend::VideoCardType GLBackend::_videoCard{ GLBackend::Unknown };


#define GLX_RENDERER_VIDEO_MEMORY_MESA 0x8187

void GLBackend::init() {
    static std::once_flag once;
    std::call_once(once, [] {

        ::gl::ContextInfo contextInfo;
        contextInfo.init();
        QString vendor { contextInfo.vendor.c_str() };
        QString renderer { contextInfo.renderer.c_str() };

        // Textures
        GL_GET_INTEGER(MAX_TEXTURE_IMAGE_UNITS);
        GL_GET_INTEGER(MAX_COMBINED_TEXTURE_IMAGE_UNITS);

        // Uniform blocks
        GL_GET_INTEGER(MAX_UNIFORM_BUFFER_BINDINGS);
        GL_GET_INTEGER(MAX_COMBINED_UNIFORM_BLOCKS);
        GL_GET_INTEGER(MAX_UNIFORM_BLOCK_SIZE);
        GL_GET_INTEGER(UNIFORM_BUFFER_OFFSET_ALIGNMENT);

        GPUIdent* gpu = GPUIdent::getInstance(vendor, renderer);
        unsigned int mem;

        if (vendor.contains("NVIDIA") ) {
            qCDebug(gpugllogging) << "NVIDIA card detected";
#if !defined(Q_OS_ANDROID) && !defined(USE_GLES)
            GL_GET_INTEGER(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX);
            GL_GET_INTEGER(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX);
            GL_GET_INTEGER(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX);
#endif

            qCDebug(gpugllogging) << "GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX: " << GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX;
            qCDebug(gpugllogging) << "GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX: " << GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX;
            qCDebug(gpugllogging) << "GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX: " << GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX;

            _totalMemory = GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX * BYTES_PER_KIB;
            _dedicatedMemory = GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX * BYTES_PER_KIB;
            _videoCard = NVIDIA;


        } else if (vendor.contains("ATI")) {
            qCDebug(gpugllogging) << "ATI card detected";
#if !defined(Q_OS_ANDROID) && !defined(USE_GLES)
            GL_GET_INTEGER(TEXTURE_FREE_MEMORY_ATI);
#endif

            _totalMemory = TEXTURE_FREE_MEMORY_ATI * BYTES_PER_KIB;
            _dedicatedMemory = _totalMemory;
            _videoCard = ATI;
        } else if ( ::gl::queryCurrentRendererIntegerMESA(GLX_RENDERER_VIDEO_MEMORY_MESA, &mem) ) {
                // This works only on Linux. queryCurrentRendererIntegerMESA will return false if the
                // function is not supported because we're not on Linux, or for any other reason.
                qCDebug(gpugllogging) << "MESA card detected";
                _totalMemory = mem * BYTES_PER_MIB;
                _dedicatedMemory = _totalMemory;
                _videoCard = MESA;
        } else {
            qCCritical(gpugllogging) << "Don't know how to get memory for OpenGL vendor " << vendor << "; renderer " << renderer << ", trying fallback";
            _videoCard = Unknown;
            _dedicatedMemory = gpu->getMemory();
            _totalMemory = _dedicatedMemory;
        }

        qCDebug(gpugllogging) << "dedicated: " << _dedicatedMemory;
        qCDebug(gpugllogging) << "total: " << _totalMemory;


        LOG_GL_CONTEXT_INFO(gpugllogging, contextInfo);


        // From here on, GPUIdent::getInstance()->getMumble() should efficiently give the same answers.
        qCDebug(gpugllogging) << "GPU:";
        qCDebug(gpugllogging) << "\tcard:" << gpu->getName();
        qCDebug(gpugllogging) << "\tdriver:" << gpu->getDriver();
        qCDebug(gpugllogging) << "\ttotal memory:" << (_totalMemory / BYTES_PER_KIB) << "KB";
        qCDebug(gpugllogging) << "\tdedicated memory:" << (_dedicatedMemory / BYTES_PER_KIB) << "KB";
        qCDebug(gpugllogging) << "\tavailable memory:" << (getAvailableMemory()  / BYTES_PER_KIB) << "KB";
        qCDebug(gpugllogging) << "Limits:";
        qCDebug(gpugllogging) << "\tmax textures:" << MAX_TEXTURE_IMAGE_UNITS;
        qCDebug(gpugllogging) << "\tmax texture binding:" << MAX_COMBINED_TEXTURE_IMAGE_UNITS;
        qCDebug(gpugllogging) << "\tmax uniforms:" << MAX_UNIFORM_BUFFER_BINDINGS;
        qCDebug(gpugllogging) << "\tmax uniform binding:" << MAX_COMBINED_UNIFORM_BLOCKS;
        qCDebug(gpugllogging) << "\tmax uniform size:" << MAX_UNIFORM_BLOCK_SIZE;
        qCDebug(gpugllogging) << "\tuniform alignment:" << UNIFORM_BUFFER_OFFSET_ALIGNMENT;
#if !defined(USE_GLES)
        qCDebug(gpugllogging, "V-Sync is %s\n", (::gl::getSwapInterval() > 0 ? "ON" : "OFF"));
#endif
    });
}

size_t GLBackend::getAvailableMemory() {
    // GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX returns 1 value
    // GL_TEXTURE_FREE_MEMORY_ATI returns 4 values, we only need the first
    GLint mem[4] = {0,0,0,0};

    switch( _videoCard ) {
        case NVIDIA:
#if !defined(Q_OS_ANDROID) && !defined(USE_GLES)
            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &mem[0]);
#endif
            return mem[0] * BYTES_PER_KIB;
        case ATI:
#if !defined(Q_OS_ANDROID) && !defined(USE_GLES)
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &mem[0]);
#endif
            return mem[0] * BYTES_PER_KIB;
        case MESA:
            return 0; // Don't know the current value
        case Unknown:
            break;
    }

    return 0;

}

bool GLBackend::availableMemoryKnown() {
    switch( _videoCard ) {
        case NVIDIA:
            return true;
        case ATI:
            return true;
        case MESA:
            return false;
        case Unknown:
            return false;
    }

    return false;
}

GLBackend::GLBackend(bool syncCache) {
    initShaderBinaryCache();
}

GLBackend::GLBackend() {
    initShaderBinaryCache();
}

GLBackend::~GLBackend() {}

void GLBackend::shutdown() {
    if (_mipGenerationFramebufferId) {
        glDeleteFramebuffers(1, &_mipGenerationFramebufferId);
        _mipGenerationFramebufferId = 0;
    }
    killInput();
    killTransform();
    killTextureManagementStage();
    killShaderBinaryCache();
}

void GLBackend::renderPassTransfer(const Batch& batch) {
    const size_t numCommands = batch.getCommands().size();
    const Batch::Commands::value_type* command = batch.getCommands().data();
    const Batch::CommandOffsets::value_type* offset = batch.getCommandOffsets().data();

    _inRenderTransferPass = true;
    { // Sync all the buffers
        PROFILE_RANGE(render_gpu_gl_detail, "syncGPUBuffer");

        for (auto& cached : batch._buffers._items) {
            if (cached._data) {
                syncGPUObject(*cached._data);
            }
        }
    }

    { // Sync all the transform states
        PROFILE_RANGE(render_gpu_gl_detail, "syncCPUTransform");
        _transform._cameras.clear();
        _transform._cameraOffsets.clear();

        for (_commandIndex = 0; _commandIndex < numCommands; ++_commandIndex) {
            switch (*command) {
                case Batch::COMMAND_draw:
                case Batch::COMMAND_drawIndexed:
                case Batch::COMMAND_drawInstanced:
                case Batch::COMMAND_drawIndexedInstanced:
                case Batch::COMMAND_multiDrawIndirect:
                case Batch::COMMAND_multiDrawIndexedIndirect:
                    preUpdateTransform();
                    break;

                case Batch::COMMAND_disableContextStereo:
                    _stereo._contextDisable = true;
                    break;

                case Batch::COMMAND_restoreContextStereo:
                    _stereo._contextDisable = false;
                    break;

                case Batch::COMMAND_copySavedViewProjectionTransformToBuffer:
                    // We need to store this transform state in the transform buffer
                    preUpdateTransform();
                    break;

                case Batch::COMMAND_setFramebuffer:
                case Batch::COMMAND_setViewportTransform:
                case Batch::COMMAND_setViewTransform:
                case Batch::COMMAND_setProjectionTransform:
                case Batch::COMMAND_setProjectionJitterEnabled:
                case Batch::COMMAND_setProjectionJitterSequence:
                case Batch::COMMAND_setProjectionJitterScale:
                case Batch::COMMAND_saveViewProjectionTransform:
                case Batch::COMMAND_setSavedViewProjectionTransform:
                {
                    CommandCall call = _commandCalls[(*command)];
                    (this->*(call))(batch, *offset);
                    break;
                }

                default:
                    break;
            }
            command++;
            offset++;
        }
    }

    { // Sync the transform buffers
        PROFILE_RANGE(render_gpu_gl_detail, "syncGPUTransform");
        transferTransformState(batch);
    }

    _inRenderTransferPass = false;
}

void GLBackend::renderPassDraw(const Batch& batch) {
    _currentDraw = -1;
    _transform._camerasItr = _transform._cameraOffsets.begin();
    const size_t numCommands = batch.getCommands().size();
    const Batch::Commands::value_type* command = batch.getCommands().data();
    const Batch::CommandOffsets::value_type* offset = batch.getCommandOffsets().data();
    for (_commandIndex = 0; _commandIndex < numCommands; ++_commandIndex) {
        switch (*command) {
            // Ignore these commands on this pass, taken care of in the transfer pass
            // Note we allow COMMAND_setViewportTransform to occur in both passes
            // as it both updates the transform object (and thus the uniforms in the 
            // UBO) as well as executes the actual viewport call
            case Batch::COMMAND_setModelTransform:
            case Batch::COMMAND_setViewTransform:
            case Batch::COMMAND_setProjectionTransform:
            case Batch::COMMAND_saveViewProjectionTransform:
            case Batch::COMMAND_setSavedViewProjectionTransform:
            case Batch::COMMAND_setProjectionJitterSequence:
                break;

            case Batch::COMMAND_draw:
            case Batch::COMMAND_drawIndexed:
            case Batch::COMMAND_drawInstanced:
            case Batch::COMMAND_drawIndexedInstanced:
            case Batch::COMMAND_multiDrawIndirect:
            case Batch::COMMAND_multiDrawIndexedIndirect: {
#ifdef GPU_BATCH_DETAILED_TRACING
                PROFILE_RANGE(render_gpu_gl_detail, "drawcall");
#endif 
                // updates for draw calls
                ++_currentDraw;
                updateInput();
                updateTransform(batch);
                updatePipeline();

                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
#ifdef GPU_BATCH_DETAILED_TRACING
            //case Batch::COMMAND_setModelTransform:
            //case Batch::COMMAND_setViewTransform:
            //case Batch::COMMAND_setProjectionTransform:
            case Batch::COMMAND_setViewportTransform:
            case Batch::COMMAND_setDepthRangeTransform:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "transform");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_clearFramebuffer:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "clear");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_blit:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "blit");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_setInputFormat:
            case Batch::COMMAND_setInputBuffer:
            case Batch::COMMAND_setIndexBuffer:
            case Batch::COMMAND_setIndirectBuffer: {
                PROFILE_RANGE(render_gpu_gl_detail, "input");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_setStateBlendFactor:
            case Batch::COMMAND_setStateScissorRect:
            case Batch::COMMAND_setPipeline: {
                PROFILE_RANGE(render_gpu_gl_detail, "pipeline");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_setUniformBuffer:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "ubo");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_setResourceBuffer:
            case Batch::COMMAND_setResourceTexture:
            case Batch::COMMAND_setResourceTextureTable:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "resource");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }

            case Batch::COMMAND_setResourceFramebufferSwapChainTexture:
            case Batch::COMMAND_setFramebuffer:
            case Batch::COMMAND_setFramebufferSwapChain:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "framebuffer");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_generateTextureMips:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "genMipMaps");

                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
            case Batch::COMMAND_beginQuery:
            case Batch::COMMAND_endQuery:
            case Batch::COMMAND_getQuery:
            {
                PROFILE_RANGE(render_gpu_gl_detail, "query");
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
#endif 
            default: {
                CommandCall call = _commandCalls[(*command)];
                (this->*(call))(batch, *offset);
                break;
            }
        }

        command++;
        offset++;
    }
}

// Support annotating captures in tools like Renderdoc
class GlDuration {
public:
#ifdef USE_GLES
    GlDuration(const char* name) {
        // We need to use strlen here instead of -1, because the Snapdragon profiler
        // will crash otherwise 
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, strlen(name), name);
    }
    ~GlDuration() {
        glPopDebugGroup();
    }
#else
    GlDuration(const char* name) {
        if (::gl::khrDebugEnabled()) {
            glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, 0, -1, name);
        }
    }
    ~GlDuration() {
        if (::gl::khrDebugEnabled()) {
            glPopDebugGroupKHR();
        }
    }
#endif
};

#if defined(GPU_STEREO_DRAWCALL_INSTANCED) && !defined(GL_CLIP_DISTANCE0)
#define GL_CLIP_DISTANCE0 GL_CLIP_DISTANCE0_EXT
#endif

#define GL_PROFILE_RANGE(category, name) \
    PROFILE_RANGE(category, name); \
    GlDuration glProfileRangeThis(name);

void GLBackend::render(const Batch& batch) {
    GL_PROFILE_RANGE(render_gpu_gl, batch.getName().c_str());

    _transform._skybox = _stereo._skybox = batch.isSkyboxEnabled();
    // FIXME move this to between the transfer and draw passes, so that
    // framebuffer setup can see the proper stereo state and enable things 
    // like foveation
    // Allow the batch to override the rendering stereo settings
    // for things like full framebuffer copy operations (deferred lighting passes)
    bool savedStereo = _stereo._enable;
    if (!batch.isStereoEnabled()) {
        _stereo._enable = false;
    }
    // Reset jitter
    _transform._projectionJitter._isEnabled = false;
    
    {
        GL_PROFILE_RANGE(render_gpu_gl_detail, "Transfer");
        renderPassTransfer(batch);
    }

#ifdef GPU_STEREO_DRAWCALL_INSTANCED
    if (_stereo.isStereo()) {
        glEnable(GL_CLIP_DISTANCE0);
    }
#endif
    {
        GL_PROFILE_RANGE(render_gpu_gl_detail, _stereo.isStereo() ? "Render Stereo" : "Render");
        renderPassDraw(batch);
    }
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
    if (_stereo.isStereo()) {
        glDisable(GL_CLIP_DISTANCE0);
    }
#endif

    // Restore the saved stereo state for the next batch
    _stereo._enable = savedStereo;

    if (batch._mustUpdatePreviousModels) {
        // Update object transform history for when the batch will be reexecuted
        for (auto& objectTransform : batch._objects) {
            objectTransform._previousModel = objectTransform._model;
        }
        batch._mustUpdatePreviousModels = false;
    }
}


void GLBackend::syncCache() {
    PROFILE_RANGE(render_gpu_gl_detail, __FUNCTION__);

    syncTransformStateCache();
    syncPipelineStateCache();
    syncInputStateCache();
    syncOutputStateCache();
}

#ifdef GPU_STEREO_DRAWCALL_DOUBLED
void GLBackend::setupStereoSide(int side) {
    ivec4 vp = _transform._viewport;
    vp.z /= 2;
    glViewport(vp.x + side * vp.z, vp.y, vp.z, vp.w);

#ifdef GPU_STEREO_CAMERA_BUFFER
#ifdef GPU_STEREO_DRAWCALL_DOUBLED
    glVertexAttribI1i(14, side);
#endif
#else
    _transform.bindCurrentCamera(side);
#endif
}
#else
#endif

void GLBackend::do_resetStages(const Batch& batch, size_t paramOffset) {
    resetStages();
}

void GLBackend::do_disableContextViewCorrection(const Batch& batch, size_t paramOffset) {
    _transform._viewCorrectionEnabled = false;
}

void GLBackend::do_restoreContextViewCorrection(const Batch& batch, size_t paramOffset) {
    _transform._viewCorrectionEnabled = true;
}

void GLBackend::do_disableContextStereo(const Batch& batch, size_t paramOffset) {

}

void GLBackend::do_restoreContextStereo(const Batch& batch, size_t paramOffset) {

}

void GLBackend::do_runLambda(const Batch& batch, size_t paramOffset) {
    std::function<void()> f = batch._lambdas.get(batch._params[paramOffset]._uint);
    f();
}

void GLBackend::do_startNamedCall(const Batch& batch, size_t paramOffset) {
    batch._currentNamedCall = batch._names.get(batch._params[paramOffset]._uint);
    _currentDraw = -1;
}

void GLBackend::do_stopNamedCall(const Batch& batch, size_t paramOffset) {
    batch._currentNamedCall.clear();
}

void GLBackend::resetStages() {
    resetInputStage();
    resetPipelineStage();
    resetTransformStage();
    resetUniformStage();
    resetResourceStage();
    resetOutputStage();
    resetQueryStage();

    (void) CHECK_GL_ERROR();
}


void GLBackend::do_pushProfileRange(const Batch& batch, size_t paramOffset) {
    if (trace_render_gpu_gl_detail().isDebugEnabled()) {
        auto name = batch._profileRanges.get(batch._params[paramOffset]._uint);
        profileRanges.push_back(name);
#if defined(NSIGHT_FOUND)
        nvtxRangePush(name.c_str());
#endif
    }
}

void GLBackend::do_popProfileRange(const Batch& batch, size_t paramOffset) {
    if (trace_render_gpu_gl_detail().isDebugEnabled()) {
        profileRanges.pop_back();
#if defined(NSIGHT_FOUND)
        nvtxRangePop();
#endif
    }
}


// TODO: As long as we have gl calls explicitely issued from interface
// code, we need to be able to record and batch these calls. THe long 
// term strategy is to get rid of any GL calls in favor of the HIFI GPU API

void GLBackend::do_glUniform1i(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();

    GLint location = getRealUniformLocation(batch._params[paramOffset + 1]._int);
    glUniform1i(
        location,
        batch._params[paramOffset + 0]._int);
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform1f(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();

    GLint location = getRealUniformLocation(batch._params[paramOffset + 1]._int);
    glUniform1f(
        location,
        batch._params[paramOffset + 0]._float);
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform2f(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();
    GLint location = getRealUniformLocation(batch._params[paramOffset + 2]._int);
    glUniform2f(
        location,
        batch._params[paramOffset + 1]._float,
        batch._params[paramOffset + 0]._float);
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform3f(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();
    GLint location = getRealUniformLocation(batch._params[paramOffset + 3]._int);
    glUniform3f(
        location,
        batch._params[paramOffset + 2]._float,
        batch._params[paramOffset + 1]._float,
        batch._params[paramOffset + 0]._float);
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform4f(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();
    GLint location = getRealUniformLocation(batch._params[paramOffset + 4]._int);
    glUniform4f(
        location,
        batch._params[paramOffset + 3]._float,
        batch._params[paramOffset + 2]._float,
        batch._params[paramOffset + 1]._float,
        batch._params[paramOffset + 0]._float);
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform3fv(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();
    GLint location = getRealUniformLocation(batch._params[paramOffset + 2]._int);
    glUniform3fv(
        location,
        batch._params[paramOffset + 1]._uint,
        (const GLfloat*)batch.readData(batch._params[paramOffset + 0]._uint));

    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform4fv(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();

    GLint location = getRealUniformLocation(batch._params[paramOffset + 2]._int);
    GLsizei count = batch._params[paramOffset + 1]._uint;
    const GLfloat* value = (const GLfloat*)batch.readData(batch._params[paramOffset + 0]._uint);
    glUniform4fv(location, count, value);

    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniform4iv(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();
    GLint location = getRealUniformLocation(batch._params[paramOffset + 2]._int);
    glUniform4iv(
        location,
        batch._params[paramOffset + 1]._uint,
        (const GLint*)batch.readData(batch._params[paramOffset + 0]._uint));

    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniformMatrix3fv(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();

    GLint location = getRealUniformLocation(batch._params[paramOffset + 3]._int);
    glUniformMatrix3fv(
        location,
        batch._params[paramOffset + 2]._uint,
        batch._params[paramOffset + 1]._uint,
        (const GLfloat*)batch.readData(batch._params[paramOffset + 0]._uint));
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glUniformMatrix4fv(const Batch& batch, size_t paramOffset) {
    if (_pipeline._program == 0) {
        // We should call updatePipeline() to bind the program but we are not doing that
        // because these uniform setters are deprecated and we don;t want to create side effect
        return;
    }
    updatePipeline();

    GLint location = getRealUniformLocation(batch._params[paramOffset + 3]._int);
    glUniformMatrix4fv(
        location,
        batch._params[paramOffset + 2]._uint,
        batch._params[paramOffset + 1]._uint,
        (const GLfloat*)batch.readData(batch._params[paramOffset + 0]._uint));
    (void)CHECK_GL_ERROR();
}

void GLBackend::do_glColor4f(const Batch& batch, size_t paramOffset) {

    glm::vec4 newColor(
        batch._params[paramOffset + 3]._float,
        batch._params[paramOffset + 2]._float,
        batch._params[paramOffset + 1]._float,
        batch._params[paramOffset + 0]._float);

    if (_input._colorAttribute != newColor) {
        _input._colorAttribute = newColor;
        glVertexAttrib4fv(gpu::Stream::COLOR, &_input._colorAttribute.r);
        // Color has been changed and is not white. To prevent colors from bleeding
        // between different objects, we need to set the _hadColorAttribute flag
        // as if a previous render call had potential colors
        _input._hadColorAttribute = (newColor != glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    (void)CHECK_GL_ERROR();
}

void GLBackend::releaseBuffer(GLuint id, Size size) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.buffersTrash.push_back({ id, size });
}

void GLBackend::releaseExternalTexture(GLuint id, const Texture::ExternalRecycler& recycler) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.externalTexturesTrash.push_back({ id, recycler });
}

void GLBackend::releaseTexture(GLuint id, Size size) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.texturesTrash.push_back({ id, size });
}

void GLBackend::releaseFramebuffer(GLuint id) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.framebuffersTrash.push_back(id);
}

void GLBackend::releaseShader(GLuint id) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.shadersTrash.push_back(id);
}

void GLBackend::releaseProgram(GLuint id) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.programsTrash.push_back(id);
}

void GLBackend::releaseQuery(GLuint id) const {
    Lock lock(_trashMutex);
    _currentFrameTrash.queriesTrash.push_back(id);
}

void GLBackend::queueLambda(const std::function<void()> lambda) const {
    Lock lock(_trashMutex);
    _lambdaQueue.push_back(lambda);
}

void GLBackend::FrameTrash::cleanup() {
    glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
    glDeleteSync(fence);

    {
        std::vector<GLuint> ids;
        ids.reserve(buffersTrash.size());
        for (auto pair : buffersTrash) {
            ids.push_back(pair.first);
        }
        if (!ids.empty()) {
            glDeleteBuffers((GLsizei)ids.size(), ids.data());
        }
    }

    {
        std::vector<GLuint> ids;
        ids.reserve(framebuffersTrash.size());
        for (auto id : framebuffersTrash) {
            ids.push_back(id);
        }
        if (!ids.empty()) {
            glDeleteFramebuffers((GLsizei)ids.size(), ids.data());
        }
    }

    {
        std::vector<GLuint> ids;
        ids.reserve(texturesTrash.size());
        for (auto pair : texturesTrash) {
            ids.push_back(pair.first);
        }
        if (!ids.empty()) {
            glDeleteTextures((GLsizei)ids.size(), ids.data());
        }
    }

    {
        if (!externalTexturesTrash.empty()) {
            std::vector<GLsync> fences;
            fences.resize(externalTexturesTrash.size());
            for (size_t i = 0; i < externalTexturesTrash.size(); ++i) {
                fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            }
            // External texture fences will be read in another thread/context, so we need a flush
            glFlush();
            size_t index = 0;
            for (auto pair : externalTexturesTrash) {
                auto fence = fences[index++];
                pair.second(pair.first, fence);
            }
        }
    }
    
    for (auto id : programsTrash) {
        glDeleteProgram(id);
    }

    for (auto id : shadersTrash) {
        glDeleteShader(id);
    }
    
    {
        std::vector<GLuint> ids;
        ids.reserve(queriesTrash.size());
        for (auto id : queriesTrash) {
            ids.push_back(id);
        }
        if (!ids.empty()) {
            glDeleteQueries((GLsizei)ids.size(), ids.data());
        }
    }
    
}

void GLBackend::recycle() const {
    PROFILE_RANGE(render_gpu_gl, __FUNCTION__)
    {
        std::list<std::function<void()>> lamdbasTrash;
        {
            Lock lock(_trashMutex);
            std::swap(_lambdaQueue, lamdbasTrash);
        }
        for (auto lambda : lamdbasTrash) {
            lambda();
        }
    }

    while (!_previousFrameTrashes.empty()) {
        _previousFrameTrashes.front().cleanup();
        _previousFrameTrashes.pop_front();
    }

    _previousFrameTrashes.emplace_back();
    {
        Lock lock(_trashMutex);
        _previousFrameTrashes.back().swap(_currentFrameTrash);
        _previousFrameTrashes.back().fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
    
    _textureManagement._transferEngine->manageMemory();
}

void GLBackend::updatePresentFrame(const Mat4& correction) {
    _transform._presentFrame.correction = correction;
    _transform._presentFrame.correctionInverse = glm::inverse(correction);

    _transform._projectionJitter._currentSampleIndex++;

    // Update previous views of saved transforms
    for (auto& viewProjState : _transform._savedTransforms) {
        viewProjState._state._previousCorrectedView = viewProjState._state._correctedView;
        viewProjState._state._previousProjection = viewProjState._state._projection;
    }
}

void GLBackend::syncProgram(const gpu::ShaderPointer& program) {
    gpu::gl::GLShader::sync(*this, *program);
}
