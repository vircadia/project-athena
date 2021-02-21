//
//  RenderablePolyVoxEntityItem.cpp
//  libraries/entities-renderer/src/
//
//  Created by Seth Alves on 5/19/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "RenderablePolyVoxEntityItem.h"

#include <math.h>

#include <glm/gtx/transform.hpp>

#include <QObject>
#include <QByteArray>
#include <QtConcurrent/QtConcurrentRun>

#include <model-networking/SimpleMeshProxy.h>
#include <ModelScriptingInterface.h>
#include <EntityEditPacketSender.h>
#include <PhysicalEntitySimulation.h>
#include <StencilMaskPass.h>
#include <graphics/ShaderConstants.h>
#include <render/ShapePipeline.h>

#include "entities-renderer/ShaderConstants.h"

#include <shaders/Shaders.h>

#include "EntityTreeRenderer.h"

#include <FadeEffect.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <Model.h>
#include <PerfStat.h>
#include <render/Scene.h>

#ifdef _WIN32
#pragma warning(push)
#pragma warning( disable : 4267 )
#endif
#include <PolyVoxCore/CubicSurfaceExtractorWithNormals.h>
#include <PolyVoxCore/MarchingCubesSurfaceExtractor.h>
#include <PolyVoxCore/SurfaceMesh.h>
#include <PolyVoxCore/SimpleVolume.h>
#include <PolyVoxCore/Material.h>
#ifdef _WIN32
#pragma warning(pop)
#endif

#include "graphics/Geometry.h"

#include "StencilMaskPass.h"

#include "EntityTreeRenderer.h"

#include "RenderablePolyVoxEntityItem.h"
#include "PhysicalEntitySimulation.h"

const float MARCHING_CUBE_COLLISION_HULL_OFFSET = 0.5;

/*
  A PolyVoxEntity has several interdependent parts:

  _voxelData -- compressed QByteArray representation of which voxels have which values
  _volData -- datastructure from the PolyVox library which holds which voxels have which values
  _mesh -- renderable representation of the voxels
  _shape -- used for bullet (physics) collisions

  Each one depends on the one before it, except that _voxelData is set from _volData if a script edits the voxels.

  There are booleans to indicate that something has been updated and the dependents now need to be updated.
  _meshReady       -- do we have something to give scripts that ask for the mesh?
  _voxelDataDirty  -- do we need to uncompress data and expand it into _volData?
  _volDataDirty    -- does recomputeMesh need to be called?
  _shapeReady      -- are we ready to tell bullet our shape?


  Here is a simplified diagram of the state machine implemented in RenderablePolyVoxEntityItem::update

  +-------------------+
  |                   |
  |                   |
  |  volDataDirty     |     voxelDataDirty
  |                +--v--+
  |         +------+Ready+--------+
  |         |      +-----+        |
  |         |                     |
  |   +-----v----+         +------v------+
  |   |BakingMesh|         |Uncompressing|
  |   +-----+----+         +------+------+
  |         |                     |
  |         |                     |
  |     +---v-------+     +-------v------------+
  |     |Compressing|     |BakingMeshNoCompress|
  |     +---------+-+     ++-------------------+
  |               |        |
  |               |        |
  |             +-v--------v+
  |             |BakingShape|
  |             +-----+-----+
  |                   |
  +-------------------+


  The work for each step is done on temporary worker threads.  The PolyVox entity will update _updateNeeded and
  enable or disable update calls on the entity, depending on if there is more work to do.

  From the 'Ready' state, if we receive an update from the network, _voxelDataDirty will be set true.  We
  uncompress the received data, bake the mesh (for the render-engine's benefit), and then compute the shape
  (for the physics-engine's benefit).  This is the right-hand side of the diagram.

  From the 'Ready' state, if a script changes a voxel, _volDataDirty will be set true.  We bake the mesh,
  compress the voxels into a new _voxelData, and transmit the new _voxelData to the entity-server.  We then
  bake the shape.  This is the left-hand side of the diagram.

  The actual state machine is more complicated than the diagram, because it's possible for _volDataDirty or
  _voxelDataDirty to be set true while worker threads are attempting to bake meshes or shapes.  If this happens,
  we jump back to a higher point in the diagram to avoid wasting effort.

  PolyVoxes are designed to seemlessly fit up against neighbors.  If voxels go right up to the edge of polyvox,
  the resulting mesh wont be closed -- the library assumes you'll have another polyvox next to it to continue the
  mesh.

  If a polyvox entity is "edged", the voxel space is wrapped in an extra layer of zero-valued voxels.  This avoids the
  previously mentioned gaps along the edges.

  Non-edged polyvox entities can be told about their neighbors in all 6 cardinal directions.  On the positive
  edges of the polyvox, the values are set from the (negative edge of the) relevant neighbor so that their meshes
  knit together.  This is handled by tellNeighborsToRecopyEdges and copyUpperEdgesFromNeighbors.  In these functions, variable
  names have XP for x-positive, XN x-negative, etc.

 */

 // FIXME move to GLM helpers
template <typename T, typename F>
void loop3(const T& start, const T& end, F f) {
    T current;
    for (current.z = start.z; current.z < end.z; ++current.z) {
        for (current.y = start.y; current.y < end.y; ++current.y) {
            for (current.x = start.x; current.x < end.x; ++current.x) {
                f(current);
            }
        }
    }
}

EntityItemPointer RenderablePolyVoxEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    std::shared_ptr<RenderablePolyVoxEntityItem> entity(new RenderablePolyVoxEntityItem(entityID), [](EntityItem* ptr) { ptr->deleteLater(); });
    entity->setProperties(properties);
    entity->initializePolyVox();
    return entity;
}

RenderablePolyVoxEntityItem::RenderablePolyVoxEntityItem(const EntityItemID& entityItemID) : PolyVoxEntityItem(entityItemID) {
}

RenderablePolyVoxEntityItem::~RenderablePolyVoxEntityItem() {
    withWriteLock([&] {
        _volData.reset();
    });
}

void RenderablePolyVoxEntityItem::initializePolyVox() {
    setVoxelVolumeSize(_voxelVolumeSize);
}

bool isEdged(PolyVoxEntityItem::PolyVoxSurfaceStyle surfaceStyle) {
    switch (surfaceStyle) {
        case PolyVoxEntityItem::SURFACE_CUBIC:
        case PolyVoxEntityItem::SURFACE_MARCHING_CUBES:
            return false;
        case PolyVoxEntityItem::SURFACE_EDGED_CUBIC:
        case PolyVoxEntityItem::SURFACE_EDGED_MARCHING_CUBES:
            return true;
    }
    return false;
}

void RenderablePolyVoxEntityItem::setVoxelData(const QByteArray& voxelData) {
    // accept compressed voxel information from the entity-server
    withWriteLock([&] {
        if (_voxelData != voxelData) {
            _voxelData = voxelData;
            _voxelDataDirty = true;
            startUpdates();
        }
    });
}

void RenderablePolyVoxEntityItem::setVoxelSurfaceStyle(PolyVoxSurfaceStyle voxelSurfaceStyle) {
    // this controls whether the polyvox surface extractor does marching-cubes or makes a cubic mesh.  It
    // also determines if the extra "edged" layer is used.
    bool volSizeChanged = false;

    withWriteLock([&] {
        if (_voxelSurfaceStyle == voxelSurfaceStyle) {
            return;
        }

        // if we are switching to or from "edged" we need to force a resize of _volData.
        bool wasEdged = isEdged();
        bool willBeEdged = isEdged(voxelSurfaceStyle);

        if (wasEdged != willBeEdged) {
            _volData.reset();
            _voxelDataDirty = true;
            volSizeChanged = true;
        }
        _voxelSurfaceStyle = voxelSurfaceStyle;
        startUpdates();
    });

    if (volSizeChanged) {
        // setVoxelVolumeSize will re-alloc _volData with the right size
        setVoxelVolumeSize(_voxelVolumeSize);
    }

    _updateFromNeighborXEdge = _updateFromNeighborYEdge = _updateFromNeighborZEdge = true;
    tellNeighborsToRecopyEdges(true);
    startUpdates();
}

bool RenderablePolyVoxEntityItem::setVoxel(const ivec3& v, uint8_t toValue) {
    if (_locked) {
        return false;
    }

    bool result = false;
    withWriteLock([&] {
        result = setVoxelInternal(v, toValue);
    });

    return result;
}

void RenderablePolyVoxEntityItem::forEachVoxelValue(const ivec3& voxelSize, std::function<void(const ivec3& v, uint8_t)> thunk) {
    // a thread-safe way for code outside this class to iterate over a range of voxels
    withReadLock([&] {
        loop3(ivec3(0), voxelSize, [&](const ivec3& v) {
            uint8_t uVoxelValue = getVoxelInternal(v);
            thunk(v, uVoxelValue);
        });
    });
}

QByteArray RenderablePolyVoxEntityItem::volDataToArray(quint16 voxelXSize, quint16 voxelYSize, quint16 voxelZSize) const {
    int totalSize = voxelXSize * voxelYSize * voxelZSize;
    ivec3 voxelSize{ voxelXSize, voxelYSize, voxelZSize };
    ivec3 low{ 0 };
    int index = 0;

    QByteArray result = QByteArray(totalSize, '\0');
    withReadLock([&] {
        if (isEdged()) {
            low += 1;
            voxelSize += 2;
        }

        loop3(low, voxelSize, [&](const ivec3& v){
            result[index++] = _volData->getVoxelAt(v.x, v.y, v.z);
        });
    });

    return result;
}

bool RenderablePolyVoxEntityItem::setAll(uint8_t toValue) {
    bool result = false;
    if (_locked) {
        return result;
    }

    withWriteLock([&] {
        loop3(ivec3(0), ivec3(_voxelVolumeSize), [&](const ivec3& v) {
            result |= setVoxelInternal(v, toValue);
        });
    });
    return result;
}


bool RenderablePolyVoxEntityItem::setCuboid(const glm::vec3& lowPosition, const glm::vec3& cuboidSize, int toValue) {
    bool result = false;
    if (_locked) {
        return result;
    }

    ivec3 iLowPosition = ivec3{ glm::round(lowPosition) };
    ivec3 iCuboidSize = ivec3{ glm::round(cuboidSize) };
    ivec3 iVoxelVolumeSize = ivec3{ glm::round(_voxelVolumeSize) };

    ivec3 low = glm::max(glm::min(iLowPosition, iVoxelVolumeSize - 1), ivec3(0));
    ivec3 high = glm::max(glm::min(low + iCuboidSize, iVoxelVolumeSize), low);

    withWriteLock([&] {
        loop3(low, high, [&] (const ivec3& v){
            result |= setVoxelInternal(v, toValue);
        });
    });
    return result;
}



bool RenderablePolyVoxEntityItem::setVoxelInVolume(const vec3& position, uint8_t toValue) {
    if (_locked) {
        return false;
    }

    // same as setVoxel but takes a vector rather than 3 floats.
    return setVoxel(glm::round(position), toValue);
}

bool RenderablePolyVoxEntityItem::setSphereInVolume(const vec3& center, float radius, uint8_t toValue) {
    bool result = false;
    if (_locked) {
        return result;
    }

    float radiusSquared = radius * radius;
    // This three-level for loop iterates over every voxel in the volume
    withWriteLock([&] {
        loop3(ivec3(0), ivec3(_voxelVolumeSize), [&](const ivec3& v) {
            // Store our current position as a vector...
            glm::vec3 pos = vec3(v) + 0.5f; // consider voxels cenetered on their coordinates
            // And compute how far the current position is from the center of the volume
            float fDistToCenterSquared = glm::distance2(pos, center);
            // If the current voxel is less than 'radius' units from the center then we set its value
            if (fDistToCenterSquared <= radiusSquared) {
                result |= setVoxelInternal(v, toValue);
            }
        });
    });

    return result;
}

bool RenderablePolyVoxEntityItem::setSphere(const vec3& centerWorldCoords, float radiusWorldCoords, uint8_t toValue) {
    bool result = false;
    if (_locked) {
        return result;
    }

    glm::mat4 vtwMatrix = voxelToWorldMatrix();
    glm::mat4 wtvMatrix = glm::inverse(vtwMatrix);

    glm::vec3 dimensions = getScaledDimensions();
    glm::vec3 voxelSize = dimensions / _voxelVolumeSize;
    float smallestDimensionSize = voxelSize.x;
    smallestDimensionSize = glm::min(smallestDimensionSize, voxelSize.y);
    smallestDimensionSize = glm::min(smallestDimensionSize, voxelSize.z);
    if (smallestDimensionSize <= 0.0f) {
        return false;
    }

    glm::vec3 maxRadiusInVoxelCoords = glm::vec3(radiusWorldCoords / smallestDimensionSize);
    glm::vec3 centerInVoxelCoords = wtvMatrix * glm::vec4(centerWorldCoords, 1.0f);

    glm::vec3 low = glm::floor(centerInVoxelCoords - maxRadiusInVoxelCoords);
    glm::vec3 high = glm::ceil(centerInVoxelCoords + maxRadiusInVoxelCoords);

    glm::ivec3 lowI = glm::clamp(low, glm::vec3(0.0f), _voxelVolumeSize);
    glm::ivec3 highI = glm::clamp(high, glm::vec3(0.0f), _voxelVolumeSize);

    glm::vec3 radials(radiusWorldCoords / voxelSize.x,
                      radiusWorldCoords / voxelSize.y,
                      radiusWorldCoords / voxelSize.z);

    // This three-level for loop iterates over every voxel in the volume that might be in the sphere
    withWriteLock([&] {
        loop3(lowI, highI, [&](const ivec3& v) {
            // set voxels whose bounding-box touches the sphere
            AABox voxelBox(glm::vec3(v) - 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
            if (voxelBox.touchesAAEllipsoid(centerInVoxelCoords, radials)) {
                result |= setVoxelInternal(v, toValue);
            }

            // TODO -- this version only sets voxels which have centers inside the sphere.  which is best?
            // // Store our current position as a vector...
            // glm::vec4 pos(x + 0.5f, y + 0.5f, z + 0.5f, 1.0); // consider voxels cenetered on their coordinates
            // // convert to world coordinates
            // glm::vec3 worldPos = glm::vec3(vtwMatrix * pos);
            // // compute how far the current position is from the center of the volume
            // float fDistToCenter = glm::distance(worldPos, centerWorldCoords);
            // // If the current voxel is less than 'radius' units from the center then we set its value
            // if (fDistToCenter <= radiusWorldCoords) {
            //     result |= setVoxelInternal(x, y, z, toValue);
            // }
        });
    });

    return result;
}

bool RenderablePolyVoxEntityItem::setCapsule(const vec3& startWorldCoords, const vec3& endWorldCoords,
                                             float radiusWorldCoords, uint8_t toValue) {
    bool result = false;
    if (_locked) {
        return result;
    }

    glm::mat4 vtwMatrix = voxelToWorldMatrix();
    glm::mat4 wtvMatrix = glm::inverse(vtwMatrix);

    glm::vec3 dimensions = getScaledDimensions();
    glm::vec3 voxelSize = dimensions / _voxelVolumeSize;
    float smallestDimensionSize = voxelSize.x;
    smallestDimensionSize = glm::min(smallestDimensionSize, voxelSize.y);
    smallestDimensionSize = glm::min(smallestDimensionSize, voxelSize.z);

    glm::vec3 maxRadiusInVoxelCoords = glm::vec3(radiusWorldCoords / smallestDimensionSize);

    glm::vec3 startInVoxelCoords = wtvMatrix * glm::vec4(startWorldCoords, 1.0f);
    glm::vec3 endInVoxelCoords = wtvMatrix * glm::vec4(endWorldCoords, 1.0f);

    glm::vec3 low = glm::min(glm::floor(startInVoxelCoords - maxRadiusInVoxelCoords),
                             glm::floor(endInVoxelCoords - maxRadiusInVoxelCoords));
    glm::vec3 high = glm::max(glm::ceil(startInVoxelCoords + maxRadiusInVoxelCoords),
                              glm::ceil(endInVoxelCoords + maxRadiusInVoxelCoords));

    glm::ivec3 lowI = glm::clamp(low, glm::vec3(0.0f), _voxelVolumeSize);
    glm::ivec3 highI = glm::clamp(high, glm::vec3(0.0f), _voxelVolumeSize);

    // This three-level for loop iterates over every voxel in the volume that might be in the capsule
    withWriteLock([&] {
        loop3(lowI, highI, [&](const ivec3& v) {
            // Store our current position as a vector...
            glm::vec4 pos{ vec3(v) + 0.5f, 1.0 }; // consider voxels cenetered on their coordinates
            // convert to world coordinates
            glm::vec3 worldPos = glm::vec3(vtwMatrix * pos);
            if (pointInCapsule(worldPos, startWorldCoords, endWorldCoords, radiusWorldCoords)) {
                result |= setVoxelInternal(v, toValue);
            }
        });
    });

    return result;
}

class RaycastFunctor {
public:
    RaycastFunctor(const std::shared_ptr<PolyVox::SimpleVolume<uint8_t>>& vol) :
        _result(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)),
        _vol(vol) {
    }

    static bool inBounds(const std::shared_ptr<PolyVox::SimpleVolume<uint8_t>>& vol, const ivec3& v) {
        // x, y, z are in polyvox volume coords
        ivec3 volSize{ vol->getWidth(), vol->getHeight(), vol->getDepth() };
        return glm::all(glm::greaterThanEqual(v, ivec3(0))) && glm::all(glm::lessThan(v, volSize));
    }

    bool operator()(PolyVox::SimpleVolume<unsigned char>::Sampler& sampler) {
        PolyVox::Vector3DInt32 positionIndex = sampler.getPosition();
        ivec3 v{ positionIndex.getX(), positionIndex.getY(), positionIndex.getZ() };

        if (!inBounds(_vol, v)) {
            return true;
        }

        if (sampler.getVoxel() == 0) {
            return true; // keep raycasting
        }

        _result = glm::vec4(v, 1.0f);
        return false;
    }
    glm::vec4 _result;
    const std::shared_ptr<PolyVox::SimpleVolume<uint8_t>> _vol;
};

#if 0
class RaycastFunctor
{
public:
    RaycastFunctor(PolyVox::SimpleVolume<uint8_t>* vol) :
        _result(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)),
        _vol(vol) {
    }

    static bool inBounds(const PolyVox::SimpleVolume<uint8_t>* vol, int x, int y, int z) {
        // x, y, z are in polyvox volume coords
        return !(x < 0 || y < 0 || z < 0 || x >= vol->getWidth() || y >= vol->getHeight() || z >= vol->getDepth());
    }


    bool operator()(PolyVox::SimpleVolume<unsigned char>::Sampler& sampler)
    {
        PolyVox::Vector3DInt32 positionIndex = sampler.getPosition();
        int x = positionIndex.getX();
        int y = positionIndex.getY();
        int z = positionIndex.getZ();

        if (!inBounds(_vol, x, y, z)) {
            return true;
        }

        if (sampler.getVoxel() == 0) {
            return true; // keep raycasting
        }

        _result = glm::vec4((float)x, (float)y, (float)z, 1.0f);
        return false;
    }
    glm::vec4 _result;
    const PolyVox::SimpleVolume<uint8_t>* _vol = nullptr;
};
#endif

bool RenderablePolyVoxEntityItem::findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                                                              const glm::vec3& viewFrustumPos, OctreeElementPointer& element,
                                                              float& distance, BoxFace& face, glm::vec3& surfaceNormal,
                                                              QVariantMap& extraInfo, bool precisionPicking) const {
    // TODO -- correctly pick against marching-cube generated meshes
    if (!precisionPicking) {
        // just intersect with bounding box
        return true;
    }

    glm::mat4 wtvMatrix = worldToVoxelMatrix(true);
    glm::vec3 normDirection = glm::normalize(direction);

    // the PolyVox ray intersection code requires a near and far point.
    // set ray cast length to long enough to cover all of the voxel space
    float distanceToEntity = glm::distance(origin, getWorldPosition());
    glm::vec3 dimensions = getScaledDimensions();
    float largestDimension = glm::compMax(dimensions) * 2.0f;
    glm::vec3 farPoint = origin + normDirection * (distanceToEntity + largestDimension);

    glm::vec4 originInVoxel = wtvMatrix * glm::vec4(origin, 1.0f);
    glm::vec4 farInVoxel = wtvMatrix * glm::vec4(farPoint, 1.0f);

    glm::vec4 result = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    PolyVox::RaycastResult raycastResult = doRayCast(originInVoxel, farInVoxel, result);
    if (raycastResult == PolyVox::RaycastResults::Completed) {
        // the ray completed its path -- nothing was hit.
        return false;
    }

    glm::vec3 result3 = glm::vec3(result);

    AABox voxelBox;
    voxelBox += result3 - Vectors::HALF;
    voxelBox += result3 + Vectors::HALF;

    glm::vec3 directionInVoxel = vec3(wtvMatrix * glm::vec4(direction, 0.0f));
    return voxelBox.findRayIntersection(glm::vec3(originInVoxel), directionInVoxel, 1.0f / directionInVoxel,
                                        distance, face, surfaceNormal);
}

bool RenderablePolyVoxEntityItem::findDetailedParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity,
                                                                   const glm::vec3& acceleration, const glm::vec3& viewFrustumPos, OctreeElementPointer& element,
                                                                   float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal,
                                                                   QVariantMap& extraInfo, bool precisionPicking) const {
    // TODO -- correctly pick against marching-cube generated meshes
    if (!precisionPicking) {
        // just intersect with bounding box
        return true;
    }

    glm::mat4 wtvMatrix = worldToVoxelMatrix(true);
    glm::vec4 originInVoxel = wtvMatrix * glm::vec4(origin, 1.0f);
    glm::vec4 velocityInVoxel = wtvMatrix * glm::vec4(velocity, 0.0f);
    glm::vec4 accelerationInVoxel = wtvMatrix * glm::vec4(acceleration, 0.0f);

    // find the first intersection with the voxel bounding box (slightly enlarged so we can catch voxels that touch the sides)
    bool success;
    glm::vec3 center = getCenterPosition(success);
    glm::vec3 dimensions = getScaledDimensions();
    const float FIRST_BOX_HALF_SCALE = 0.51f;
    AABox voxelBox1(wtvMatrix * vec4(center - FIRST_BOX_HALF_SCALE * dimensions, 1.0f),
                    wtvMatrix * vec4(2.0f * FIRST_BOX_HALF_SCALE * dimensions, 0.0f));
    bool hit1;
    float parabolicDistance1;
    // If we're starting inside the box, our first point is originInVoxel
    if (voxelBox1.contains(originInVoxel)) {
        parabolicDistance1 = 0.0f;
        hit1 = true;
    } else {
        BoxFace face1;
        glm::vec3 surfaceNormal1;
        hit1 = voxelBox1.findParabolaIntersection(glm::vec3(originInVoxel), glm::vec3(velocityInVoxel), glm::vec3(accelerationInVoxel),
                                                  parabolicDistance1, face1, surfaceNormal1);
    }

    if (hit1) {
        // find the second intersection, which should be with the inside of the box (use a slightly large box again)
        const float SECOND_BOX_HALF_SCALE = 0.52f;
        AABox voxelBox2(wtvMatrix * vec4(center - SECOND_BOX_HALF_SCALE * dimensions, 1.0f),
                        wtvMatrix * vec4(2.0f * SECOND_BOX_HALF_SCALE * dimensions, 0.0f));
        glm::vec4 originInVoxel2 = originInVoxel + velocityInVoxel * parabolicDistance1 + 0.5f * accelerationInVoxel * parabolicDistance1 * parabolicDistance1;
        glm::vec4 velocityInVoxel2 = velocityInVoxel + accelerationInVoxel * parabolicDistance1;
        glm::vec4 accelerationInVoxel2 = accelerationInVoxel;
        float parabolicDistance2;
        BoxFace face2;
        glm::vec3 surfaceNormal2;
        // this should always be true
        if (voxelBox2.findParabolaIntersection(glm::vec3(originInVoxel2), glm::vec3(velocityInVoxel2), glm::vec3(accelerationInVoxel2),
                                               parabolicDistance2, face2, surfaceNormal2)) {
            const int MAX_SECTIONS = 15;
            PolyVox::RaycastResult raycastResult = PolyVox::RaycastResults::Completed;
            glm::vec4 result = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            glm::vec4 segmentStartVoxel = originInVoxel2;
            for (int i = 0; i < MAX_SECTIONS; i++) {
                float t = parabolicDistance2 * ((float)(i + 1)) / ((float)MAX_SECTIONS);
                glm::vec4 segmentEndVoxel = originInVoxel2 + velocityInVoxel2 * t + 0.5f * accelerationInVoxel2 * t * t;
                raycastResult = doRayCast(segmentStartVoxel, segmentEndVoxel, result);
                if (raycastResult != PolyVox::RaycastResults::Completed) {
                    // We hit something!
                    break;
                }
                segmentStartVoxel = segmentEndVoxel;
            }

            if (raycastResult == PolyVox::RaycastResults::Completed) {
                // the parabola completed its path -- nothing was hit.
                return false;
            }

            glm::vec3 result3 = glm::vec3(result);

            AABox voxelBox;
            voxelBox += result3 - Vectors::HALF;
            voxelBox += result3 + Vectors::HALF;

            return voxelBox.findParabolaIntersection(glm::vec3(originInVoxel), glm::vec3(velocityInVoxel), glm::vec3(accelerationInVoxel),
                                                     parabolicDistance, face, surfaceNormal);
        }
    }
    return false;
}

PolyVox::RaycastResult RenderablePolyVoxEntityItem::doRayCast(glm::vec4 originInVoxel,
                                                              glm::vec4 farInVoxel,
                                                              glm::vec4& result) const {
    PolyVox::Vector3DFloat startPoint(originInVoxel.x, originInVoxel.y, originInVoxel.z);
    PolyVox::Vector3DFloat endPoint(farInVoxel.x, farInVoxel.y, farInVoxel.z);

    PolyVox::RaycastResult raycastResult;
    withReadLock([&] {
        RaycastFunctor callback(_volData);
        raycastResult = PolyVox::raycastWithEndpoints(_volData.get(), startPoint, endPoint, callback);

        // result is in voxel-space coordinates.
        result = callback._result;
    });

    return raycastResult;
}


// virtual
ShapeType RenderablePolyVoxEntityItem::getShapeType() const {
    if (_collisionless) {
        return SHAPE_TYPE_NONE;
    }
    return SHAPE_TYPE_COMPOUND;
}

void RenderablePolyVoxEntityItem::setRegistrationPoint(const glm::vec3& value) {
    if (value != getRegistrationPoint()) {
        withWriteLock([&] {
            _shapeReady = false;
        });
        EntityItem::setRegistrationPoint(value);
        startUpdates();
    }
}

bool RenderablePolyVoxEntityItem::isReadyToComputeShape() const {
    ShapeType shapeType = getShapeType();
    if (shapeType == SHAPE_TYPE_NONE) {
        return true;
    }

    bool result;
    withReadLock([&] {
        result = _shapeReady;
    });
    return result;
}

void RenderablePolyVoxEntityItem::computeShapeInfo(ShapeInfo& info) {
    ShapeType shapeType = getShapeType();
    if (shapeType == SHAPE_TYPE_NONE) {
        info.setParams(getShapeType(), 0.5f * getScaledDimensions());
        return;
    }

    withReadLock([&] {
        info = _shapeInfo;
    });
}

void RenderablePolyVoxEntityItem::changeUpdates(bool value) {
    if (_updateNeeded != value) {
        EntityTreePointer entityTree = getTree();
        if (entityTree) {
            EntitySimulationPointer simulation = entityTree->getSimulation();
            if (simulation) {
                _updateNeeded = value;
                _flags |= Simulation::DIRTY_UPDATEABLE;
                simulation->changeEntity(getThisPointer());
            }
        }
    }
}

void RenderablePolyVoxEntityItem::startUpdates() {
    changeUpdates(true);
}

void RenderablePolyVoxEntityItem::stopUpdates() {
    changeUpdates(false);
}

void RenderablePolyVoxEntityItem::update(const quint64& now) {
    bool doRecomputeMesh { false };
    bool doUncompress { false };
    bool doCompress { false };
    bool doRecomputeShape { false };

    withWriteLock([&] {
        tellNeighborsToRecopyEdges(false);

        switch (_state) {

            case PolyVoxState::Ready: {
                if (_volDataDirty) {
                    _volDataDirty = _voxelDataDirty = false;
                    _state = PolyVoxState::BakingMesh;
                    doRecomputeMesh = true;
                } else if (_voxelDataDirty) {
                    _voxelDataDirty = false;
                    _state = PolyVoxState::Uncompressing;
                    doUncompress = true;
                } else {
                    copyUpperEdgesFromNeighbors();
                    if (!_volDataDirty && !_voxelDataDirty) {
                        // nothing to do
                        stopUpdates();
                    }
                }
                break;
            }

            case PolyVoxState::Uncompressing: {
                break; // wait
            }
            case PolyVoxState::UncompressingFinished: {
                if (_volDataDirty) {
                    _volDataDirty = _voxelDataDirty = false;
                    _state = PolyVoxState::BakingMeshNoCompress;
                    doRecomputeMesh = true;
                } else if (_voxelDataDirty) {
                    _voxelDataDirty = false;
                    // _voxelData changed while we were uncompressing the previous version, uncompress again
                    _state = PolyVoxState::Uncompressing;
                    doUncompress = true;
                } else {
                    _state = PolyVoxState::Ready;
                }
                break;
            }

            case PolyVoxState::BakingMesh: {
                break; // wait
            }
            case PolyVoxState::BakingMeshFinished: {
                if (_volDataDirty) {
                    _volDataDirty = _voxelDataDirty = false;
                    _state = PolyVoxState::BakingMesh;
                    // a local edit happened while we were baking the mesh.  rebake mesh...
                    doRecomputeMesh = true;
                } else if (_voxelDataDirty) {
                    _voxelDataDirty = false;
                    // we received a change from the wire while baking the mesh.
                    _state = PolyVoxState::Uncompressing;
                    doUncompress = true;
                } else {
                    _state = PolyVoxState::Compressing;
                    doCompress = true;
                }
                break;
            }

            case PolyVoxState::BakingMeshNoCompress: {
                break; // wait
            }
            case PolyVoxState::BakingMeshNoCompressFinished: {
                if (_volDataDirty) {
                    _volDataDirty = _voxelDataDirty = false;
                    _state = PolyVoxState::BakingMesh;
                    // a local edit happened while we were baking the mesh.  rebake mesh...
                    doRecomputeMesh = true;
                } else if (_voxelDataDirty) {
                    _voxelDataDirty = false;
                    // we received a change from the wire while baking the mesh.
                    _state = PolyVoxState::Uncompressing;
                    doUncompress = true;
                } else {
                    _state = PolyVoxState::BakingShape;
                    doRecomputeShape = true;
                }
                break;
            }

            case PolyVoxState::Compressing: {
                break; // wait
            }
            case PolyVoxState::CompressingFinished: {
                _state = PolyVoxState::BakingShape;
                doRecomputeShape = true;
                break;
            }

            case PolyVoxState::BakingShape: {
                break; // wait
            }
            case PolyVoxState::BakingShapeFinished: {
                _state = PolyVoxState::Ready;
                break;
            }
        }
    });

    if (doRecomputeMesh) {
        recomputeMesh();
    }
    if (doUncompress) {
        uncompressVolumeData();
    }
    if (doCompress) {
        compressVolumeDataAndSendEditPacket();
    }
    if (doRecomputeShape) {
        computeShapeInfoWorker();
    }
}

void RenderablePolyVoxEntityItem::setVoxelVolumeSize(const glm::vec3& voxelVolumeSize) {
    // This controls how many individual voxels are in the entity.  This is unrelated to
    // the dimentions of the entity -- it defines the sizes of the arrays that hold voxel values.
    // In addition to setting the number of voxels, this is used in a few places for its
    // side-effect of allocating _volData to be the correct size.
    withWriteLock([&] {
        if (_volData && _voxelVolumeSize == voxelVolumeSize) {
            return;
        }

        _voxelDataDirty = true;
        _voxelVolumeSize = voxelVolumeSize;
        _volData.reset();
        _onCount = 0;
        _updateFromNeighborXEdge = _updateFromNeighborYEdge = _updateFromNeighborZEdge = true;
        startUpdates();

        static const PolyVox::Vector3DInt32 lowCorner(0, 0, 0);
        PolyVox::Vector3DInt32 highCorner;

        if (isEdged()) {
            // with _EDGED_ we maintain an extra box of voxels around those that the user asked for.  This
            // changes how the surface extractor acts -- it becomes impossible to have holes in the
            // generated mesh.  The non _EDGED_ modes will leave holes in the mesh at the edges of the
            // voxel space.
            highCorner = PolyVox::Vector3DInt32(_voxelVolumeSize.x + 1, // corners are inclusive
                                                _voxelVolumeSize.y + 1,
                                                _voxelVolumeSize.z + 1);
        } else {
            // these should each have -1 after them, but if we leave layers on the upper-axis faces,
            // they act more like I expect.
            highCorner = PolyVox::Vector3DInt32(_voxelVolumeSize.x,
                                                _voxelVolumeSize.y,
                                                _voxelVolumeSize.z);
        }

        _volData.reset(new PolyVox::SimpleVolume<uint8_t>(PolyVox::Region(lowCorner, highCorner)));
        // having the "outside of voxel-space" value be 255 has helped me notice some problems.
        _volData->setBorderValue(255);
    });

    tellNeighborsToRecopyEdges(true);
}

bool inUserBounds(const std::shared_ptr<PolyVox::SimpleVolume<uint8_t>> vol,
                  PolyVoxEntityItem::PolyVoxSurfaceStyle surfaceStyle,
                  const ivec3& v) {
    if (glm::any(glm::lessThan(v, ivec3(0)))) {
        return false;
    }

    ivec3 volume { vol->getWidth(), vol->getHeight(), vol->getDepth() };
    // x, y, z are in user voxel-coords, not adjusted-for-edge voxel-coords.
    if (PolyVoxEntityItem::isEdged(surfaceStyle)) {
        volume -= 2;
    }

    if (glm::any(glm::greaterThanEqual(v, volume))) {
        return false;
    }

    return true;
}


uint8_t RenderablePolyVoxEntityItem::getVoxel(const ivec3& v) const {
    uint8_t result;
    withReadLock([&] {
        result = getVoxelInternal(v);
    });
    return result;
}


uint8_t RenderablePolyVoxEntityItem::getVoxelInternal(const ivec3& v) const {
    if (!inUserBounds(_volData, _voxelSurfaceStyle, v)) {
        return 0;
    }

    // if _voxelSurfaceStyle is *_EDGED_*, we maintain an extra layer of
    // voxels all around the requested voxel space.  Having the empty voxels around
    // the edges changes how the surface extractor behaves.
    if (isEdged()) {
        return _volData->getVoxelAt(v.x + 1, v.y + 1, v.z + 1);
    }
    return _volData->getVoxelAt(v.x, v.y, v.z);
}


void RenderablePolyVoxEntityItem::setVoxelMarkNeighbors(int x, int y, int z, uint8_t toValue) {
    _volData->setVoxelAt(x, y, z, toValue);
    if (x == 0) {
        _neighborXNeedsUpdate = true;
        startUpdates();
    }
    if (y == 0) {
        _neighborYNeedsUpdate = true;
        startUpdates();
    }
    if (z == 0) {
        _neighborZNeedsUpdate = true;
        startUpdates();
    }
}

bool RenderablePolyVoxEntityItem::setVoxelInternal(const ivec3& v, uint8_t toValue) {
    // set a voxel without recompressing the voxel data.  This assumes that the caller has write-locked the entity.
    bool result = updateOnCount(v, toValue);
    if (result) {
        if (isEdged()) {
            setVoxelMarkNeighbors(v.x + 1, v.y + 1, v.z + 1, toValue);
        } else {
            setVoxelMarkNeighbors(v.x, v.y, v.z, toValue);
        }
        _volDataDirty = true;
        startUpdates();
    }

    return result;
}

bool RenderablePolyVoxEntityItem::updateOnCount(const ivec3& v, uint8_t toValue) {
    // keep _onCount up to date
    if (!inUserBounds(_volData, _voxelSurfaceStyle, v)) {
        return false;
    }

    uint8_t uVoxelValue = getVoxelInternal(v);
    if (toValue != 0) {
        if (uVoxelValue == 0) {
            _onCount++;
            return true;
        }
    } else {
        // toValue == 0
        if (uVoxelValue != 0) {
            _onCount--;
            assert(_onCount >= 0);
            return true;
        }
    }
    return false;
}

void RenderablePolyVoxEntityItem::uncompressVolumeData() {
    // take compressed data and expand it into _volData.
    QByteArray voxelData;
    auto entity = std::static_pointer_cast<RenderablePolyVoxEntityItem>(getThisPointer());

    withReadLock([&] {
        voxelData = _voxelData;
    });

    QtConcurrent::run([=] {
        QDataStream reader(voxelData);
        quint16 voxelXSize, voxelYSize, voxelZSize;
        reader >> voxelXSize;
        reader >> voxelYSize;
        reader >> voxelZSize;

        if (voxelXSize == 0 || voxelXSize > PolyVoxEntityItem::MAX_VOXEL_DIMENSION ||
            voxelYSize == 0 || voxelYSize > PolyVoxEntityItem::MAX_VOXEL_DIMENSION ||
            voxelZSize == 0 || voxelZSize > PolyVoxEntityItem::MAX_VOXEL_DIMENSION) {
            qCDebug(entitiesrenderer) << "voxelSize is not reasonable, skipping uncompressions."
                                      << voxelXSize << voxelYSize << voxelZSize << getName() << getID();
            entity->setVoxelsFromData(QByteArray(1, 0), 1, 1, 1);
            return;
        }
        int rawSize = voxelXSize * voxelYSize * voxelZSize;

        QByteArray compressedData;
        reader >> compressedData;

        QByteArray uncompressedData = qUncompress(compressedData);

        if (uncompressedData.size() != rawSize) {
            qCDebug(entitiesrenderer) << "PolyVox uncompress -- size is (" << voxelXSize << voxelYSize << voxelZSize << ")"
                     << "so expected uncompressed length of" << rawSize << "but length is" << uncompressedData.size()
                                      << getName() << getID();
            entity->setVoxelsFromData(QByteArray(1, 0), 1, 1, 1);
            return;
        }

        entity->setVoxelsFromData(uncompressedData, voxelXSize, voxelYSize, voxelZSize);
    });
}

void RenderablePolyVoxEntityItem::setVoxelsFromData(QByteArray uncompressedData,
                                                    quint16 voxelXSize, quint16 voxelYSize, quint16 voxelZSize) {
    // this accepts the payload from uncompressVolumeData
    withWriteLock([&] {
        loop3(ivec3(0), ivec3(voxelXSize, voxelYSize, voxelZSize), [&](const ivec3& v) {
            int uncompressedIndex = (v.z * voxelYSize * voxelXSize) + (v.y * voxelZSize) + v.x;
            setVoxelInternal(v, uncompressedData[uncompressedIndex]);
        });

        _state = PolyVoxState::UncompressingFinished;
    });
}

void RenderablePolyVoxEntityItem::compressVolumeDataAndSendEditPacket() {
    // compress the data in _volData and save the results.  The compressed form is used during
    // saves to disk and for transmission over the wire to the entity-server

    EntityItemPointer entity = getThisPointer();

    quint16 voxelXSize;
    quint16 voxelYSize;
    quint16 voxelZSize;
    withReadLock([&] {
        voxelXSize = _voxelVolumeSize.x;
        voxelYSize = _voxelVolumeSize.y;
        voxelZSize = _voxelVolumeSize.z;
    });

    QtConcurrent::run([voxelXSize, voxelYSize, voxelZSize, entity] {
        auto polyVoxEntity = std::static_pointer_cast<RenderablePolyVoxEntityItem>(entity);
        QByteArray uncompressedData = polyVoxEntity->volDataToArray(voxelXSize, voxelYSize, voxelZSize);

        QByteArray newVoxelData;
        QDataStream writer(&newVoxelData, QIODevice::WriteOnly | QIODevice::Truncate);

        writer << voxelXSize << voxelYSize << voxelZSize;

        QByteArray compressedData = qCompress(uncompressedData, 9);
        writer << compressedData;

        // make sure the compressed data can be sent over the wire-protocol
        if (newVoxelData.size() > 1150) {
            // HACK -- until we have a way to allow for properties larger than MTU, don't update.
            // revert the active voxel-space to the last version that fit.
            qCDebug(entitiesrenderer) << "compressed voxel data is too large" << entity->getName() << entity->getID();

            const auto polyVoxEntity = std::static_pointer_cast<RenderablePolyVoxEntityItem>(entity);
            polyVoxEntity->compressVolumeDataFinished(QByteArray());
            return;
        }

        std::static_pointer_cast<RenderablePolyVoxEntityItem>(entity)->compressVolumeDataFinished(newVoxelData);
    });
}

void RenderablePolyVoxEntityItem::compressVolumeDataFinished(const QByteArray& voxelData) {
    // compressed voxel information from the entity-server
    withWriteLock([&] {
        if (voxelData.size() > 0 && _voxelData != voxelData) {
            _voxelData = voxelData;
        }
        _state = PolyVoxState::CompressingFinished;
    });

    auto now = usecTimestampNow();
    setLastEdited(now);
    setLastBroadcast(now);

    EntityTreeElementPointer element = getElement();
    EntityTreePointer tree = element ? element->getTree() : nullptr;

    if (tree) {
        tree->withReadLock([&] {
            EntityPropertyFlags desiredProperties;
            desiredProperties.setHasProperty(PROP_VOXEL_DATA);
            EntityItemProperties properties = getProperties(desiredProperties, false);
            properties.setVoxelDataDirty();
            properties.setLastEdited(now);

            EntitySimulationPointer simulation = tree ? tree->getSimulation() : nullptr;
            PhysicalEntitySimulationPointer peSimulation = std::static_pointer_cast<PhysicalEntitySimulation>(simulation);
            EntityEditPacketSender* packetSender = peSimulation ? peSimulation->getPacketSender() : nullptr;
            if (packetSender) {
                packetSender->queueEditEntityMessage(PacketType::EntityEdit, tree, getID(), properties);
            }
        });
    }
}


EntityItemPointer lookUpNeighbor(EntityTreePointer tree, EntityItemID neighborID, EntityItemWeakPointer& currentWP) {
    EntityItemPointer current = currentWP.lock();

    if (!current && neighborID == UNKNOWN_ENTITY_ID) {
        // no neighbor
        return nullptr;
    }

    if (current && current->getID() == neighborID) {
        // same neighbor
        return current;
    }

    if (neighborID == UNKNOWN_ENTITY_ID) {
        currentWP.reset();
        return nullptr;
    }

    current = tree->findEntityByID(neighborID);
    if (!current) {
        return nullptr;
    }

    currentWP = current;
    return current;
}

void RenderablePolyVoxEntityItem::cacheNeighbors() {
    // this attempts to turn neighbor entityIDs into neighbor weak-pointers
    EntityTreeElementPointer element = getElement();
    EntityTreePointer tree = element ? element->getTree() : nullptr;
    if (!tree) {
        return;
    }
    lookUpNeighbor(tree, _xNNeighborID, _xNNeighbor);
    lookUpNeighbor(tree, _yNNeighborID, _yNNeighbor);
    lookUpNeighbor(tree, _zNNeighborID, _zNNeighbor);
    lookUpNeighbor(tree, _xPNeighborID, _xPNeighbor);
    lookUpNeighbor(tree, _yPNeighborID, _yPNeighbor);
    lookUpNeighbor(tree, _zPNeighborID, _zPNeighbor);
}

void RenderablePolyVoxEntityItem::copyUpperEdgesFromNeighbors() {
    // fill in our upper edges with a copy of our neighbors lower edges so that the meshes knit together
    if (_voxelSurfaceStyle != PolyVoxEntityItem::SURFACE_MARCHING_CUBES) {
        return;
    }

    if (!_updateFromNeighborXEdge && !_updateFromNeighborYEdge && !_updateFromNeighborZEdge) {
        return;
    }

    cacheNeighbors();

    if (_updateFromNeighborXEdge) {
        _updateFromNeighborXEdge = false;
        auto currentXPNeighbor = getXPNeighbor();
        if (currentXPNeighbor && currentXPNeighbor->getVoxelVolumeSize() == _voxelVolumeSize) {
            withWriteLock([&] {
                int x = _volData->getWidth() - 1;
                for (int y = 0; y < _volData->getHeight(); y++) {
                    for (int z = 0; z < _volData->getDepth(); z++) {
                        uint8_t neighborValue = currentXPNeighbor->getVoxel({ 0, y, z });
                        uint8_t prevValue = _volData->getVoxelAt(x, y, z);
                        if (prevValue != neighborValue) {
                            _volData->setVoxelAt(x, y, z, neighborValue);
                            _volDataDirty = true;
                        }
                    }
                }
            });
        }
    }

    if (_updateFromNeighborYEdge) {
        _updateFromNeighborYEdge = false;
        auto currentYPNeighbor = getYPNeighbor();
        if (currentYPNeighbor && currentYPNeighbor->getVoxelVolumeSize() == _voxelVolumeSize) {
            withWriteLock([&] {
                int y = _volData->getHeight() - 1;
                for (int x = 0; x < _volData->getWidth(); x++) {
                    for (int z = 0; z < _volData->getDepth(); z++) {
                        uint8_t neighborValue = currentYPNeighbor->getVoxel({ x, 0, z });
                        uint8_t prevValue = _volData->getVoxelAt(x, y, z);
                        if (prevValue != neighborValue) {
                            _volData->setVoxelAt(x, y, z, neighborValue);
                            _volDataDirty = true;
                        }
                    }
                }
            });
        }
    }

    if (_updateFromNeighborZEdge) {
        _updateFromNeighborZEdge = false;
        auto currentZPNeighbor = getZPNeighbor();
        if (currentZPNeighbor && currentZPNeighbor->getVoxelVolumeSize() == _voxelVolumeSize) {
            withWriteLock([&] {
                int z = _volData->getDepth() - 1;
                for (int x = 0; x < _volData->getWidth(); x++) {
                    for (int y = 0; y < _volData->getHeight(); y++) {
                        uint8_t neighborValue = currentZPNeighbor->getVoxel({ x, y, 0 });
                        uint8_t prevValue = _volData->getVoxelAt(x, y, z);
                        if (prevValue != neighborValue) {
                            _volData->setVoxelAt(x, y, z, neighborValue);
                            _volDataDirty = true;
                        }
                    }
                }
            });
        }
    }
}

void RenderablePolyVoxEntityItem::tellNeighborsToRecopyEdges(bool force) {
    // if this polyvox has changed any of its voxels with a zero coord (in x, y, or z) notify neighbors, if there are any
    if (force || _neighborXNeedsUpdate || _neighborYNeedsUpdate || _neighborZNeedsUpdate) {
        cacheNeighbors();

        if (force || _neighborXNeedsUpdate) {
            _neighborXNeedsUpdate = false;
            auto currentXNNeighbor = getXNNeighbor();
            if (currentXNNeighbor) {
                currentXNNeighbor->neighborXEdgeChanged();
            }
        }
        if (force || _neighborYNeedsUpdate) {
            _neighborYNeedsUpdate = false;
            auto currentYNNeighbor = getYNNeighbor();
            if (currentYNNeighbor) {
                currentYNNeighbor->neighborYEdgeChanged();
            }
        }
        if (force || _neighborZNeedsUpdate) {
            _neighborZNeedsUpdate = false;
            auto currentZNNeighbor = getZNNeighbor();
            if (currentZNNeighbor) {
                currentZNNeighbor->neighborZEdgeChanged();
            }
        }
    }
}


void RenderablePolyVoxEntityItem::recomputeMesh() {
    // use _volData to make a renderable mesh
    PolyVoxSurfaceStyle voxelSurfaceStyle;
    withReadLock([&] {
        voxelSurfaceStyle = _voxelSurfaceStyle;
    });

    auto entity = std::static_pointer_cast<RenderablePolyVoxEntityItem>(getThisPointer());

    QtConcurrent::run([entity, voxelSurfaceStyle] {
        graphics::MeshPointer mesh(new graphics::Mesh());

        // A mesh object to hold the result of surface extraction
        PolyVox::SurfaceMesh<PolyVox::PositionMaterialNormal> polyVoxMesh;

        entity->withReadLock([&] {
            PolyVox::SimpleVolume<uint8_t>* volData = entity->getVolData();
            switch (voxelSurfaceStyle) {
                case PolyVoxEntityItem::SURFACE_EDGED_MARCHING_CUBES:
                case PolyVoxEntityItem::SURFACE_MARCHING_CUBES: {
                    PolyVox::MarchingCubesSurfaceExtractor<PolyVox::SimpleVolume<uint8_t>> surfaceExtractor
                        (volData, volData->getEnclosingRegion(), &polyVoxMesh);
                    surfaceExtractor.execute();
                    break;
                }
                case PolyVoxEntityItem::SURFACE_EDGED_CUBIC:
                case PolyVoxEntityItem::SURFACE_CUBIC: {
                    PolyVox::CubicSurfaceExtractorWithNormals<PolyVox::SimpleVolume<uint8_t>> surfaceExtractor
                        (volData, volData->getEnclosingRegion(), &polyVoxMesh);
                    surfaceExtractor.execute();
                    break;
                }
            }
        });

        // convert PolyVox mesh to a Sam mesh
        const std::vector<uint32_t>& vecIndices = polyVoxMesh.getIndices();
        auto indexBuffer = std::make_shared<gpu::Buffer>(vecIndices.size() * sizeof(uint32_t),
                                                         (gpu::Byte*)vecIndices.data());
        auto indexBufferPtr = gpu::BufferPointer(indexBuffer);
        gpu::BufferView indexBufferView(indexBufferPtr, gpu::Element(gpu::SCALAR, gpu::UINT32, gpu::INDEX));
        mesh->setIndexBuffer(indexBufferView);

        const std::vector<PolyVox::PositionMaterialNormal>& vecVertices = polyVoxMesh.getRawVertexData();
        auto vertexBuffer = std::make_shared<gpu::Buffer>(vecVertices.size() * sizeof(PolyVox::PositionMaterialNormal),
                                                          (gpu::Byte*)vecVertices.data());
        auto vertexBufferPtr = gpu::BufferPointer(vertexBuffer);
        gpu::BufferView vertexBufferView(vertexBufferPtr, 0,
                                         vertexBufferPtr->getSize(),
                                         sizeof(PolyVox::PositionMaterialNormal),
                                         gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
        mesh->setVertexBuffer(vertexBufferView);

        // TODO -- use 3-byte normals rather than 3-float normals
        mesh->addAttribute(gpu::Stream::NORMAL,
                           gpu::BufferView(vertexBufferPtr,
                                           sizeof(float) * 3, // polyvox mesh is packed: position, normal, material
                                           vertexBufferPtr->getSize(),
                                           sizeof(PolyVox::PositionMaterialNormal),
                                           gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ)));

        std::vector<graphics::Mesh::Part> parts;
        parts.emplace_back(graphics::Mesh::Part((graphics::Index)0, // startIndex
                                             (graphics::Index)vecIndices.size(), // numIndices
                                             (graphics::Index)0, // baseVertex
                                             graphics::Mesh::TRIANGLES)); // topology
        mesh->setPartBuffer(gpu::BufferView(new gpu::Buffer(parts.size() * sizeof(graphics::Mesh::Part), (gpu::Byte*) parts.data()),
                                            gpu::Element::PART_DRAWCALL));
        entity->setMesh(mesh);
    });
}

void RenderablePolyVoxEntityItem::setMesh(graphics::MeshPointer mesh) {
    // this catches the payload from recomputeMesh
    withWriteLock([&] {
        if (!_collisionless) {
            _flags |= Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS;
        }
        _shapeReady = false;
        _mesh = mesh;
        _state = PolyVoxState::BakingMeshFinished;
        _meshReady = true;
        startUpdates();
    });

    somethingChangedNotification();
}

void RenderablePolyVoxEntityItem::computeShapeInfoWorker() {
    // this creates a collision-shape for the physics engine.  The shape comes from
    // _volData for cubic extractors and from _mesh for marching-cube extractors

    EntityItemPointer entity = getThisPointer();

    PolyVoxSurfaceStyle voxelSurfaceStyle;
    glm::vec3 voxelVolumeSize;
    graphics::MeshPointer mesh;

    withReadLock([&] {
        voxelSurfaceStyle = _voxelSurfaceStyle;
        voxelVolumeSize = _voxelVolumeSize;
        mesh = _mesh;
    });

    QtConcurrent::run([entity, voxelSurfaceStyle, voxelVolumeSize, mesh] {
        auto polyVoxEntity = std::static_pointer_cast<RenderablePolyVoxEntityItem>(entity);
        QVector<QVector<glm::vec3>> pointCollection;
        AABox box;
        glm::mat4 vtoM = std::static_pointer_cast<RenderablePolyVoxEntityItem>(entity)->voxelToLocalMatrix();

        if (voxelSurfaceStyle == PolyVoxEntityItem::SURFACE_MARCHING_CUBES ||
            voxelSurfaceStyle == PolyVoxEntityItem::SURFACE_EDGED_MARCHING_CUBES) {
            // pull each triangle in the mesh into a polyhedron which can be collided with
            unsigned int i = 0;

            const gpu::BufferView& vertexBufferView = mesh->getVertexBuffer();
            const gpu::BufferView& indexBufferView = mesh->getIndexBuffer();

            gpu::BufferView::Iterator<const uint32_t> it = indexBufferView.cbegin<uint32_t>();
            while (it != indexBufferView.cend<uint32_t>()) {
                uint32_t p0Index = *(it++);
                uint32_t p1Index = *(it++);
                uint32_t p2Index = *(it++);

                const glm::vec3& p0 = vertexBufferView.get<const glm::vec3>(p0Index);
                const glm::vec3& p1 = vertexBufferView.get<const glm::vec3>(p1Index);
                const glm::vec3& p2 = vertexBufferView.get<const glm::vec3>(p2Index);

                glm::vec3 av = (p0 + p1 + p2) / 3.0f; // center of the triangular face
                glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                glm::vec3 p3 = av - normal * MARCHING_CUBE_COLLISION_HULL_OFFSET;

                glm::vec3 p0Model = glm::vec3(vtoM * glm::vec4(p0, 1.0f));
                glm::vec3 p1Model = glm::vec3(vtoM * glm::vec4(p1, 1.0f));
                glm::vec3 p2Model = glm::vec3(vtoM * glm::vec4(p2, 1.0f));
                glm::vec3 p3Model = glm::vec3(vtoM * glm::vec4(p3, 1.0f));

                box += p0Model;
                box += p1Model;
                box += p2Model;
                box += p3Model;

                QVector<glm::vec3> pointsInPart;
                pointsInPart << p0Model;
                pointsInPart << p1Model;
                pointsInPart << p2Model;
                pointsInPart << p3Model;
                // add next convex hull
                QVector<glm::vec3> newMeshPoints;
                pointCollection << newMeshPoints;
                // add points to the new convex hull
                pointCollection[i++] << pointsInPart;
            }
        } else {
            unsigned int i = 0;
            polyVoxEntity->forEachVoxelValue(voxelVolumeSize, [&](const ivec3& v, uint8_t value) {
                if (value > 0) {
                    const auto& x = v.x;
                    const auto& y = v.y;
                    const auto& z = v.z;
                    if (glm::all(glm::greaterThan(v, ivec3(0))) &&
                        glm::all(glm::lessThan(v, ivec3(voxelVolumeSize) - 1)) &&
                        (polyVoxEntity->getVoxelInternal({ x - 1, y, z }) > 0) &&
                        (polyVoxEntity->getVoxelInternal({ x, y - 1, z }) > 0) &&
                        (polyVoxEntity->getVoxelInternal({ x, y, z - 1 }) > 0) &&
                        (polyVoxEntity->getVoxelInternal({ x + 1, y, z }) > 0) &&
                        (polyVoxEntity->getVoxelInternal({ x, y + 1, z }) > 0) &&
                        (polyVoxEntity->getVoxelInternal({ x, y, z + 1 }) > 0)) {
                        // this voxel has neighbors in every cardinal direction, so there's no need
                        // to include it in the collision hull.
                        return;
                    }

                    QVector<glm::vec3> pointsInPart;

                    float offL = -0.5f;
                    float offH = 0.5f;
                    if (voxelSurfaceStyle == PolyVoxEntityItem::SURFACE_EDGED_CUBIC) {
                        offL += 1.0f;
                        offH += 1.0f;
                    }

                    glm::vec3 p000 = glm::vec3(vtoM * glm::vec4(x + offL, y + offL, z + offL, 1.0f));
                    glm::vec3 p001 = glm::vec3(vtoM * glm::vec4(x + offL, y + offL, z + offH, 1.0f));
                    glm::vec3 p010 = glm::vec3(vtoM * glm::vec4(x + offL, y + offH, z + offL, 1.0f));
                    glm::vec3 p011 = glm::vec3(vtoM * glm::vec4(x + offL, y + offH, z + offH, 1.0f));
                    glm::vec3 p100 = glm::vec3(vtoM * glm::vec4(x + offH, y + offL, z + offL, 1.0f));
                    glm::vec3 p101 = glm::vec3(vtoM * glm::vec4(x + offH, y + offL, z + offH, 1.0f));
                    glm::vec3 p110 = glm::vec3(vtoM * glm::vec4(x + offH, y + offH, z + offL, 1.0f));
                    glm::vec3 p111 = glm::vec3(vtoM * glm::vec4(x + offH, y + offH, z + offH, 1.0f));

                    box += p000;
                    box += p001;
                    box += p010;
                    box += p011;
                    box += p100;
                    box += p101;
                    box += p110;
                    box += p111;

                    pointsInPart << p000;
                    pointsInPart << p001;
                    pointsInPart << p010;
                    pointsInPart << p011;
                    pointsInPart << p100;
                    pointsInPart << p101;
                    pointsInPart << p110;
                    pointsInPart << p111;

                    // add next convex hull
                    QVector<glm::vec3> newMeshPoints;
                    pointCollection << newMeshPoints;
                    // add points to the new convex hull
                    pointCollection[i++] << pointsInPart;
                }
            });
        }
        polyVoxEntity->setCollisionPoints(pointCollection, box);
    });
}

void RenderablePolyVoxEntityItem::setCollisionPoints(ShapeInfo::PointCollection pointCollection, AABox box) {
    // this catches the payload from computeShapeInfoWorker
    if (pointCollection.isEmpty()) {
        EntityItem::computeShapeInfo(_shapeInfo);
        withWriteLock([&] {
            _shapeReady = true;
            _state = PolyVoxState::BakingShapeFinished;
        });
        return;
    }

    glm::vec3 collisionModelDimensions = box.getDimensions();
    // include the registrationPoint in the shape key, because the offset is already
    // included in the points and the shapeManager wont know that the shape has changed.
    withWriteLock([&] {
        QString shapeKey = QString(_voxelData.toBase64()) + "," +
            QString::number(_registrationPoint.x) + "," +
            QString::number(_registrationPoint.y) + "," +
            QString::number(_registrationPoint.z);
        _shapeInfo.setParams(SHAPE_TYPE_COMPOUND, collisionModelDimensions, shapeKey);
        _shapeInfo.setPointCollection(pointCollection);
        _shapeReady = true;
        _state = PolyVoxState::BakingShapeFinished;
    });
}

void RenderablePolyVoxEntityItem::setXNNeighborID(const EntityItemID& xNNeighborID) {
    if (xNNeighborID == _id) { // TODO loops are still possible
        return;
    }

    if (xNNeighborID != _xNNeighborID) {
        PolyVoxEntityItem::setXNNeighborID(xNNeighborID);
        _neighborXNeedsUpdate = true;
        startUpdates();
    }
}

void RenderablePolyVoxEntityItem::setYNNeighborID(const EntityItemID& yNNeighborID) {
    if (yNNeighborID == _id) { // TODO loops are still possible
        return;
    }

    if (yNNeighborID != _yNNeighborID) {
        PolyVoxEntityItem::setYNNeighborID(yNNeighborID);
        _neighborYNeedsUpdate = true;
        startUpdates();
    }
}

void RenderablePolyVoxEntityItem::setZNNeighborID(const EntityItemID& zNNeighborID) {
    if (zNNeighborID == _id) { // TODO loops are still possible
        return;
    }

    if (zNNeighborID != _zNNeighborID) {
        PolyVoxEntityItem::setZNNeighborID(zNNeighborID);
        _neighborZNeedsUpdate = true;
        startUpdates();
    }
}

void RenderablePolyVoxEntityItem::setXPNeighborID(const EntityItemID& xPNeighborID) {
    if (xPNeighborID == _id) { // TODO loops are still possible
        return;
    }
    if (xPNeighborID != _xPNeighborID) {
        PolyVoxEntityItem::setXPNeighborID(xPNeighborID);
        _updateFromNeighborXEdge = true;
        startUpdates();
    }
}

void RenderablePolyVoxEntityItem::setYPNeighborID(const EntityItemID& yPNeighborID) {
    if (yPNeighborID == _id) { // TODO loops are still possible
        return;
    }
    if (yPNeighborID != _yPNeighborID) {
        PolyVoxEntityItem::setYPNeighborID(yPNeighborID);
        _updateFromNeighborYEdge = true;
        startUpdates();
    }
}

void RenderablePolyVoxEntityItem::setZPNeighborID(const EntityItemID& zPNeighborID) {
    if (zPNeighborID == _id) { // TODO loops are still possible
        return;
    }
    if (zPNeighborID != _zPNeighborID) {
        PolyVoxEntityItem::setZPNeighborID(zPNeighborID);
        _updateFromNeighborZEdge = true;
        startUpdates();
    }
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getXNNeighbor() {
    return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_xNNeighbor.lock());
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getYNNeighbor() {
        return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_yNNeighbor.lock());
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getZNNeighbor() {
    return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_zNNeighbor.lock());
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getXPNeighbor() {
    return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_xPNeighbor.lock());
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getYPNeighbor() {
    return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_yPNeighbor.lock());
}

std::shared_ptr<RenderablePolyVoxEntityItem> RenderablePolyVoxEntityItem::getZPNeighbor() {
    return std::dynamic_pointer_cast<RenderablePolyVoxEntityItem>(_zPNeighbor.lock());
}

// deprecated
bool RenderablePolyVoxEntityItem::getMeshes(MeshProxyList& result) {
    bool success = false;
    if (_mesh) {
        MeshProxy* meshProxy = nullptr;
        glm::mat4 transform = voxelToLocalMatrix();
        withReadLock([&] {
            gpu::BufferView::Index numVertices = (gpu::BufferView::Index)_mesh->getNumVertices();
            if (!_meshReady) {
                // we aren't ready to return a mesh.  the caller will have to try again later.
                success = false;
            } else if (numVertices == 0) {
                // we are ready, but there are no triangles in the mesh.
                success = true;
            } else {
                success = true;
                // the mesh will be in voxel-space.  transform it into object-space
                meshProxy = new SimpleMeshProxy(
                    _mesh->map([=](glm::vec3 position) { return glm::vec3(transform * glm::vec4(position, 1.0f)); },
                    [=](glm::vec3 color) { return color; },
                    [=](glm::vec3 normal) { return glm::normalize(glm::vec3(transform * glm::vec4(normal, 0.0f))); },
                    [&](uint32_t index) { return index; }));
                result << meshProxy;
            }
        });
    }
    return success;
}

scriptable::ScriptableModelBase RenderablePolyVoxEntityItem::getScriptableModel() {
    if (!_mesh) {
        return scriptable::ScriptableModelBase();
    }

    bool success = false;
    glm::mat4 transform = voxelToLocalMatrix();
    scriptable::ScriptableModelBase result;
    result.objectID = getThisPointer()->getID();
    withReadLock([&] {
        gpu::BufferView::Index numVertices = (gpu::BufferView::Index)_mesh->getNumVertices();
        if (!_meshReady) {
            // we aren't ready to return a mesh.  the caller will have to try again later.
            success = false;
        } else if (numVertices == 0) {
            // we are ready, but there are no triangles in the mesh.
            success = true;
        } else {
            success = true;
            // the mesh will be in voxel-space.  transform it into object-space
            result.append(_mesh->map(
                [=](glm::vec3 position){ return glm::vec3(transform * glm::vec4(position, 1.0f)); },
                [=](glm::vec3 color){ return color; },
                [=](glm::vec3 normal){ return glm::normalize(glm::vec3(transform * glm::vec4(normal, 0.0f))); },
                [&](uint32_t index){ return index; }
            ));
        }
    });
    return result;
}

using namespace render;
using namespace render::entities;

static uint8_t CUSTOM_PIPELINE_NUMBER;
static std::map<std::tuple<bool, bool, bool>, ShapePipelinePointer> _pipelines;
static gpu::Stream::FormatPointer _vertexFormat;

ShapePipelinePointer shapePipelineFactory(const ShapePlumber& plumber, const ShapeKey& key, RenderArgs* args) {
    // FIXME: custom pipelines like this don't handle shadows or renderLayers correctly

    if (_pipelines.empty()) {
        using namespace shader::entities_renderer::program;

        static const std::vector<std::tuple<bool, bool, uint32_t>> keys = {
            std::make_tuple(false, false, polyvox), std::make_tuple(true, false, polyvox_forward)
#ifdef POLYVOX_ENTITY_USE_FADE_EFFECT
            , std::make_tuple(false, true, polyvox_fade), std::make_tuple(true, true, polyvox_forward_fade)
#else
            , std::make_tuple(false, true, polyvox), std::make_tuple(true, true, polyvox_forward)
#endif
        };
        for (auto& key : keys) {
            for (int i = 0; i < 2; ++i) {
                bool wireframe = i != 0;

                auto state = std::make_shared<gpu::State>();
                state->setCullMode(gpu::State::CULL_BACK);
                state->setDepthTest(true, true, gpu::LESS_EQUAL);
                PrepareStencil::testMaskDrawShape(*state);

                if (wireframe) {
                    state->setFillMode(gpu::State::FILL_LINE);
                }

                auto pipeline = gpu::Pipeline::create(gpu::Shader::createProgram(std::get<2>(key)), state);
                if (std::get<1>(key)) {
                    _pipelines[std::make_tuple(std::get<0>(key), std::get<1>(key), wireframe)] = std::make_shared<render::ShapePipeline>(pipeline, nullptr, nullptr, nullptr);
                } else {
                    const auto& fadeEffect = DependencyManager::get<FadeEffect>();
                    _pipelines[std::make_tuple(std::get<0>(key), std::get<1>(key), wireframe)] = std::make_shared<render::ShapePipeline>(pipeline, nullptr,
                        fadeEffect->getBatchSetter(), fadeEffect->getItemUniformSetter());
                }
            }
        }
    }

    return _pipelines[std::make_tuple(args->_renderMethod == Args::RenderMethod::FORWARD, key.isFaded(), key.isWireframe())];
}

PolyVoxEntityRenderer::PolyVoxEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    static std::once_flag once;
    std::call_once(once, [&] {
        CUSTOM_PIPELINE_NUMBER = render::ShapePipeline::registerCustomShapePipelineFactory(shapePipelineFactory);
        _vertexFormat = std::make_shared<gpu::Stream::Format>();
        _vertexFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        _vertexFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 12);
    });
    _params = std::make_shared<gpu::Buffer>(sizeof(glm::vec4), nullptr);
}

ItemKey PolyVoxEntityRenderer::getKey() {
    auto builder = ItemKey::Builder::opaqueShape().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    return builder.build();
}

ShapeKey PolyVoxEntityRenderer::getShapeKey() {
    auto builder = ShapeKey::Builder().withCustom(CUSTOM_PIPELINE_NUMBER);
    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }
    return builder.build();
}

bool PolyVoxEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (resultWithReadLock<bool>([&] {
        if (entity->voxelToLocalMatrix() != _lastVoxelToLocalMatrix) {
            return true;
        }

        if (entity->_mesh != _mesh) {
            return true;
        }

        return false;
    })) {
        return true;
    }

    return Parent::needsRenderUpdateFromTypedEntity(entity);
}

void PolyVoxEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
#ifdef POLYVOX_ENTITY_USE_FADE_EFFECT
    if (!_hasTransitioned) {
        transaction.resetTransitionOnItem(_renderItemID, render::Transition::ELEMENT_ENTER_DOMAIN);
        _hasTransitioned = true;
    }
#endif
}

void PolyVoxEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    _lastVoxelToLocalMatrix = entity->voxelToLocalMatrix();
    _position = entity->getWorldPosition();
    _orientation = entity->getWorldOrientation();
    _lastVoxelVolumeSize = entity->getVoxelVolumeSize();
    _params->setSubData(0, vec4(_lastVoxelVolumeSize, 0.0));
    graphics::MeshPointer newMesh;
    entity->withReadLock([&] {
        newMesh = entity->_mesh;
    });

    if (newMesh && newMesh->getIndexBuffer()._buffer) {
        _mesh = newMesh;
    }

    std::array<QString, 3> xyzTextureURLs{ {
        entity->getXTextureURL(),
        entity->getYTextureURL(),
        entity->getZTextureURL()
    } };
    for (size_t i = 0; i < xyzTextureURLs.size(); ++i) {
        auto& texture = _xyzTextures[i];
        const auto& textureURL = xyzTextureURLs[i];
        if (textureURL.isEmpty()) {
            texture.reset();
        } else if (!texture || texture->getURL() != QUrl(textureURL)) {
            texture = DependencyManager::get<TextureCache>()->getTexture(textureURL);
        }
    }
}

void PolyVoxEntityRenderer::doRender(RenderArgs* args) {
    if (!_mesh || !_mesh->getIndexBuffer()._buffer) {
        return;
    }

    PerformanceTimer perfTimer("RenderablePolyVoxEntityItem::render");
    gpu::Batch& batch = *args->_batch;

    glm::mat4 rotation = glm::mat4_cast(BillboardModeHelpers::getBillboardRotation(_position, _orientation, _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    Transform transform(glm::translate(_position) * rotation * _lastVoxelToLocalMatrix);
    batch.setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }

    batch.setInputFormat(_vertexFormat);
    batch.setInputBuffer(gpu::Stream::POSITION, _mesh->getVertexBuffer()._buffer, 0,
        sizeof(PolyVox::PositionMaterialNormal));

    // TODO -- should we be setting this?
    // batch.setInputBuffer(gpu::Stream::NORMAL, mesh->getVertexBuffer()._buffer,
    //                      12,
    //                      sizeof(PolyVox::PositionMaterialNormal));
    batch.setIndexBuffer(gpu::UINT32, _mesh->getIndexBuffer()._buffer, 0);

    for (size_t i = 0; i < _xyzTextures.size(); ++i) {
        const auto& texture = _xyzTextures[i];
        if (texture) {
            batch.setResourceTexture((uint32_t)i, texture->getGPUTexture());
        } else {
            batch.setResourceTexture((uint32_t)i, DependencyManager::get<TextureCache>()->getWhiteTexture());
        }
    }

    batch.setUniformBuffer(0, _params);
    batch.drawIndexed(gpu::TRIANGLES, (gpu::uint32)_mesh->getNumIndices(), 0);
}

QDebug operator<<(QDebug debug, PolyVoxState state) {
    switch (state) {
        case PolyVoxState::Ready:
            debug << "Ready";
            break;
        case PolyVoxState::Uncompressing:
            debug << "Uncompressing";
            break;
        case PolyVoxState::UncompressingFinished:
            debug << "UncompressingFinished";
            break;
        case PolyVoxState::BakingMesh:
            debug << "BakingMesh";
            break;
        case PolyVoxState::BakingMeshFinished:
            debug << "BakingMeshFinished";
            break;
        case PolyVoxState::BakingMeshNoCompress:
            debug << "BakingMeshNoCompress";
            break;
        case PolyVoxState::BakingMeshNoCompressFinished:
            debug << "BakingMeshNoCompressFinished";
            break;
        case PolyVoxState::Compressing:
            debug << "Compressing";
            break;
        case PolyVoxState::CompressingFinished:
            debug << "CompressingFinished";
            break;
        case PolyVoxState::BakingShape:
            debug << "BakingShape";
            break;
        case PolyVoxState::BakingShapeFinished:
            debug << "BakingShapeFinished";
            break;
    }
    return debug;
}
