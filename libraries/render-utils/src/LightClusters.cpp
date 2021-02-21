//
//  LightClusters.cpp
//
//  Created by Sam Gateau on 9/7/2016.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LightClusters.h"

#include <gpu/Context.h>
#include <shaders/Shaders.h>
#include <graphics/ShaderConstants.h>

#include "RenderUtilsLogging.h"
#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"


namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

FrustumGrid::FrustumGrid(const FrustumGrid& source) :
    frustumNear(source.frustumNear),
    rangeNear(source.rangeNear),
    rangeFar(source.rangeFar),
    frustumFar(source.frustumFar),
    dims(source.dims),
    spare(source.spare),
    eyeToGridProj(source.eyeToGridProj),
    worldToEyeMat(source.worldToEyeMat),
    eyeToWorldMat(source.eyeToWorldMat)
{}

void FrustumGrid::generateGridPlanes(Planes& xPlanes, Planes& yPlanes, Planes& zPlanes) {
    xPlanes.resize(dims.x + 1);
    yPlanes.resize(dims.y + 1);
    zPlanes.resize(dims.z + 1);

    float centerY = float(dims.y) * 0.5f;
    float centerX = float(dims.x) * 0.5f;

    for (int z = 0; z < (int) zPlanes.size(); z++) {
        ivec3 pos(0, 0, z);
        zPlanes[z] = glm::vec4(0.0f, 0.0f, 1.0f, -frustumGrid_clusterPosToEye(pos, vec3(0.0)).z);
    }

    for (int x = 0; x < (int) xPlanes.size(); x++) {
        auto slicePos = frustumGrid_clusterPosToEye(glm::vec3((float)x, centerY, 0.0));
        auto sliceDir = glm::normalize(slicePos);
        xPlanes[x] = glm::vec4(sliceDir.z, 0.0, -sliceDir.x, 0.0);
    }

    for (int y = 0; y < (int) yPlanes.size(); y++) {
        auto slicePos = frustumGrid_clusterPosToEye(glm::vec3(centerX, (float)y, 0.0));
        auto sliceDir = glm::normalize(slicePos);
        yPlanes[y] = glm::vec4(0.0, sliceDir.z, -sliceDir.y, 0.0);
    }

}

#include "DeferredLightingEffect.h"
#ifdef Q_OS_MAC
const glm::uvec4 LightClusters::MAX_GRID_DIMENSIONS { 16, 16, 16, 16384 };
#else
const glm::uvec4 LightClusters::MAX_GRID_DIMENSIONS { 32, 32, 31, 16384 };
#endif


LightClusters::LightClusters() :
    _lightIndicesBuffer(std::make_shared<gpu::Buffer>()),
    _clusterGridBuffer(/*std::make_shared<gpu::Buffer>(), */gpu::Element::INDEX_INT32),
    _clusterContentBuffer(/*std::make_shared<gpu::Buffer>(), */gpu::Element::INDEX_INT32) {
}

void LightClusters::setDimensions(glm::uvec3 gridDims, uint32_t listBudget) {
    ivec3 configDimensions;
    auto gridBudget = MAX_GRID_DIMENSIONS.w;
    configDimensions.x = std::max(1, (int) std::min(MAX_GRID_DIMENSIONS.x, gridDims.x));
    configDimensions.y = std::max(1, (int) std::min(MAX_GRID_DIMENSIONS.y, gridDims.y));
    configDimensions.z = std::max(1, (int) std::min(MAX_GRID_DIMENSIONS.z, gridDims.z));

    auto sliceCost = configDimensions.x * configDimensions.y;
    auto maxNumSlices = (int)(gridBudget / sliceCost) - 1;
    configDimensions.z = std::min(maxNumSlices, configDimensions.z);


    // Grab the frustumGridBuffer and force it updated
    const auto& constFrustumGrid = _frustumGridBuffer.get();
    const auto& dims = constFrustumGrid.dims;
    if ((dims.x != configDimensions.x) || (dims.y != configDimensions.y) || (dims.z != configDimensions.z)) {
        auto& theFrustumGrid = _frustumGridBuffer.edit();
        theFrustumGrid.dims = configDimensions;
        theFrustumGrid.generateGridPlanes(_gridPlanes[0], _gridPlanes[1], _gridPlanes[2]);
        _clusterResourcesInvalid = true;
    }

    auto configListBudget = std::min(MAX_GRID_DIMENSIONS.w, listBudget);
    if (_clusterContentBudget != configListBudget) {
        _clusterContentBudget = configListBudget;
        _clusterResourcesInvalid = true;
    }
}

uint32_t LightClusters::getNumClusters() const {
    auto theFrustumGrid = _frustumGridBuffer.get();
    return theFrustumGrid.frustumGrid_numClusters();
}

 void LightClusters::updateClusterResource() {
    if (!_clusterResourcesInvalid) {
        return;
    }
    _clusterResourcesInvalid = false;
    auto numClusters = getNumClusters();
    if (numClusters != (uint32_t) _clusterGrid.size()) {
        _clusterGrid.clear();
        _clusterGrid.resize(numClusters, EMPTY_CLUSTER);
        _clusterGridBuffer._size = (numClusters * sizeof(uint32_t));
        _clusterGridBuffer._buffer = std::make_shared<gpu::Buffer>(_clusterGridBuffer._size, (gpu::Byte*) _clusterGrid.data(), _clusterGridBuffer._size);
    }

    // Since LightIndex is 2bytes, we can fit 2 in a uint32
    auto configListBudget = _clusterContentBudget;
    if (sizeof(LightIndex) == 2) {
        configListBudget *= 2;
    }

    if (configListBudget != (uint32_t) _clusterContent.size()) {
        _clusterContent.clear();
        _clusterContent.resize(configListBudget, INVALID_LIGHT);
        _clusterContentBuffer._size = (configListBudget * sizeof(LightIndex));
        _clusterContentBuffer._buffer = std::make_shared<gpu::Buffer>(_clusterContentBuffer._size, (gpu::Byte*) _clusterContent.data(), _clusterContentBuffer._size);
    }
}

void LightClusters::setRangeNearFar(float rangeNear, float rangeFar) {
    bool changed = false;

    if (_frustumGridBuffer->rangeNear != rangeNear) {
        _frustumGridBuffer.edit().rangeNear = rangeNear;
        changed = true;
    }
    if (_frustumGridBuffer->rangeFar != rangeFar) {
        _frustumGridBuffer.edit().rangeFar = rangeFar;
        changed = true;
    }

    if (changed) {
        _frustumGridBuffer.edit().generateGridPlanes(_gridPlanes[0], _gridPlanes[1], _gridPlanes[2]);
    }
}

void LightClusters::updateFrustum(const ViewFrustum& frustum) {
    _frustum = frustum;

    _frustumGridBuffer.edit().updateFrustum(frustum);

    if (true) {
        _frustumGridBuffer.edit().generateGridPlanes(_gridPlanes[0], _gridPlanes[1], _gridPlanes[2]);
    }
}

void LightClusters::updateLightStage(const LightStagePointer& lightStage) {
    _lightStage = lightStage;
    
  }

void LightClusters::updateLightFrame(const LightStage::FramePointer& lightFrame, bool points, bool spots) {

    // start fresh
    _visibleLightIndices.clear();

    // Now gather the lights
    // gather lights
    auto& srcPointLights = lightFrame->_pointLights;
    auto& srcSpotLights = lightFrame->_spotLights;
    int numPointLights = (int)srcPointLights.size();
    int numSpotLights = (int)srcSpotLights.size();

    _visibleLightIndices.resize(numPointLights + numSpotLights + 1);

    _visibleLightIndices[0] = 0;

    if (points && !srcPointLights.empty()) {
        memcpy(_visibleLightIndices.data() + (_visibleLightIndices[0] + 1), srcPointLights.data(), srcPointLights.size() * sizeof(int));
        _visibleLightIndices[0] += (int)srcPointLights.size();
    }
    if (spots && !srcSpotLights.empty()) {
        memcpy(_visibleLightIndices.data() + (_visibleLightIndices[0] + 1), srcSpotLights.data(), srcSpotLights.size() * sizeof(int));
        _visibleLightIndices[0] += (int)srcSpotLights.size();
    }

    _lightIndicesBuffer._buffer->setData(_visibleLightIndices.size() * sizeof(int), (const gpu::Byte*) _visibleLightIndices.data());
    _lightIndicesBuffer._size = _visibleLightIndices.size() * sizeof(int);
}

float distanceToPlane(const glm::vec3& point, const glm::vec4& plane) {
    return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
}

bool reduceSphereToPlane(const glm::vec4& sphere, const glm::vec4& plane, glm::vec4& reducedSphere) {
    float distance = distanceToPlane(glm::vec3(sphere), plane);

    if (std::abs(distance) <= sphere.w) {
        reducedSphere = glm::vec4(sphere.x - distance * plane.x, sphere.y - distance * plane.y, sphere.z - distance * plane.z, sqrt(sphere.w * sphere.w - distance * distance));
        return true;
    }

    return false;
}


uint32_t scanLightVolumeBoxSlice(FrustumGrid& grid, const FrustumGrid::Planes planes[3], int zSlice, int yMin, int yMax, int xMin, int xMax, LightClusters::LightID lightId, const glm::vec4& eyePosRadius,
    std::vector< std::vector<LightClusters::LightIndex>>& clusterGrid) {
    glm::ivec3 gridPosToOffset(1, grid.dims.x, grid.dims.x * grid.dims.y);
    uint32_t numClustersTouched = 0;

    for (auto y = yMin; (y <= yMax); y++) {
        for (auto x = xMin; (x <= xMax); x++) {
            auto index = x + gridPosToOffset.y * y + gridPosToOffset.z * zSlice;
            clusterGrid[index].emplace_back(lightId);
            numClustersTouched++;
        }
    }

    return numClustersTouched;
}

uint32_t scanLightVolumeBox(FrustumGrid& grid, const FrustumGrid::Planes planes[3], int zMin, int zMax, int yMin, int yMax, int xMin, int xMax, LightClusters::LightID lightId, const glm::vec4& eyePosRadius,
    std::vector< std::vector<LightClusters::LightIndex>>& clusterGrid) {
    glm::ivec3 gridPosToOffset(1, grid.dims.x, grid.dims.x * grid.dims.y);
    uint32_t numClustersTouched = 0;

    for (auto z = zMin; (z <= zMax); z++) {
        for (auto y = yMin; (y <= yMax); y++) {
            for (auto x = xMin; (x <= xMax); x++) {
                auto index = x + gridPosToOffset.y * y + gridPosToOffset.z * z;
                clusterGrid[index].emplace_back(lightId);
                numClustersTouched++;
            }
        }
    }

    return numClustersTouched;
}

uint32_t scanLightVolumeSphere(FrustumGrid& grid, const FrustumGrid::Planes planes[3], int zMin, int zMax, int yMin, int yMax, int xMin, int xMax, LightClusters::LightID lightId, const glm::vec4& eyePosRadius,
    std::vector< std::vector<LightClusters::LightIndex>>& clusterGrid) {
    glm::ivec3 gridPosToOffset(1, grid.dims.x, grid.dims.x * grid.dims.y);
    uint32_t numClustersTouched = 0;
    const auto& xPlanes = planes[0];
    const auto& yPlanes = planes[1];
    const auto& zPlanes = planes[2];

    // FInd the light origin cluster
    auto centerCluster = grid.frustumGrid_eyeToClusterPos(glm::vec3(eyePosRadius));

    int center_z = centerCluster.z;
    int center_y = centerCluster.y;

    for (auto z = zMin; (z <= zMax); z++) {
        auto zSphere = eyePosRadius;
        if (z != center_z) {
            auto plane = (z < center_z) ? zPlanes[z + 1] : -zPlanes[z];
            if (!reduceSphereToPlane(zSphere, plane, zSphere)) {
                // pass this slice!
                continue;
            }
        }
        for (auto y = yMin; (y <= yMax); y++) {
            auto ySphere = zSphere;
            if (y != center_y) {
                auto plane = (y < center_y) ? yPlanes[y + 1] : -yPlanes[y];
                if (!reduceSphereToPlane(ySphere, plane, ySphere)) {
                    // pass this slice!
                    continue;
                }
            }

            glm::vec3 spherePoint(ySphere);

            auto x = xMin;
            for (; (x < xMax); ++x) {
                const auto& plane = xPlanes[x + 1];
                auto testDistance = distanceToPlane(spherePoint, plane) + ySphere.w;
                if (testDistance >= 0.0f) {
                    break;
                }
            }
            auto xs = xMax;
            for (; (xs >= x); --xs) {
                auto plane = -xPlanes[xs];
                auto testDistance = distanceToPlane(spherePoint, plane) + ySphere.w;
                if (testDistance >= 0.0f) {
                    break;
                }
            }

            for (; (x <= xs); x++) {
                auto index = grid.frustumGrid_clusterToIndex(ivec3(x, y, z));
                if (index < (int)clusterGrid.size()) {
                    clusterGrid[index].emplace_back(lightId);
                    numClustersTouched++;
                } else {
                    qCDebug(renderutils) << "WARNING: LightClusters::scanLightVolumeSphere invalid index found ? numClusters = " << clusterGrid.size() << " index = " << index << " found from cluster xyz = " << x << " " << y << " " << z;
                }
            }
        }
    }

    return numClustersTouched;
}

glm::ivec3 LightClusters::updateClusters() {
    // Make sure resource are in good shape
    updateClusterResource();

    // Clean up last info
    uint32_t numClusters = (uint32_t)_clusterGrid.size();

    std::vector< std::vector< LightIndex > > clusterGridPoint(numClusters);
    std::vector< std::vector< LightIndex > > clusterGridSpot(numClusters);

    _clusterGrid.clear();
    _clusterGrid.resize(numClusters, EMPTY_CLUSTER);

    uint32_t maxNumIndices = (uint32_t)_clusterContent.size();
    _clusterContent.clear();
    _clusterContent.resize(maxNumIndices, INVALID_LIGHT);


    auto theFrustumGrid(_frustumGridBuffer.get());

    glm::ivec3 gridPosToOffset(1, theFrustumGrid.dims.x, theFrustumGrid.dims.x * theFrustumGrid.dims.y);

    uint32_t numClusterTouched = 0;
    uint32_t numLightsIn = _visibleLightIndices[0];
    uint32_t numClusteredLights = 0;
    for (size_t lightNum = 1; lightNum < _visibleLightIndices.size(); ++lightNum) {
        auto lightId = _visibleLightIndices[lightNum];
        auto light = _lightStage->getLight(lightId);
        if (!light) {
            continue;
        }

        auto worldOri = light->getPosition();
        auto radius = light->getMaximumRadius();
        bool isSpot = light->isSpot();

        // Bring into frustum eye space
        auto eyeOri = theFrustumGrid.frustumGrid_worldToEye(glm::vec4(worldOri, 1.0f));

        // Remove light that slipped through and is not in the z range
        float eyeZMax = eyeOri.z - radius;
        if (eyeZMax > -theFrustumGrid.rangeNear) {
            continue;
        }
        float eyeZMin = eyeOri.z + radius;
        bool beyondFar = false;
        if (eyeZMin < -theFrustumGrid.rangeFar) {
            beyondFar = true;
        }

        // Get z slices
        int zMin = theFrustumGrid.frustumGrid_eyeDepthToClusterLayer(eyeZMin);
        int zMax = theFrustumGrid.frustumGrid_eyeDepthToClusterLayer(eyeZMax);
        // That should never happen
        if (zMin == -2 && zMax == -2) {
            continue;
        }

        // Before Range NEar just apss, range neatr == true near for now
        if ((zMin == -1) && (zMax == -1)) {
            continue;
        }

        // CLamp the z range 
        zMin = std::max(0, zMin);

        auto xLeftDistance = radius - distanceToPlane(eyeOri, _gridPlanes[0][0]);
        auto xRightDistance = radius + distanceToPlane(eyeOri, _gridPlanes[0].back());

        auto yBottomDistance = radius - distanceToPlane(eyeOri, _gridPlanes[1][0]);
        auto yTopDistance = radius + distanceToPlane(eyeOri, _gridPlanes[1].back());

        if ((xLeftDistance < 0.f) || (xRightDistance < 0.f) || (yBottomDistance < 0.f) || (yTopDistance < 0.f)) {
            continue;
        }

        // find 2D corners of the sphere in grid
        int xMin { 0 };
        int xMax { theFrustumGrid.dims.x - 1 };
        int yMin { 0 };
        int yMax { theFrustumGrid.dims.y - 1 };

        float radius2 = radius * radius;

        auto eyeOriH = glm::vec3(eyeOri);
        auto eyeOriV = glm::vec3(eyeOri);

        eyeOriH.y = 0.0f;
        eyeOriV.x = 0.0f;

        float eyeOriLen2H = glm::length2(eyeOriH);
        float eyeOriLen2V = glm::length2(eyeOriV);

        if ((eyeOriLen2H > radius2)) {
            float eyeOriLenH = sqrt(eyeOriLen2H);

            auto eyeOriDirH = glm::vec3(eyeOriH) / eyeOriLenH;

            float eyeToTangentCircleLenH = sqrt(eyeOriLen2H - radius2);

            float eyeToTangentCircleCosH = eyeToTangentCircleLenH / eyeOriLenH;

            float eyeToTangentCircleSinH = radius / eyeOriLenH;


            // rotate the eyeToOriDir (H & V) in both directions
            glm::vec3 leftDir(eyeOriDirH.x * eyeToTangentCircleCosH + eyeOriDirH.z * eyeToTangentCircleSinH, 0.0f, eyeOriDirH.x * -eyeToTangentCircleSinH + eyeOriDirH.z * eyeToTangentCircleCosH);
            glm::vec3 rightDir(eyeOriDirH.x * eyeToTangentCircleCosH - eyeOriDirH.z * eyeToTangentCircleSinH, 0.0f, eyeOriDirH.x * eyeToTangentCircleSinH + eyeOriDirH.z * eyeToTangentCircleCosH);

            auto lc = theFrustumGrid.frustumGrid_eyeToClusterDirH(leftDir);
            if (lc > xMax) {
                lc = xMin;
            }
            auto rc = theFrustumGrid.frustumGrid_eyeToClusterDirH(rightDir);
            if (rc < 0) {
                rc = xMax;
            }
            xMin = std::max(xMin, lc);
            xMax = std::min(rc, xMax);
            assert(xMin <= xMax);
        }

        if ((eyeOriLen2V > radius2)) {
            float eyeOriLenV = sqrt(eyeOriLen2V);

            auto eyeOriDirV = glm::vec3(eyeOriV) / eyeOriLenV;

            float eyeToTangentCircleLenV = sqrt(eyeOriLen2V - radius2);

            float eyeToTangentCircleCosV = eyeToTangentCircleLenV / eyeOriLenV;

            float eyeToTangentCircleSinV = radius / eyeOriLenV;


            // rotate the eyeToOriDir (H & V) in both directions
            glm::vec3 bottomDir(0.0f, eyeOriDirV.y * eyeToTangentCircleCosV + eyeOriDirV.z * eyeToTangentCircleSinV, eyeOriDirV.y * -eyeToTangentCircleSinV + eyeOriDirV.z * eyeToTangentCircleCosV);
            glm::vec3 topDir(0.0f, eyeOriDirV.y * eyeToTangentCircleCosV - eyeOriDirV.z * eyeToTangentCircleSinV, eyeOriDirV.y * eyeToTangentCircleSinV + eyeOriDirV.z * eyeToTangentCircleCosV);

            auto bc = theFrustumGrid.frustumGrid_eyeToClusterDirV(bottomDir);
            auto tc = theFrustumGrid.frustumGrid_eyeToClusterDirV(topDir);
            if (bc > yMax) {
                bc = yMin;
            }
            if (tc < 0) {
                tc = yMax;
            }
            yMin = std::max(yMin, bc);
            yMax =std::min(tc, yMax);
            assert(yMin <= yMax);
        }

        // now voxelize
        auto& clusterGrid = (isSpot ? clusterGridSpot : clusterGridPoint);
        if (beyondFar) {
            numClusterTouched += scanLightVolumeBoxSlice(theFrustumGrid, _gridPlanes, zMin, yMin, yMax, xMin, xMax, lightId, glm::vec4(glm::vec3(eyeOri), radius), clusterGrid);
        } else {
            numClusterTouched += scanLightVolumeSphere(theFrustumGrid, _gridPlanes, zMin, zMax, yMin, yMax, xMin, xMax, lightId, glm::vec4(glm::vec3(eyeOri), radius), clusterGrid);
        }

        numClusteredLights++;
    }

    // Lights have been gathered now reexpress in terms of 2 sequential buffers
    // Start filling from near to far and stops if it overflows
    bool checkBudget = false;
    if (numClusterTouched > maxNumIndices) {
        checkBudget = true;
    }
    uint16_t indexOffset = 0;
    for (int i = 0; i < (int) clusterGridPoint.size(); i++) {
        auto& clusterPoint = clusterGridPoint[i];
        auto& clusterSpot = clusterGridSpot[i];

        uint8_t numLightsPoint = ((uint8_t)clusterPoint.size());
        uint8_t numLightsSpot = ((uint8_t)clusterSpot.size());
        uint16_t numLights = numLightsPoint + numLightsSpot;
        uint16_t offset = indexOffset;

        // Check for overflow
        if (checkBudget) {
            if ((indexOffset + numLights) > (uint16_t) maxNumIndices) {
                break;
            }
        }

        // Encode the cluster grid: [ ContentOffset - 16bits, Num Point LIghts - 8bits, Num Spot Lights - 8bits] 
        _clusterGrid[i] = (uint32_t)((0xFF000000 & (numLightsSpot << 24)) | (0x00FF0000 & (numLightsPoint << 16)) | (0x0000FFFF & offset));


        if (numLightsPoint) {
            memcpy(_clusterContent.data() + indexOffset, clusterPoint.data(), numLightsPoint * sizeof(LightIndex));
            indexOffset += numLightsPoint;
        }
        if (numLightsSpot) {
            memcpy(_clusterContent.data() + indexOffset, clusterSpot.data(), numLightsSpot * sizeof(LightIndex));
            indexOffset += numLightsSpot;
        }
    }

    // update the buffers
    _clusterGridBuffer._buffer->setData(_clusterGridBuffer._size, (gpu::Byte*) _clusterGrid.data());
    _clusterContentBuffer._buffer->setSubData(0, indexOffset * sizeof(LightIndex), (gpu::Byte*) _clusterContent.data());
    
    return glm::ivec3(numLightsIn, numClusteredLights, numClusterTouched);
}



LightClusteringPass::LightClusteringPass() {
    _lightClusters = std::make_shared<LightClusters>();
}


void LightClusteringPass::configure(const Config& config) {
    if (_lightClusters) {
        _lightClusters->setRangeNearFar(config.rangeNear, config.rangeFar);
        _lightClusters->setDimensions(glm::uvec3(config.dimX, config.dimY, config.dimZ));
    }
    
    _freeze = config.freeze;
}

void LightClusteringPass::run(const render::RenderContextPointer& renderContext, const Input& inputs, Output& output) {
    auto args = renderContext->args;
    
    auto deferredTransform = inputs.get0();
    auto lightingModel = inputs.get1();
    auto lightFrame = inputs.get2();
    auto surfaceGeometryFramebuffer = inputs.get3();

    // first update the Grid with the new frustum
    if (!_freeze) {
        _lightClusters->updateFrustum(args->getViewFrustum());
    }
    
    // From the LightStage and the current frame, update the light cluster Grid
    auto lightStage = renderContext->_scene->getStage<LightStage>();
    assert(lightStage);
    _lightClusters->updateLightStage(lightStage);
    _lightClusters->updateLightFrame(lightFrame, lightingModel->isPointLightEnabled(), lightingModel->isSpotLightEnabled());
    
    auto clusteringStats = _lightClusters->updateClusters();

    output = _lightClusters;

    auto config = std::static_pointer_cast<Config>(renderContext->jobConfig);
    config->numSceneLights = lightStage->getNumLights();
    config->numFreeSceneLights = lightStage->getNumFreeLights();
    config->numAllocatedSceneLights = lightStage->getNumAllocatedLights();
    config->setNumInputLights(clusteringStats.x);
    config->setNumClusteredLights(clusteringStats.y);
    config->setNumClusteredLightReferences(clusteringStats.z);
}

DebugLightClusters::DebugLightClusters(uint transformSlot) : _transformSlot(transformSlot) {

}


void DebugLightClusters::configure(const Config& config) {
    doDrawGrid = config.doDrawGrid;
    doDrawClusterFromDepth = config.doDrawClusterFromDepth;
    doDrawContent = config.doDrawContent;

}

const gpu::PipelinePointer DebugLightClusters::getDrawClusterGridPipeline() {
    if (!_drawClusterGrid) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::lightClusters_drawGrid);
        auto state = std::make_shared<gpu::State>();

        state->setDepthTest(true, false, gpu::LESS_EQUAL);

        // Blend on transparent
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawClusterGrid = gpu::Pipeline::create(program, state);
    }
    return _drawClusterGrid;
}

const gpu::PipelinePointer DebugLightClusters::getDrawClusterFromDepthPipeline() {
    if (!_drawClusterFromDepth) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::lightClusters_drawClusterFromDepth);
        auto state = std::make_shared<gpu::State>();
        
        // Blend on transparent
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawClusterFromDepth = gpu::Pipeline::create(program, state);
    }
    return _drawClusterFromDepth;
}

const gpu::PipelinePointer DebugLightClusters::getDrawClusterContentPipeline() {
    if (!_drawClusterContent) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::lightClusters_drawClusterContent);
        auto state = std::make_shared<gpu::State>();
        
        // Blend on transparent
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawClusterContent = gpu::Pipeline::create(program, state);
    }
    return _drawClusterContent;
}


void DebugLightClusters::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    if (!(doDrawClusterFromDepth || doDrawContent || doDrawGrid)) {
        return;
    }

    auto deferredTransform = inputs.get0();
    auto lightingModel = inputs.get1();
    auto linearDepthTarget = inputs.get2();
    auto lightClusters = inputs.get3();

    auto args = renderContext->args;

    
    gpu::doInBatch(nullptr, args->_context, [&](gpu::Batch& batch) { 
        batch.enableStereo(false);

        // Assign the camera transform
        batch.setViewportTransform(args->_viewport);
        batch.setSavedViewProjectionTransform(_transformSlot);

        // Then the actual ClusterGrid attributes
        batch.setModelTransform(Transform());

        // Bind the Light CLuster data strucutre
        // FIXME consolidate code with DeferredLightingEffect logic that does the same thing
        batch.setUniformBuffer(gr::Buffer::Light, lightClusters->_lightStage->getLightArrayBuffer());
        batch.setUniformBuffer(ru::Buffer::LightClusterFrustumGrid, lightClusters->_frustumGridBuffer);
        batch.setUniformBuffer(ru::Buffer::LightClusterGrid, lightClusters->_clusterGridBuffer);
        batch.setUniformBuffer(ru::Buffer::LightClusterContent, lightClusters->_clusterContentBuffer);

        if (doDrawClusterFromDepth) {
            batch.setPipeline(getDrawClusterFromDepthPipeline());
            batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, deferredTransform->getFrameTransformBuffer());

            if (linearDepthTarget) {
                batch.setResourceTexture(ru::Texture::DeferredLinearZEye, linearDepthTarget->getLinearDepthTexture());
            }

            batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
              
            batch.setResourceTexture(ru::Texture::DeferredLinearZEye, nullptr);
            batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, nullptr);
        }

        if (doDrawContent) {

            // bind the one gpu::Pipeline we need
            batch.setPipeline(getDrawClusterContentPipeline());
            batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, deferredTransform->getFrameTransformBuffer());

            if (linearDepthTarget) {
                batch.setResourceTexture(ru::Texture::DeferredLinearZEye, linearDepthTarget->getLinearDepthTexture());
            }

            batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
              
            batch.setResourceTexture(ru::Texture::DeferredLinearZEye, nullptr);
            batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, nullptr);
        }
    });



    gpu::doInBatch(nullptr, args->_context, [&](gpu::Batch& batch) { 
        auto& drawGridAndCleanBatch = batch;
        if (doDrawGrid) {
            // bind the one gpu::Pipeline we need
            drawGridAndCleanBatch.setPipeline(getDrawClusterGridPipeline());

            auto dims = lightClusters->_frustumGridBuffer->dims;
            glm::ivec3 summedDims(dims.x * dims.y * dims.z, dims.x * dims.y, dims.x);
            drawGridAndCleanBatch.drawInstanced(summedDims.x, gpu::LINES, 24, 0);
        }

        drawGridAndCleanBatch.setUniformBuffer(gr::Buffer::Light, nullptr);
        drawGridAndCleanBatch.setUniformBuffer(ru::Buffer::LightClusterFrustumGrid, nullptr);
        drawGridAndCleanBatch.setUniformBuffer(ru::Buffer::LightClusterGrid, nullptr);
        drawGridAndCleanBatch.setUniformBuffer(ru::Buffer::LightClusterContent, nullptr);

        drawGridAndCleanBatch.setResourceTexture(ru::Texture::DeferredColor, nullptr);
        drawGridAndCleanBatch.setResourceTexture(ru::Texture::DeferredNormal, nullptr);
        drawGridAndCleanBatch.setResourceTexture(ru::Texture::DeferredSpecular, nullptr);
        drawGridAndCleanBatch.setResourceTexture(ru::Texture::DeferredLinearZEye, nullptr);
    });
}
