//
//  Backend.h
//  interface/src/gpu
//
//  Created by Olivier Prat on 05/18/2018.
//  Copyright 2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_Backend_h
#define hifi_gpu_Backend_h

#include <GLMHelpers.h>

#include "Forward.h"
#include "Batch.h"
#include "Buffer.h"
#include "Framebuffer.h"

class QImage;

namespace gpu {
class Context;

struct ContextStats {
public:
    int _ISNumFormatChanges = 0;
    int _ISNumInputBufferChanges = 0;
    int _ISNumIndexBufferChanges = 0;

    int _RSNumResourceBufferBounded = 0;
    int _RSNumTextureBounded = 0;
    int _RSAmountTextureMemoryBounded = 0;

    int _DSNumAPIDrawcalls = 0;
    int _DSNumDrawcalls = 0;
    int _DSNumTriangles = 0;

    int _PSNumSetPipelines = 0;

    ContextStats() {}
    ContextStats(const ContextStats& stats) = default;

    void evalDelta(const ContextStats& begin, const ContextStats& end);
};

class Backend {
public:
    virtual ~Backend() {}

    virtual void shutdown() {}
    virtual const std::string& getVersion() const = 0;

    void setStereoState(const StereoState& stereo);

    virtual void render(const Batch& batch) = 0;
    virtual void syncCache() = 0;
    virtual void syncProgram(const gpu::ShaderPointer& program) = 0;
    virtual void recycle() const = 0;
    virtual void downloadFramebuffer(const FramebufferPointer& srcFramebuffer, const Vec4i& region, QImage& destImage) = 0;
    virtual void updatePresentFrame(const Mat4& correction = Mat4()) = 0;

    virtual bool supportedTextureFormat(const gpu::Element& format) = 0;

        // Shared header between C++ and GLSL
#include "TransformCamera_shared.slh"

    class TransformCamera : public _TransformCamera {
    public:
        const Backend::TransformCamera& recomputeDerived(const Transform& view, const Transform& previousView, const Mat4& previousProjection) const;
        // Jitter should be divided by framebuffer size
        TransformCamera getMonoCamera(bool isSkybox, const Transform& view, Transform previousView, Mat4 previousProjection, Vec2 normalizedJitter) const;
        // Jitter should be divided by framebuffer size
        TransformCamera getEyeCamera(int eye, const StereoState& stereo, const StereoState& prevStereo, const Transform& view, const Transform& previousView,
            Vec2 normalizedJitter) const;
    };

    template <typename T, typename U>
    static void setGPUObject(const U& object, T* gpuObject) {
        object.gpuObject.setGPUObject(gpuObject);
    }
    template <typename T, typename U>
    static T* getGPUObject(const U& object) {
        return reinterpret_cast<T*>(object.gpuObject.getGPUObject());
    }

    void resetStats() const { _stats = ContextStats(); }
    void getStats(ContextStats& stats) const { stats = _stats; }

    virtual bool isTextureManagementSparseEnabled() const = 0;

    // These should only be accessed by Backend implementation to report the buffer and texture allocations,
    // they are NOT public objects
    static ContextMetricSize freeGPUMemSize;

    static ContextMetricCount bufferCount;
    static ContextMetricSize bufferGPUMemSize;

    static ContextMetricCount textureResidentCount;
    static ContextMetricCount textureFramebufferCount;
    static ContextMetricCount textureResourceCount;
    static ContextMetricCount textureExternalCount;

    static ContextMetricSize textureResidentGPUMemSize;
    static ContextMetricSize textureFramebufferGPUMemSize;
    static ContextMetricSize textureResourceGPUMemSize;
    static ContextMetricSize textureExternalGPUMemSize;

    static ContextMetricCount texturePendingGPUTransferCount;
    static ContextMetricSize texturePendingGPUTransferMemSize;
    static ContextMetricSize textureResourcePopulatedGPUMemSize;
    static ContextMetricSize textureResourceIdealGPUMemSize;

protected:
    virtual bool isStereo() const {
        return _stereo.isStereo();
    }

    void getStereoProjections(mat4* eyeProjections) const {
        for (int i = 0; i < 2; ++i) {
            eyeProjections[i] = _stereo._eyeProjections[i];
        }
    }

    void getStereoViews(mat4* eyeViews) const {
        for (int i = 0; i < 2; ++i) {
            eyeViews[i] = _stereo._eyeViews[i];
        }
    }

    friend class Context;
    mutable ContextStats _stats;
    StereoState _stereo;
    StereoState _prevStereo;
};

}

#endif
