//
//  LightClusters.h
//
//  Created by Sam Gateau on 9/7/2016.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_utils_LightClusters_h
#define hifi_render_utils_LightClusters_h

#include <ViewFrustum.h>
#include <gpu/Buffer.h>
#include <render/Engine.h>
#include "LightStage.h"
#include "DeferredFrameTransform.h"
#include "LightingModel.h"
#include "SurfaceGeometryPass.h"

class FrustumGrid {
public:
    float frustumNear { 0.1f };
    float rangeNear { 0.1f };
    float rangeFar { 200.0f };
    float frustumFar { 10000.0f };

    glm::ivec3 dims { 1, 1, 1 };
    float spare;

    glm::mat4 eyeToGridProj;
    glm::mat4 worldToEyeMat;
    glm::mat4 eyeToWorldMat;

    FrustumGrid() = default;
    FrustumGrid(const FrustumGrid& source);

    void updateFrustum(const ViewFrustum& frustum) {
        frustumNear = frustum.getNearClip();
        frustumFar = frustum.getFarClip();
        
        eyeToGridProj = frustum.evalProjectionMatrixRange(rangeNear, rangeFar);

        Transform view;
        frustum.evalViewTransform(view);
        eyeToWorldMat = view.getMatrix();
        worldToEyeMat = view.getInverseMatrix();
    }

    // Copy paste of the slh functions
    using vec3 = glm::vec3;
    using ivec3 = glm::ivec3;
    using mat4 = glm::mat4;
#define frustumGrid (*this)
#include "LightClusterGrid_shared.slh"


    using Planes = std::vector < glm::vec4 >;
    void generateGridPlanes(Planes& xPlanes, Planes& yPlanes, Planes& zPlanes);
};



class LightClusters {
public:
    using LightID = LightStage::Index;

    static const glm::uvec4 MAX_GRID_DIMENSIONS;

    LightClusters();

    void setDimensions(glm::uvec3 gridDims, uint32_t listBudget = MAX_GRID_DIMENSIONS.w);
    void setRangeNearFar(float rangeNear, float rangeFar);

    uint32_t getNumClusters() const;

    void updateFrustum(const ViewFrustum& frustum);

    void updateLightStage(const LightStagePointer& lightStage);

    void updateLightFrame(const LightStage::FramePointer& lightFrame, bool points = true, bool spots = true);

    glm::ivec3  updateClusters();


    ViewFrustum _frustum;


    LightStagePointer _lightStage;

    

    gpu::StructBuffer<FrustumGrid> _frustumGridBuffer;

    FrustumGrid::Planes _gridPlanes[3];

    LightStage::LightIndices _visibleLightIndices;
    gpu::BufferView _lightIndicesBuffer;

    const uint32_t EMPTY_CLUSTER { 0x0000FFFF };
    const LightID INVALID_LIGHT { LightStage::INVALID_INDEX };

    using LightIndex = uint16_t;

    std::vector<uint32_t> _clusterGrid;
    std::vector<LightIndex> _clusterContent;
    gpu::BufferView _clusterGridBuffer;
    gpu::BufferView _clusterContentBuffer;
    uint32_t _clusterContentBudget { 0 };

    bool _clusterResourcesInvalid { true };
    void updateClusterResource();
};

using LightClustersPointer = std::shared_ptr<LightClusters>;



class LightClusteringPassConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(float rangeNear MEMBER rangeNear NOTIFY dirty)
    Q_PROPERTY(float rangeFar MEMBER rangeFar NOTIFY dirty)
 
    Q_PROPERTY(int dimX MEMBER dimX NOTIFY dirty)
    Q_PROPERTY(int dimY MEMBER dimY NOTIFY dirty)
    Q_PROPERTY(int dimZ MEMBER dimZ NOTIFY dirty)
    
    Q_PROPERTY(bool freeze MEMBER freeze NOTIFY dirty)

    Q_PROPERTY(int numClusteredLightReferences MEMBER numClusteredLightReferences NOTIFY dirty)
    Q_PROPERTY(int numInputLights MEMBER numInputLights NOTIFY dirty)
    Q_PROPERTY(int numClusteredLights MEMBER numClusteredLights NOTIFY dirty)

    Q_PROPERTY(int numSceneLights MEMBER numSceneLights NOTIFY dirty)
    Q_PROPERTY(int numFreeSceneLights MEMBER numFreeSceneLights NOTIFY dirty)
    Q_PROPERTY(int numAllocatedSceneLights MEMBER numAllocatedSceneLights NOTIFY dirty)

public:
    LightClusteringPassConfig() : render::Job::Config(true){}
    float rangeNear{ 0.1f };
    float rangeFar{ 200.0f };

    int dimX { 14 };
    int dimY { 14 };
    int dimZ { 14 };


    bool freeze{ false };

    int numClusteredLightReferences { 0 };
    int numInputLights { 0 };
    int numClusteredLights { 0 };

    void setNumClusteredLightReferences(int numRefs) { numClusteredLightReferences = numRefs; }
    void setNumInputLights(int numLights) { numInputLights = numLights; }
    void setNumClusteredLights(int numLights) { numClusteredLights = numLights; }

    int numSceneLights { 0 };
    int numFreeSceneLights { 0 };
    int numAllocatedSceneLights { 0 };
signals:
    void dirty();
    
protected:
};

class LightClusteringPass {
public:
    using Input = render::VaryingSet4<DeferredFrameTransformPointer, LightingModelPointer, LightStage::FramePointer, LinearDepthFramebufferPointer>;
    using Output = LightClustersPointer;
    using Config = LightClusteringPassConfig;
    using JobModel = render::Job::ModelIO<LightClusteringPass, Input, Output, Config>;
    
    LightClusteringPass();
    
    void configure(const Config& config);
    
    void run(const render::RenderContextPointer& renderContext, const Input& input, Output& output);
    
protected:
    LightClustersPointer _lightClusters;
    bool _freeze;
};







class DebugLightClustersConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(bool doDrawGrid MEMBER doDrawGrid NOTIFY dirty)
    Q_PROPERTY(bool doDrawClusterFromDepth MEMBER doDrawClusterFromDepth NOTIFY dirty)
    Q_PROPERTY(bool doDrawContent MEMBER doDrawContent NOTIFY dirty)
public:
    DebugLightClustersConfig() : render::Job::Config(false){}


    bool doDrawGrid{ false };
    bool doDrawClusterFromDepth { false };
    bool doDrawContent { false };

signals:
    void dirty();

protected:
};


#include "DeferredFramebuffer.h"

class DebugLightClusters {
public:
    using Inputs = render::VaryingSet4 < DeferredFrameTransformPointer, LightingModelPointer, LinearDepthFramebufferPointer, LightClustersPointer>;
    using Config = DebugLightClustersConfig;
    using JobModel = render::Job::ModelI<DebugLightClusters, Inputs, Config>;

    DebugLightClusters(uint transformSlot);

    void configure(const Config& config);

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    gpu::BufferPointer _gridBuffer;
    gpu::PipelinePointer _drawClusterGrid;
    gpu::PipelinePointer _drawClusterFromDepth;
    gpu::PipelinePointer _drawClusterContent;
    uint _transformSlot;
    const gpu::PipelinePointer getDrawClusterGridPipeline();
    const gpu::PipelinePointer getDrawClusterFromDepthPipeline();
    const gpu::PipelinePointer getDrawClusterContentPipeline();
    bool doDrawGrid { false };
    bool doDrawClusterFromDepth { false };
    bool doDrawContent { false };
};

#endif
