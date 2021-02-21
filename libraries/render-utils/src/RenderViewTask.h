//
//  RenderViewTask.h
//  render-utils/src/
//
//  Created by Sam Gateau on 5/25/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_RenderViewTask_h
#define hifi_RenderViewTask_h

#include <render/Engine.h>
#include <render/RenderFetchCullSortTask.h>

#include "AssembleLightingStageTask.h"

class RenderShadowsAndDeferredTask {
public:
    using Input = render::VaryingSet3<RenderFetchCullSortTask::Output, LightingModelPointer, AssembleLightingStageTask::Output>;
    using JobModel = render::Task::ModelI<RenderShadowsAndDeferredTask, Input>;

    RenderShadowsAndDeferredTask() {}

    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor,
        uint8_t tagBits, uint8_t tagMask, uint8_t transformOffset);

};

class DeferredForwardSwitchJob {
public:
    using Input = render::VaryingSet3<RenderFetchCullSortTask::Output, LightingModelPointer, AssembleLightingStageTask::Output>;
    using JobModel = render::Switch::ModelI<DeferredForwardSwitchJob, Input>;

    DeferredForwardSwitchJob() {}

    void configure(const render::SwitchConfig& config) {}
    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor,
        uint8_t tagBits, uint8_t tagMask, uint8_t transformOffset);

};

class RenderViewTask {
public:
    using Input = RenderFetchCullSortTask::Output;
    using JobModel = render::Task::ModelI<RenderViewTask, Input>;

    RenderViewTask() {}

    enum TransformOffset: uint8_t {
        MAIN_VIEW = 0,
        SECONDARY_VIEW = 2 // each view uses 1 transform for the main view, and one for the background, so these need to be increments of 2
    };

    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor,
        uint8_t tagBits = 0x00, uint8_t tagMask = 0x00, TransformOffset transformOffset = TransformOffset::MAIN_VIEW);

};


#endif // hifi_RenderViewTask_h
