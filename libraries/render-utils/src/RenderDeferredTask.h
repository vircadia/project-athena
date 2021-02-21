//
//  RenderDeferredTask.h
//  render-utils/src/
//
//  Created by Sam Gateau on 5/29/15.
//  Copyright 20154 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderDeferredTask_h
#define hifi_RenderDeferredTask_h

#include <gpu/Pipeline.h>
#include <render/RenderFetchCullSortTask.h>
#include "AssembleLightingStageTask.h"
#include "LightingModel.h"
#include "LightClusters.h"
#include "RenderShadowTask.h"

class RenderTransparentDeferredConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(int numDrawn READ getNumDrawn NOTIFY newStats)
    Q_PROPERTY(int maxDrawn MEMBER maxDrawn NOTIFY dirty)

public:
    int getNumDrawn() { return _numDrawn; }
    void setNumDrawn(int numDrawn) {
        _numDrawn = numDrawn;
        emit newStats();
    }

    int maxDrawn{ -1 };

signals:
    void newStats();
    void dirty();

protected:
    int _numDrawn{ 0 };
};

class RenderTransparentDeferred {
public:
    using Inputs = render::VaryingSet7<render::ItemBounds, HazeStage::FramePointer, LightStage::FramePointer, LightingModelPointer, LightClustersPointer, LightStage::ShadowFramePointer, DeferredFrameTransformPointer>;
    using Config = RenderTransparentDeferredConfig;
    using JobModel = render::Job::ModelI<RenderTransparentDeferred, Inputs, Config>;

    RenderTransparentDeferred(render::ShapePlumberPointer shapePlumber, uint transformSlot)
        : _shapePlumber(shapePlumber), _transformSlot(transformSlot) {}

    void configure(const Config& config) { _maxDrawn = config.maxDrawn; }
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    render::ShapePlumberPointer _shapePlumber;
    uint _transformSlot;
    int _maxDrawn;  // initialized by Config
};

class DrawStateSortConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(int numDrawn READ getNumDrawn NOTIFY numDrawnChanged)
    Q_PROPERTY(int maxDrawn MEMBER maxDrawn NOTIFY dirty)
    Q_PROPERTY(bool stateSort MEMBER stateSort NOTIFY dirty)
public:
    int getNumDrawn() { return numDrawn; }
    void setNumDrawn(int num) {
        numDrawn = num;
        emit numDrawnChanged();
    }

    int maxDrawn{ -1 };
    bool stateSort{ true };

signals:
    void numDrawnChanged();
    void dirty();

protected:
    int numDrawn{ 0 };
};

class DrawStateSortDeferred {
public:
    using Inputs = render::VaryingSet3<render::ItemBounds, LightingModelPointer, DeferredFrameTransformPointer>;

    using Config = DrawStateSortConfig;
    using JobModel = render::Job::ModelI<DrawStateSortDeferred, Inputs, Config>;

    DrawStateSortDeferred(render::ShapePlumberPointer shapePlumber, uint transformSlot)
        : _shapePlumber(shapePlumber), _transformSlot(transformSlot) {
    }

    void configure(const Config& config) {
        _maxDrawn = config.maxDrawn;
        _stateSort = config.stateSort;
    }
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    render::ShapePlumberPointer _shapePlumber;
    uint _transformSlot;
    int _maxDrawn;  // initialized by Config
    bool _stateSort;
};

class SetSeparateDeferredDepthBuffer {
public:
    using Inputs = DeferredFramebufferPointer;
    using JobModel = render::Job::ModelI<SetSeparateDeferredDepthBuffer, Inputs>;

    SetSeparateDeferredDepthBuffer() = default;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    gpu::FramebufferPointer _framebuffer;
};

class RenderDeferredTaskConfig : public render::Task::Config {
    Q_OBJECT
    Q_PROPERTY(float fadeScale MEMBER fadeScale NOTIFY dirty)
    Q_PROPERTY(float fadeDuration MEMBER fadeDuration NOTIFY dirty)
    Q_PROPERTY(float resolutionScale MEMBER resolutionScale NOTIFY dirty)
    Q_PROPERTY(bool debugFade MEMBER debugFade NOTIFY dirty)
    Q_PROPERTY(float debugFadePercent MEMBER debugFadePercent NOTIFY dirty)
public:
    float fadeScale{ 0.5f };
    float fadeDuration{ 3.0f };
    float resolutionScale{ 1.f };
    float debugFadePercent{ 0.f };
    bool debugFade{ false };

signals:
    void dirty();
};

class RenderDeferredTask {
public:
    using Input = render::VaryingSet4<RenderFetchCullSortTask::Output, LightingModelPointer, AssembleLightingStageTask::Output, RenderShadowTask::Output>;
    using Config = RenderDeferredTaskConfig;
    using JobModel = render::Task::ModelI<RenderDeferredTask, Input, Config>;

    void configure(const Config& config);
    void build(JobModel& task, const render::Varying& input, render::Varying& output, uint8_t transformOffset);
};


class PreparePrimaryFramebufferConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(float resolutionScale  WRITE setResolutionScale READ getResolutionScale)
public:
    float getResolutionScale() const { return resolutionScale; }
    void setResolutionScale(float scale) {
        const float SCALE_RANGE_MIN = 0.1f;
        const float SCALE_RANGE_MAX = 2.0f;
        resolutionScale = std::max(SCALE_RANGE_MIN, std::min(SCALE_RANGE_MAX, scale));
    }

signals:
    void dirty();

protected:
    float resolutionScale{ 1.0f };
};

class PreparePrimaryFramebuffer {
public:

    using Output = gpu::FramebufferPointer;
    using Config = PreparePrimaryFramebufferConfig;
    using JobModel = render::Job::ModelO<PreparePrimaryFramebuffer, Output, Config>;

    PreparePrimaryFramebuffer(float resolutionScale = 1.0f) : _resolutionScale{ resolutionScale } {}
    void configure(const Config& config);
    void run(const render::RenderContextPointer& renderContext, Output& primaryFramebuffer);

    gpu::FramebufferPointer _primaryFramebuffer;
    float _resolutionScale{ 1.0f };

private:

    static gpu::FramebufferPointer createFramebuffer(const char* name, const glm::uvec2& size);
};

#endif  // hifi_RenderDeferredTask_h
