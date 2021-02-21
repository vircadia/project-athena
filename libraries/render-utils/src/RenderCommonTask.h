//
//  Created by Bradley Austin Davis on 2018/01/09
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderCommonTask_h
#define hifi_RenderCommonTask_h

#include <gpu/Pipeline.h>
#include "LightStage.h"
#include "HazeStage.h"
#include "LightingModel.h"
#include "DeferredFrameTransform.h"

class BeginGPURangeTimer {
public:
    using JobModel = render::Job::ModelO<BeginGPURangeTimer, gpu::RangeTimerPointer>;

    BeginGPURangeTimer(const std::string& name) : _gpuTimer(std::make_shared<gpu::RangeTimer>(name)) {}

    void run(const render::RenderContextPointer& renderContext, gpu::RangeTimerPointer& timer);

protected:
    gpu::RangeTimerPointer _gpuTimer;
};

using GPURangeTimerConfig = render::GPUJobConfig;

class EndGPURangeTimer {
public:
    using Config = GPURangeTimerConfig;
    using JobModel = render::Job::ModelI<EndGPURangeTimer, gpu::RangeTimerPointer, Config>;

    EndGPURangeTimer() {}

    void configure(const Config& config) {}
    void run(const render::RenderContextPointer& renderContext, const gpu::RangeTimerPointer& timer);

protected:
};

class DrawLayered3DConfig : public render::Job::Config {
    Q_OBJECT
        Q_PROPERTY(int numDrawn READ getNumDrawn NOTIFY numDrawnChanged)
        Q_PROPERTY(int maxDrawn MEMBER maxDrawn NOTIFY dirty)
public:
    int getNumDrawn() { return numDrawn; }
    void setNumDrawn(int num) { numDrawn = num; emit numDrawnChanged(); }

    int maxDrawn{ -1 };

signals:
    void numDrawnChanged();
    void dirty();

protected:
    int numDrawn{ 0 };
};

class DrawLayered3D {
public:
    using Inputs = render::VaryingSet4<render::ItemBounds, DeferredFrameTransformPointer, LightingModelPointer, HazeStage::FramePointer>;
    using Config = DrawLayered3DConfig;
    using JobModel = render::Job::ModelI<DrawLayered3D, Inputs, Config>;

    DrawLayered3D(const render::ShapePlumberPointer& shapePlumber, bool opaque, bool jitter, unsigned int transformSlot);

    void configure(const Config& config) { _maxDrawn = config.maxDrawn; }
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    render::ShapePlumberPointer _shapePlumber;
    int _maxDrawn; // initialized by Config
    uint _transformSlot;
    bool _opaquePass { true };
    bool _isJitterEnabled { false };
};

class Blit {
public:
    using JobModel = render::Job::ModelI<Blit, gpu::FramebufferPointer>;

    void run(const render::RenderContextPointer& renderContext, const gpu::FramebufferPointer& srcFramebuffer);
};

class NewFramebuffer {
public:
    using Output = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelO<NewFramebuffer, Output>;

    NewFramebuffer(gpu::Element pixelFormat = gpu::Element::COLOR_SRGBA_32);

    void run(const render::RenderContextPointer& renderContext, Output& output);
protected:
    gpu::Element _pixelFormat;
private:
    gpu::FramebufferPointer _outputFramebuffer;
};

class NewOrDefaultFramebuffer {
public:
    using Input = glm::uvec2;
    using Output = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelIO<NewOrDefaultFramebuffer, Input, Output>;

    void run(const render::RenderContextPointer& renderContext, const Input& input, Output& output);
private:
    gpu::FramebufferPointer _outputFramebuffer;
};

class ResolveFramebuffer {
public:
    using Inputs = render::VaryingSet2<gpu::FramebufferPointer, gpu::FramebufferPointer>;
    using Outputs = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelIO<ResolveFramebuffer, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& source, Outputs& dest);
};

class ExtractFrustums {
public:

    enum Frustum {
        SHADOW_CASCADE0_FRUSTUM = 0,
        SHADOW_CASCADE1_FRUSTUM,
        SHADOW_CASCADE2_FRUSTUM,
        SHADOW_CASCADE3_FRUSTUM,

        SHADOW_CASCADE_FRUSTUM_COUNT,

        VIEW_FRUSTUM = SHADOW_CASCADE_FRUSTUM_COUNT,

        FRUSTUM_COUNT
    };

    using Inputs = LightStage::ShadowFramePointer;
    using Outputs = render::VaryingArray<ViewFrustumPointer, FRUSTUM_COUNT>;
    using JobModel = render::Job::ModelIO<ExtractFrustums, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& output);
};

class SetRenderMethod {
public:
    using JobModel = render::Job::Model<SetRenderMethod>;

    SetRenderMethod(render::Args::RenderMethod method) : _method(method) {}

    void run(const render::RenderContextPointer& renderContext) { renderContext->args->_renderMethod = _method; }

protected:
    render::Args::RenderMethod _method;
};

#endif // hifi_RenderDeferredTask_h
