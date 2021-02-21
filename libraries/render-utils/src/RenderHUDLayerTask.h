//
//  Created by Sam Gateau on 2019/06/14
//  Copyright 2013-2019 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderHUDLayerTask_h
#define hifi_RenderHUDLayerTask_h

#include "LightingModel.h"
#include "HazeStage.h"
#include "DeferredFrameTransform.h"

class CompositeHUD {
public:
    // IF specified the input Framebuffer is actively set by the batch of this job before calling the HUDOperator.
    // If not, the current Framebuffer is left unchanged.
    //using Inputs = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelI<CompositeHUD, gpu::FramebufferPointer>;

    CompositeHUD(uint transformSlot) : _transformSlot(transformSlot) {}

    void run(const render::RenderContextPointer& renderContext, const gpu::FramebufferPointer& inputs);

private:
    uint _transformSlot;
};

class RenderHUDLayerTask {
public:
    // Framebuffer where to draw, lighting model, opaque items, transparent items
    using Input = render::VaryingSet6<gpu::FramebufferPointer, LightingModelPointer, render::ItemBounds, render::ItemBounds, HazeStage::FramePointer, DeferredFrameTransformPointer>;
    using JobModel = render::Task::ModelI<RenderHUDLayerTask, Input>;

    void build(JobModel& task, const render::Varying& input, render::Varying& output, render::ShapePlumberPointer shapePlumber, uint transformSlot);
};

#endif // hifi_RenderHUDLayerTask_h