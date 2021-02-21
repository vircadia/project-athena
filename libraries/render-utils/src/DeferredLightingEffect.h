//
//  DeferredLightingEffect.h
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 9/11/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DeferredLightingEffect_h
#define hifi_DeferredLightingEffect_h

#include <QVector>

#include <DependencyManager.h>
#include <NumericalConstants.h>

#include "graphics/Light.h"
#include "graphics/Geometry.h"

#include <procedural/ProceduralSkybox.h>

#include <render/CullTask.h>

#include "DeferredFrameTransform.h"
#include "DeferredFramebuffer.h"
#include "LightingModel.h"

#include "LightStage.h"
#include "LightClusters.h"
#include "BackgroundStage.h"
#include "HazeStage.h"

#include "SurfaceGeometryPass.h"
#include "SubsurfaceScattering.h"
#include "AmbientOcclusionEffect.h"

// THis is where we currently accumulate the local lights, let s change that sooner than later
class DeferredLightingEffect : public Dependency {
    SINGLETON_DEPENDENCY
    
public:
    void init();
 
    static void setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch);
    static void setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch, const LightStage::Frame& lightFrame);
    static void unsetKeyLightBatch(gpu::Batch& batch);

    static void setupLocalLightsBatch(gpu::Batch& batch, const LightClustersPointer& lightClusters);
    static void unsetLocalLightsBatch(gpu::Batch& batch);

private:
    DeferredLightingEffect() = default;

    graphics::MeshPointer _pointLightMesh;
    graphics::MeshPointer getPointLightMesh();
    graphics::MeshPointer _spotLightMesh;
    graphics::MeshPointer getSpotLightMesh();

    gpu::PipelinePointer _directionalSkyboxLight;
    gpu::PipelinePointer _directionalAmbientSphereLight;

    gpu::PipelinePointer _directionalSkyboxLightShadow;
    gpu::PipelinePointer _directionalAmbientSphereLightShadow;

    gpu::PipelinePointer _localLight;
    gpu::PipelinePointer _localLightOutline;

    friend class LightClusteringPass;
    friend class RenderDeferredSetup;
    friend class RenderDeferredLocals;
    friend class RenderDeferredCleanup;
};

class PrepareDeferred {
public:
    // Inputs: primaryFramebuffer and lightingModel
    using Inputs = render::VaryingSet2 <gpu::FramebufferPointer, LightingModelPointer>;
    using Outputs = render::VaryingSet3<DeferredFramebufferPointer, gpu::FramebufferPointer, gpu::FramebufferPointer>;

    using JobModel = render::Job::ModelIO<PrepareDeferred, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs);

    DeferredFramebufferPointer _deferredFramebuffer;
};

class RenderDeferredSetup {
public:

    void run(const render::RenderContextPointer& renderContext,
        const DeferredFrameTransformPointer& frameTransform,
        const DeferredFramebufferPointer& deferredFramebuffer,
        const LightingModelPointer& lightingModel,
        const LightStage::FramePointer& lightFrame,
        const LightStage::ShadowFramePointer& shadowFrame,
        const HazeStage::FramePointer& hazeFrame,
        const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer,
        const AmbientOcclusionFramebufferPointer& ambientOcclusionFramebuffer,
        const SubsurfaceScatteringResourcePointer& subsurfaceScatteringResource);
};

class RenderDeferredLocals {
public:
    using JobModel = render::Job::ModelI<RenderDeferredLocals, DeferredFrameTransformPointer>;
    
    void run(const render::RenderContextPointer& renderContext,
        const DeferredFrameTransformPointer& frameTransform,
        const DeferredFramebufferPointer& deferredFramebuffer,
        const LightingModelPointer& lightingModel,
        const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer,
        const LightClustersPointer& lightClusters);

    gpu::BufferView _localLightsBuffer;

    RenderDeferredLocals();
};


class RenderDeferredCleanup {
public:
    using JobModel = render::Job::Model<RenderDeferredCleanup>;
    
    void run(const render::RenderContextPointer& renderContext);
};

using RenderDeferredConfig = render::GPUJobConfig;

class RenderDeferred {
public:
    using ExtraDeferredBuffer = render::VaryingSet3<SurfaceGeometryFramebufferPointer, AmbientOcclusionFramebufferPointer, SubsurfaceScatteringResourcePointer>;
    using Inputs = render::VaryingSet8<
        DeferredFrameTransformPointer, DeferredFramebufferPointer, ExtraDeferredBuffer, LightingModelPointer, LightClustersPointer, LightStage::FramePointer, LightStage::ShadowFramePointer, HazeStage::FramePointer>;

    using Config = RenderDeferredConfig;
    using JobModel = render::Job::ModelI<RenderDeferred, Inputs, Config>;

    RenderDeferred();

    void configure(const Config& config);

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);
    
    RenderDeferredSetup setupJob;
    RenderDeferredLocals lightsJob;
    RenderDeferredCleanup cleanupJob;

protected:
    gpu::RangeTimerPointer _gpuTimer;

private:
};

class DefaultLightingSetup {
public:
    using JobModel = render::Job::Model<DefaultLightingSetup>;

    void run(const render::RenderContextPointer& renderContext);

protected:
    graphics::LightPointer _defaultLight;
    LightStage::Index _defaultLightID{ LightStage::INVALID_INDEX };
    graphics::SunSkyStagePointer _defaultBackground;
    BackgroundStage::Index _defaultBackgroundID{ BackgroundStage::INVALID_INDEX };
    graphics::HazePointer _defaultHaze{ nullptr };
    HazeStage::Index _defaultHazeID{ HazeStage::INVALID_INDEX };
    graphics::SkyboxPointer _defaultSkybox { new ProceduralSkybox() };
    NetworkTexturePointer _defaultSkyboxNetworkTexture;
    NetworkTexturePointer _defaultAmbientNetworkTexture;
    gpu::TexturePointer _defaultAmbientTexture;
};

#endif // hifi_DeferredLightingEffect_h
