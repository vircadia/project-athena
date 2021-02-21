//
//  RenderableModelEntityItem.cpp
//  interface/src
//
//  Created by Brad Hefta-Gaub on 8/6/14.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableModelEntityItem.h"

#include <set>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <QtCore/QJsonDocument>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QUrlQuery>

#include <AbstractViewStateInterface.h>
#include <Model.h>
#include <PerfStat.h>
#include <render/Scene.h>
#include <DependencyManager.h>
#include <AnimationCache.h>
#include <shared/QtHelpers.h>

#include "EntityTreeRenderer.h"
#include "EntitiesRendererLogging.h"


void ModelEntityWrapper::setModel(const ModelPointer& model) {
    withWriteLock([&] {
        if (_model != model) {
            _model = model;
            if (_model) {
                _needsInitialSimulation = true;
            }
        }
    });
}

ModelPointer ModelEntityWrapper::getModel() const {
    return resultWithReadLock<ModelPointer>([&] {
        return _model;
    });
}

EntityItemPointer RenderableModelEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    EntityItemPointer entity(new RenderableModelEntityItem(entityID, properties.getDimensionsInitialized()),
                             [](EntityItem* ptr) { ptr->deleteLater(); });
    
    entity->setProperties(properties);

    return entity;
}

RenderableModelEntityItem::RenderableModelEntityItem(const EntityItemID& entityItemID, bool dimensionsInitialized) :
    ModelEntityWrapper(entityItemID),
    _dimensionsInitialized(dimensionsInitialized) {

}

RenderableModelEntityItem::~RenderableModelEntityItem() { }

void RenderableModelEntityItem::setUnscaledDimensions(const glm::vec3& value) {
    glm::vec3 newDimensions = glm::max(value, glm::vec3(0.0f)); // can never have negative dimensions
    if (getUnscaledDimensions() != newDimensions) {
        _dimensionsInitialized = true;
        ModelEntityItem::setUnscaledDimensions(value);
    }
}

void RenderableModelEntityItem::autoResizeJointArrays() {
    ModelPointer model = getModel();
    if (model && model->isLoaded() && !_needsInitialSimulation) {
        resizeJointArrays(model->getJointStateCount());
    }
}

bool RenderableModelEntityItem::needsUpdateModelBounds() const {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);
    ModelPointer model = getModel();
    if (!hasModel() || !model) {
        return false;
    }

    if (!_dimensionsInitialized || !model->isLoaded()) {
        return false;
    }

    if (model->needsReload()) {
        return true;
    }

    if (isAnimatingSomething()) {
        return true;
    }

    if (_needsInitialSimulation || _needsJointSimulation) {
        return true;
    }

    if (model->getScaleToFitDimensions() != getScaledDimensions()) {
        return true;
    }

    if (model->getRegistrationPoint() != getRegistrationPoint()) {
        return true;
    }

    bool success;
    auto transform = getTransform(success);
    if (success) {
        if (model->getTranslation() != transform.getTranslation()) {
            return true;
        }
        if (model->getRotation() != transform.getRotation()) {
            return true;
        }
    }

    return false;
}

void RenderableModelEntityItem::updateModelBounds() {
    DETAILED_PROFILE_RANGE(simulation_physics, "updateModelBounds");

    if (!_dimensionsInitialized || !hasModel()) {
        return;
    }

    ModelPointer model = getModel();
    if (!model || !model->isLoaded()) {
        return;
    }

    bool updateRenderItems = false;
    if (model->needsReload()) {
        model->updateGeometry();
        updateRenderItems = true;
    }

    bool overridingModelTransform = model->isOverridingModelTransformAndOffset();
    glm::vec3 scaledDimensions = getScaledDimensions();
    glm::vec3 registrationPoint = getRegistrationPoint();
    bool needsSimulate = false;
    if (!overridingModelTransform &&
        (model->getScaleToFitDimensions() != scaledDimensions ||
        model->getRegistrationPoint() != registrationPoint ||
        !model->getIsScaledToFit() || _needsToRescaleModel ||
        _useOriginalPivot == model->getSnapModelToRegistrationPoint())) {
        // The machinery for updateModelBounds will give existing models the opportunity to fix their
        // translation/rotation/scale/registration.  The first two are straightforward, but the latter two
        // have guards to make sure they don't happen after they've already been set.  Here we reset those guards.
        // This doesn't cause the entity values to change -- it just allows the model to match once it comes in.
        model->setScaleToFit(false, scaledDimensions);
        model->setSnapModelToRegistrationPoint(false, registrationPoint);

        // now recalculate the bounds and registration
        model->setScaleToFit(true, scaledDimensions);
        model->setSnapModelToRegistrationPoint(!_useOriginalPivot, registrationPoint);
        updateRenderItems = true;
        needsSimulate = true;
        _needsToRescaleModel = false;
    }

    bool success;
    auto transform = getTransform(success);
    if (success && (model->getTranslation() != transform.getTranslation() ||
            model->getRotation() != transform.getRotation())) {
        model->setTransformNoUpdateRenderItems(transform);
        updateRenderItems = true;
    }

    if (_needsInitialSimulation || _needsJointSimulation || needsSimulate || isAnimatingSomething()) {
        // NOTE: on isAnimatingSomething() we need to call Model::simulate() which calls Rig::updateRig()
        // TODO: there is opportunity to further optimize the isAnimatingSomething() case.
        model->simulate(0.0f);
        locationChanged();
        _needsInitialSimulation = false;
        _needsJointSimulation = false;
        updateRenderItems = true;
    }

    if (updateRenderItems) {
        glm::vec3 scale = model->getScale();
        model->setUseDualQuaternionSkinning(!isNonUniformScale(scale));
        model->updateRenderItems();
    }
}


EntityItemProperties RenderableModelEntityItem::getProperties(const EntityPropertyFlags& desiredProperties, bool allowEmptyDesiredProperties) const {
    EntityItemProperties properties = ModelEntityItem::getProperties(desiredProperties, allowEmptyDesiredProperties); // get the properties from our base class
    if (_originalTexturesRead) {
        properties.setTextureNames(_originalTextures);
    }

    ModelPointer model = getModel();
    if (model) {
        properties.setRenderInfoVertexCount(model->getRenderInfoVertexCount());
        properties.setRenderInfoTextureCount(model->getRenderInfoTextureCount());
        properties.setRenderInfoTextureSize(model->getRenderInfoTextureSize());
        properties.setRenderInfoDrawCalls(model->getRenderInfoDrawCalls());
        properties.setRenderInfoHasTransparent(model->getRenderInfoHasTransparent());

        if (model->isLoaded()) {
            // TODO: improve naturalDimensions in the future,
            //       for now we've added this hack for setting natural dimensions of models
            Extents meshExtents = model->getHFMModel().getUnscaledMeshExtents();
            properties.setNaturalDimensions(meshExtents.maximum - meshExtents.minimum);
            properties.calculateNaturalPosition(meshExtents.minimum, meshExtents.maximum);
        }
    }

    return properties;
}

glm::vec3 RenderableModelEntityItem::getPivot() const {
    auto model = getModel();
    auto pivot = EntityItem::getPivot();
    if (!model || !model->isLoaded() || !_useOriginalPivot) {
        return pivot;
    }

    return pivot + model->getOriginalOffset();
}

bool RenderableModelEntityItem::supportsDetailedIntersection() const {
    return true;
}

bool RenderableModelEntityItem::findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                         const glm::vec3& viewFrustumPos, OctreeElementPointer& element, float& distance, BoxFace& face,
                         glm::vec3& surfaceNormal, QVariantMap& extraInfo, bool precisionPicking) const {
    auto model = getModel();
    if (!model || !model->isLoaded()) {
        return false;
    }

    return model->findRayIntersectionAgainstSubMeshes(origin, direction, viewFrustumPos, distance,
               face, surfaceNormal, extraInfo, precisionPicking, false);
}

bool RenderableModelEntityItem::findDetailedParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity,
                        const glm::vec3& acceleration, const glm::vec3& viewFrustumPos, OctreeElementPointer& element,
                        float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo, bool precisionPicking) const {
    auto model = getModel();
    if (!model || !model->isLoaded()) {
        return false;
    }

    return model->findParabolaIntersectionAgainstSubMeshes(origin, velocity, acceleration, viewFrustumPos, parabolicDistance,
        face, surfaceNormal, extraInfo, precisionPicking, false);
}

QString RenderableModelEntityItem::getCollisionShapeURL() const {
    return getShapeType() == SHAPE_TYPE_COMPOUND ? getCompoundShapeURL() : getModelURL();
}

void RenderableModelEntityItem::fetchCollisionGeometryResource() {
    _collisionGeometryResource = DependencyManager::get<ModelCache>()->getCollisionGeometryResource(getCollisionShapeURL());
    if (_collisionGeometryResource) {
        if (_collisionGeometryResource->isLoaded()) {
            markDirtyFlags(Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
            locationChanged();
        } else {
            connect(_collisionGeometryResource.get(), &GeometryResource::finished, this, [&] {
                markDirtyFlags(Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
                locationChanged();
            });
        }
    }
}

bool RenderableModelEntityItem::unableToLoadCollisionShape() {
    if (!_collisionGeometryResource) {
        fetchCollisionGeometryResource();
    }
    return (_collisionGeometryResource && _collisionGeometryResource->isFailed());
}

void RenderableModelEntityItem::setShapeType(ShapeType type) {
    ModelEntityItem::setShapeType(type);
    auto shapeType = getShapeType();
    if (shapeType == SHAPE_TYPE_COMPOUND || shapeType == SHAPE_TYPE_SIMPLE_COMPOUND) {
        if (!_collisionGeometryResource && !getCollisionShapeURL().isEmpty()) {
            fetchCollisionGeometryResource();
        }
    } else if (_collisionGeometryResource) {
        // the compoundURL has been set but the shapeType does not agree
        _collisionGeometryResource.reset();
    }
}

void RenderableModelEntityItem::setCompoundShapeURL(const QString& url) {
    auto currentCompoundShapeURL = getCompoundShapeURL();
    ModelEntityItem::setCompoundShapeURL(url);
    if (url != currentCompoundShapeURL && !url.isEmpty()) {
        auto shapeType = getShapeType();
        if (shapeType == SHAPE_TYPE_COMPOUND) {
            fetchCollisionGeometryResource();
        }
    }
}

void RenderableModelEntityItem::setModelURL(const QString& url) {
    auto currentModelURL = getModelURL();
    ModelEntityItem::setModelURL(url);
    if (url != currentModelURL && !url.isEmpty()) {
        auto shapeType = getShapeType();
        if (shapeType == SHAPE_TYPE_SIMPLE_COMPOUND) {
            fetchCollisionGeometryResource();
        }
    }
}

bool RenderableModelEntityItem::isReadyToComputeShape() const {
    auto model = getModel();
    auto shapeType = getShapeType();
    if (shapeType == SHAPE_TYPE_COMPOUND || shapeType == SHAPE_TYPE_SIMPLE_COMPOUND) {
        auto shapeURL = getCollisionShapeURL();
        // we need a render geometry with a scale to proceed
        if (model && !model->getURL().isEmpty() && !shapeURL.isEmpty() && _dimensionsInitialized && model->isLoaded()) {
            if (!_collisionGeometryResource) {
                const_cast<RenderableModelEntityItem*>(this)->fetchCollisionGeometryResource();
            }

            // do we have both URLs AND both geometries AND they are both fully loaded?
            return _collisionGeometryResource && _collisionGeometryResource->isLoaded() && _collisionGeometryResource->isHFMModelLoaded();
        }

        return false;
    } else if (shapeType >= SHAPE_TYPE_SIMPLE_HULL && shapeType <= SHAPE_TYPE_STATIC_MESH) {
        return model && model->isLoaded() && _dimensionsInitialized;
    }
    return true;
}

void RenderableModelEntityItem::computeShapeInfo(ShapeInfo& shapeInfo) {
    const uint32_t TRIANGLE_STRIDE = 3;
    const uint32_t QUAD_STRIDE = 4;

    ShapeType type = getShapeType();

    auto model = getModel();
    if (type >= SHAPE_TYPE_COMPOUND && type <= SHAPE_TYPE_STATIC_MESH) {
        if (!model) {
            type = SHAPE_TYPE_NONE;
        } else if (!model->isLoaded()) {
            type = SHAPE_TYPE_NONE;
            if (!model->didVisualGeometryRequestFail()) {
                markDirtyFlags(Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
                locationChanged();
            }
        }
    }

    if (type == SHAPE_TYPE_COMPOUND || type == SHAPE_TYPE_SIMPLE_COMPOUND) {
        if (!_collisionGeometryResource || !_collisionGeometryResource->isLoaded() || !_collisionGeometryResource->isHFMModelLoaded()) {
            type = SHAPE_TYPE_NONE;
            if (!_collisionGeometryResource->isFailed()) {
                markDirtyFlags(Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
                locationChanged();
            }
        }
    }

    if (type == SHAPE_TYPE_COMPOUND) {
        updateModelBounds();

        const HFMModel& collisionGeometry = _collisionGeometryResource->getHFMModel();

        ShapeInfo::PointCollection& pointCollection = shapeInfo.getPointCollection();
        pointCollection.clear();
        uint32_t i = 0;

        // the way OBJ files get read, each section under a "g" line is its own meshPart.  We only expect
        // to find one actual "mesh" (with one or more meshParts in it), but we loop over the meshes, just in case.
        foreach (const HFMMesh& mesh, collisionGeometry.meshes) {
            // each meshPart is a convex hull
            foreach (const HFMMeshPart &meshPart, mesh.parts) {
                pointCollection.push_back(QVector<glm::vec3>());
                ShapeInfo::PointList& pointsInPart = pointCollection[i];

                // run through all the triangles and (uniquely) add each point to the hull
                uint32_t numIndices = (uint32_t)meshPart.triangleIndices.size();
                // TODO: assert rather than workaround after we start sanitizing HFMMesh higher up
                //assert(numIndices % TRIANGLE_STRIDE == 0);
                numIndices -= numIndices % TRIANGLE_STRIDE; // WORKAROUND lack of sanity checking in FBXSerializer

                for (uint32_t j = 0; j < numIndices; j += TRIANGLE_STRIDE) {
                    glm::vec3 p0 = mesh.vertices[meshPart.triangleIndices[j]];
                    glm::vec3 p1 = mesh.vertices[meshPart.triangleIndices[j + 1]];
                    glm::vec3 p2 = mesh.vertices[meshPart.triangleIndices[j + 2]];
                    if (!pointsInPart.contains(p0)) {
                        pointsInPart << p0;
                    }
                    if (!pointsInPart.contains(p1)) {
                        pointsInPart << p1;
                    }
                    if (!pointsInPart.contains(p2)) {
                        pointsInPart << p2;
                    }
                }

                // run through all the quads and (uniquely) add each point to the hull
                numIndices = (uint32_t)meshPart.quadIndices.size();
                // TODO: assert rather than workaround after we start sanitizing HFMMesh higher up
                //assert(numIndices % QUAD_STRIDE == 0);
                numIndices -= numIndices % QUAD_STRIDE; // WORKAROUND lack of sanity checking in FBXSerializer

                for (uint32_t j = 0; j < numIndices; j += QUAD_STRIDE) {
                    glm::vec3 p0 = mesh.vertices[meshPart.quadIndices[j]];
                    glm::vec3 p1 = mesh.vertices[meshPart.quadIndices[j + 1]];
                    glm::vec3 p2 = mesh.vertices[meshPart.quadIndices[j + 2]];
                    glm::vec3 p3 = mesh.vertices[meshPart.quadIndices[j + 3]];
                    if (!pointsInPart.contains(p0)) {
                        pointsInPart << p0;
                    }
                    if (!pointsInPart.contains(p1)) {
                        pointsInPart << p1;
                    }
                    if (!pointsInPart.contains(p2)) {
                        pointsInPart << p2;
                    }
                    if (!pointsInPart.contains(p3)) {
                        pointsInPart << p3;
                    }
                }

                if (pointsInPart.size() == 0) {
                    qCDebug(entitiesrenderer) << "Warning -- meshPart has no faces";
                    pointCollection.pop_back();
                    continue;
                }
                ++i;
            }
        }

        // We expect that the collision model will have the same units and will be displaced
        // from its origin in the same way the visual model is.  The visual model has
        // been centered and probably scaled.  We take the scaling and offset which were applied
        // to the visual model and apply them to the collision model (without regard for the
        // collision model's extents).

        glm::vec3 dimensions = getScaledDimensions();
        glm::vec3 extents = model->getHFMModel().getUnscaledMeshExtents().size();
        glm::vec3 scaleToFit = dimensions / extents;
        // multiply each point by scale before handing the point-set off to the physics engine.
        // also determine the extents of the collision model.
        glm::vec3 registrationOffset = dimensions * (ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - getRegistrationPoint());
        glm::vec3 offset = model->getSnapModelToRegistrationPoint() ? model->getOffset() : glm::vec3(0.0f);
        for (int32_t i = 0; i < pointCollection.size(); i++) {
            for (int32_t j = 0; j < pointCollection[i].size(); j++) {
                // back compensate for registration so we can apply that offset to the shapeInfo later
                pointCollection[i][j] = scaleToFit * (pointCollection[i][j] + offset) - registrationOffset;
            }
        }
        shapeInfo.setParams(type, 0.5f * extents, getCompoundShapeURL() + model->getSnapModelToRegistrationPoint());
        adjustShapeInfoByRegistration(shapeInfo, model->getSnapModelToRegistrationPoint());
    } else if (type >= SHAPE_TYPE_SIMPLE_HULL && type <= SHAPE_TYPE_STATIC_MESH) {
        updateModelBounds();
        model->updateGeometry();

        // compute meshPart local transforms
        QVector<glm::mat4> localTransforms;
        const HFMModel& hfmModel = model->getHFMModel();
        int numHFMMeshes = hfmModel.meshes.size();
        int totalNumVertices = 0;
        glm::vec3 dimensions = getScaledDimensions();
        glm::mat4 invRegistrationOffset = glm::translate(dimensions * (getRegistrationPoint() - ENTITY_ITEM_DEFAULT_REGISTRATION_POINT));
        for (int i = 0; i < numHFMMeshes; i++) {
            const HFMMesh& mesh = hfmModel.meshes.at(i);
            if (mesh.clusters.size() > 0) {
                const HFMCluster& cluster = mesh.clusters.at(0);
                auto jointMatrix = model->getRig().getJointTransform(cluster.jointIndex);
                // we backtranslate by the registration offset so we can apply that offset to the shapeInfo later
                localTransforms.push_back(invRegistrationOffset * jointMatrix * cluster.inverseBindMatrix);
            } else {
                localTransforms.push_back(invRegistrationOffset);
            }
            totalNumVertices += mesh.vertices.size();
        }
        const int32_t MAX_VERTICES_PER_STATIC_MESH = 1e6;
        if (totalNumVertices > MAX_VERTICES_PER_STATIC_MESH) {
            qWarning() << "model" << getModelURL() << "has too many vertices" << totalNumVertices << "and will collide as a box.";
            shapeInfo.setParams(SHAPE_TYPE_BOX, 0.5f * dimensions);
            return;
        }

        std::vector<std::shared_ptr<const graphics::Mesh>> meshes;
        if (type == SHAPE_TYPE_SIMPLE_COMPOUND) {
            auto& hfmMeshes = _collisionGeometryResource->getHFMModel().meshes;
            meshes.reserve(hfmMeshes.size());
            for (auto& hfmMesh : hfmMeshes) {
                meshes.push_back(hfmMesh._mesh);
            }
        } else {
            meshes = model->getGeometry()->getMeshes();
        }
        int32_t numMeshes = (int32_t)(meshes.size());

        const int MAX_ALLOWED_MESH_COUNT = 1000;
        if (numMeshes > MAX_ALLOWED_MESH_COUNT) {
            // too many will cause the deadlock timer to throw...
            shapeInfo.setParams(SHAPE_TYPE_BOX, 0.5f * dimensions);
            return;
        }

        ShapeInfo::PointCollection& pointCollection = shapeInfo.getPointCollection();
        pointCollection.clear();
        if (type == SHAPE_TYPE_SIMPLE_COMPOUND) {
            pointCollection.resize(numMeshes);
        } else {
            pointCollection.resize(1);
        }

        ShapeInfo::TriangleIndices& triangleIndices = shapeInfo.getTriangleIndices();
        triangleIndices.clear();

        Extents extents;
        int32_t meshCount = 0;
        int32_t pointListIndex = 0;
        for (auto& mesh : meshes) {
            if (!mesh) {
                continue;
            }
            const gpu::BufferView& vertices = mesh->getVertexBuffer();
            const gpu::BufferView& indices = mesh->getIndexBuffer();
            const gpu::BufferView& parts = mesh->getPartBuffer();

            ShapeInfo::PointList& points = pointCollection[pointListIndex];

            // reserve room
            int32_t sizeToReserve = (int32_t)(vertices.getNumElements());
            if (type == SHAPE_TYPE_SIMPLE_COMPOUND) {
                // a list of points for each mesh
                pointListIndex++;
            } else {
                // only one list of points
                sizeToReserve += (int32_t)((gpu::Size)points.size());
            }
            points.reserve(sizeToReserve);

            // copy points
            uint32_t meshIndexOffset = (uint32_t)points.size();
            const glm::mat4& localTransform = localTransforms[meshCount];
            gpu::BufferView::Iterator<const glm::vec3> vertexItr = vertices.cbegin<const glm::vec3>();
            while (vertexItr != vertices.cend<const glm::vec3>()) {
                glm::vec3 point = extractTranslation(localTransform * glm::translate(*vertexItr));
                points.push_back(point);
                extents.addPoint(point);
                ++vertexItr;
            }

            if (type == SHAPE_TYPE_STATIC_MESH) {
                // copy into triangleIndices
                triangleIndices.reserve((int32_t)((gpu::Size)(triangleIndices.size()) + indices.getNumElements()));
                gpu::BufferView::Iterator<const graphics::Mesh::Part> partItr = parts.cbegin<const graphics::Mesh::Part>();
                while (partItr != parts.cend<const graphics::Mesh::Part>()) {
                    auto numIndices = partItr->_numIndices;
                    if (partItr->_topology == graphics::Mesh::TRIANGLES) {
                        // TODO: assert rather than workaround after we start sanitizing HFMMesh higher up
                        //assert(numIndices % TRIANGLE_STRIDE == 0);
                        numIndices -= numIndices % TRIANGLE_STRIDE; // WORKAROUND lack of sanity checking in FBXSerializer

                        auto indexItr = indices.cbegin<const gpu::BufferView::Index>() + partItr->_startIndex;
                        auto indexEnd = indexItr + numIndices;
                        while (indexItr != indexEnd) {
                            triangleIndices.push_back(*indexItr + meshIndexOffset);
                            ++indexItr;
                        }
                    } else if (partItr->_topology == graphics::Mesh::TRIANGLE_STRIP) {
                        // TODO: resurrect assert after we start sanitizing HFMMesh higher up
                        //assert(numIndices > 2);

                        uint32_t approxNumIndices = TRIANGLE_STRIDE * numIndices;
                        if (approxNumIndices > (uint32_t)(triangleIndices.capacity() - triangleIndices.size())) {
                            // we underestimated the final size of triangleIndices so we pre-emptively expand it
                            triangleIndices.reserve(triangleIndices.size() + approxNumIndices);
                        }

                        auto indexItr = indices.cbegin<const gpu::BufferView::Index>() + partItr->_startIndex;
                        auto indexEnd = indexItr + (numIndices - 2);

                        // first triangle uses the first three indices
                        triangleIndices.push_back(*(indexItr++) + meshIndexOffset);
                        triangleIndices.push_back(*(indexItr++) + meshIndexOffset);
                        triangleIndices.push_back(*(indexItr++) + meshIndexOffset);

                        // the rest use previous and next index
                        uint32_t triangleCount = 1;
                        while (indexItr != indexEnd) {
                            if ((*indexItr) != graphics::Mesh::PRIMITIVE_RESTART_INDEX) {
                                if (triangleCount % 2 == 0) {
                                    // even triangles use first two indices in order
                                    triangleIndices.push_back(*(indexItr - 2) + meshIndexOffset);
                                    triangleIndices.push_back(*(indexItr - 1) + meshIndexOffset);
                                } else {
                                    // odd triangles swap order of first two indices
                                    triangleIndices.push_back(*(indexItr - 1) + meshIndexOffset);
                                    triangleIndices.push_back(*(indexItr - 2) + meshIndexOffset);
                                }
                                triangleIndices.push_back(*indexItr + meshIndexOffset);
                                ++triangleCount;
                            }
                            ++indexItr;
                        }
                    }
                    ++partItr;
                }
            } else if (type == SHAPE_TYPE_SIMPLE_COMPOUND) {
                // for each mesh copy unique part indices, separated by special bogus (flag) index values
                gpu::BufferView::Iterator<const graphics::Mesh::Part> partItr = parts.cbegin<const graphics::Mesh::Part>();
                while (partItr != parts.cend<const graphics::Mesh::Part>()) {
                    // collect unique list of indices for this part
                    std::set<int32_t> uniqueIndices;
                    auto numIndices = partItr->_numIndices;
                    if (partItr->_topology == graphics::Mesh::TRIANGLES) {
                        // TODO: assert rather than workaround after we start sanitizing HFMMesh higher up
                        //assert(numIndices% TRIANGLE_STRIDE == 0);
                        numIndices -= numIndices % TRIANGLE_STRIDE; // WORKAROUND lack of sanity checking in FBXSerializer

                        auto indexItr = indices.cbegin<const gpu::BufferView::Index>() + partItr->_startIndex;
                        auto indexEnd = indexItr + numIndices;
                        while (indexItr != indexEnd) {
                            uniqueIndices.insert(*indexItr);
                            ++indexItr;
                        }
                    } else if (partItr->_topology == graphics::Mesh::TRIANGLE_STRIP) {
                        // TODO: resurrect assert after we start sanitizing HFMMesh higher up
                        //assert(numIndices > TRIANGLE_STRIDE - 1);

                        auto indexItr = indices.cbegin<const gpu::BufferView::Index>() + partItr->_startIndex;
                        auto indexEnd = indexItr + (numIndices - 2);

                        // first triangle uses the first three indices
                        uniqueIndices.insert(*(indexItr++));
                        uniqueIndices.insert(*(indexItr++));
                        uniqueIndices.insert(*(indexItr++));

                        // the rest use previous and next index
                        uint32_t triangleCount = 1;
                        while (indexItr != indexEnd) {
                            if ((*indexItr) != graphics::Mesh::PRIMITIVE_RESTART_INDEX) {
                                if (triangleCount % 2 == 0) {
                                    // EVEN triangles use first two indices in order
                                    uniqueIndices.insert(*(indexItr - 2));
                                    uniqueIndices.insert(*(indexItr - 1));
                                } else {
                                    // ODD triangles swap order of first two indices
                                    uniqueIndices.insert(*(indexItr - 1));
                                    uniqueIndices.insert(*(indexItr - 2));
                                }
                                uniqueIndices.insert(*indexItr);
                                ++triangleCount;
                            }
                            ++indexItr;
                        }
                    }

                    // store uniqueIndices in triangleIndices
                    triangleIndices.reserve(triangleIndices.size() + (int32_t)uniqueIndices.size());
                    for (auto index : uniqueIndices) {
                        triangleIndices.push_back(index);
                    }
                    // flag end of part
                    triangleIndices.push_back(END_OF_MESH_PART);

                    ++partItr;
                }
                // flag end of mesh
                triangleIndices.push_back(END_OF_MESH);
            }
            ++meshCount;
        }

        // scale and shift
        glm::vec3 extentsSize = extents.size();
        glm::vec3 scaleToFit = dimensions / extentsSize;
        for (int32_t i = 0; i < 3; ++i) {
            if (extentsSize[i] < 1.0e-6f) {
                scaleToFit[i] = 1.0f;
            }
        }
        for (auto points : pointCollection) {
            for (int32_t i = 0; i < points.size(); ++i) {
                points[i] = (points[i] * scaleToFit);
            }
        }

        shapeInfo.setParams(type, 0.5f * extents.size(), getModelURL() + model->getSnapModelToRegistrationPoint());
        adjustShapeInfoByRegistration(shapeInfo, model->getSnapModelToRegistrationPoint());
    } else {
        EntityItem::computeShapeInfo(shapeInfo);
    }
}

void RenderableModelEntityItem::setJointMap(std::vector<int> jointMap) {
    if (jointMap.size() > 0) {
        _jointMap = jointMap;
        _jointMapCompleted = true;
        return;
    }

    _jointMapCompleted = false;
};

int RenderableModelEntityItem::avatarJointIndex(int modelJointIndex) {
    int result = -1;
    int mapSize = (int)_jointMap.size();
    if (modelJointIndex >= 0 && modelJointIndex < mapSize) {
        result = _jointMap[modelJointIndex];
    }

    return result;
}

bool RenderableModelEntityItem::contains(const glm::vec3& point) const {
    auto model = getModel();
    if (model && model->isLoaded()) {
        auto shapeType = getShapeType();
        if (shapeType == SHAPE_TYPE_COMPOUND || shapeType == SHAPE_TYPE_SIMPLE_COMPOUND) {
            if (_collisionGeometryResource && _collisionGeometryResource->isLoaded() && _collisionGeometryResource->isHFMModelLoaded() && EntityItem::contains(point)) {
                glm::mat4 worldToHFMMatrix = model->getWorldToHFMMatrix();
                glm::vec3 hfmPoint = worldToHFMMatrix * glm::vec4(point, 1.0f);
                return _collisionGeometryResource->getHFMModel().convexHullContains(hfmPoint);
            }
        } else if (shapeType >= SHAPE_TYPE_SIMPLE_HULL && shapeType <= SHAPE_TYPE_STATIC_MESH) {
            if (EntityItem::contains(point)) {
                glm::mat4 worldToHFMMatrix = model->getWorldToHFMMatrix();
                glm::vec3 hfmPoint = worldToHFMMatrix * glm::vec4(point, 1.0f);
                return model->getHFMModel().convexHullContains(hfmPoint);
            }
        } else {
            return EntityItem::contains(point);
        }
    }

    return false;
}

bool RenderableModelEntityItem::shouldBePhysical() const {
    bool physicalModelLoaded = false;
    ShapeType shapeType = getShapeType();
    if (shapeType >= SHAPE_TYPE_COMPOUND && shapeType <= SHAPE_TYPE_STATIC_MESH) {
        auto model = getModel();
        // If we have a model, make sure it hasn't failed to download.
        // If it has, we'll report back that we shouldn't be physical so that physics aren't held waiting for us to be ready.
        physicalModelLoaded = model && !model->didVisualGeometryRequestFail();
        if (shapeType == SHAPE_TYPE_COMPOUND || shapeType == SHAPE_TYPE_SIMPLE_COMPOUND) {
            physicalModelLoaded &= _collisionGeometryResource && !_collisionGeometryResource->isFailed();
        }
    } else if (shapeType != SHAPE_TYPE_NONE) {
        physicalModelLoaded = true;
    }

    return physicalModelLoaded && !isDead() && !isLocalEntity() && QUrl(getModelURL()).isValid();
}

int RenderableModelEntityItem::getJointParent(int index) const {
    auto model = getModel();
    if (model) {
        return model->getRig().getJointParentIndex(index);
    }
    return -1;
}

glm::quat RenderableModelEntityItem::getAbsoluteJointRotationInObjectFrame(int index) const {
    auto model = getModel();
    if (model) {
        glm::quat result;
        if (model->getAbsoluteJointRotationInRigFrame(index, result)) {
            return result;
        }
    }
    return glm::quat();
}

glm::vec3 RenderableModelEntityItem::getAbsoluteJointTranslationInObjectFrame(int index) const {
    auto model = getModel();
    if (model) {
        glm::vec3 result;
        if (model->getAbsoluteJointTranslationInRigFrame(index, result)) {
            return result;
        }
    }
    return glm::vec3(0.0f);
}

bool RenderableModelEntityItem::setAbsoluteJointRotationInObjectFrame(int index, const glm::quat& rotation) {
    auto model = getModel();
    if (!model) {
        return false;
    }
    const Rig& rig = model->getRig();
    int jointParentIndex = rig.getJointParentIndex(index);
    if (jointParentIndex == -1) {
        return setLocalJointRotation(index, rotation);
    }

    bool success;
    AnimPose jointParentPose;
    success = rig.getAbsoluteJointPoseInRigFrame(jointParentIndex, jointParentPose);
    if (!success) {
        return false;
    }
    AnimPose jointParentInversePose = jointParentPose.inverse();

    AnimPose jointAbsolutePose; // in rig frame
    success = rig.getAbsoluteJointPoseInRigFrame(index, jointAbsolutePose);
    if (!success) {
        return false;
    }
    jointAbsolutePose.rot() = rotation;

    AnimPose jointRelativePose = jointParentInversePose * jointAbsolutePose;
    return setLocalJointRotation(index, jointRelativePose.rot());
}

bool RenderableModelEntityItem::setAbsoluteJointTranslationInObjectFrame(int index, const glm::vec3& translation) {
    auto model = getModel();
    if (!model) {
        return false;
    }
    const Rig& rig = model->getRig();

    int jointParentIndex = rig.getJointParentIndex(index);
    if (jointParentIndex == -1) {
        return setLocalJointTranslation(index, translation);
    }

    bool success;
    AnimPose jointParentPose;
    success = rig.getAbsoluteJointPoseInRigFrame(jointParentIndex, jointParentPose);
    if (!success) {
        return false;
    }
    AnimPose jointParentInversePose = jointParentPose.inverse();

    AnimPose jointAbsolutePose; // in rig frame
    success = rig.getAbsoluteJointPoseInRigFrame(index, jointAbsolutePose);
    if (!success) {
        return false;
    }
    jointAbsolutePose.trans() = translation;

    AnimPose jointRelativePose = jointParentInversePose * jointAbsolutePose;
    return setLocalJointTranslation(index, jointRelativePose.trans());
}

bool RenderableModelEntityItem::getJointMapCompleted() {
    return _jointMapCompleted;
}

glm::quat RenderableModelEntityItem::getLocalJointRotation(int index) const {
    auto model = getModel();
    if (model) {
        glm::quat result;
        if (model->getJointRotation(index, result)) {
            return result;
        }
    }
    return glm::quat();
}

glm::vec3 RenderableModelEntityItem::getLocalJointTranslation(int index) const {
    auto model = getModel();
    if (model) {
        glm::vec3 result;
        if (model->getJointTranslation(index, result)) {
            return result;
        }
    }
    return glm::vec3();
}

void RenderableModelEntityItem::setOverrideTransform(const Transform& transform, const glm::vec3& offset) {
    auto model = getModel();
    if (model) {
        model->overrideModelTransformAndOffset(transform, offset);
    }
}

bool RenderableModelEntityItem::setLocalJointRotation(int index, const glm::quat& rotation) {
    autoResizeJointArrays();
    bool result = false;
    _jointDataLock.withWriteLock([&] {
        _jointRotationsExplicitlySet = true;
        if (index >= 0 && index < _localJointData.size()) {
            auto& jointData = _localJointData[index];
            if (jointData.joint.rotation != rotation) {
                jointData.joint.rotation = rotation;
                jointData.joint.rotationSet = true;
                jointData.rotationDirty = true;
                result = true;
                _needsJointSimulation = true;
            }
        }
    });
    return result;
}

bool RenderableModelEntityItem::setLocalJointTranslation(int index, const glm::vec3& translation) {
    autoResizeJointArrays();
    bool result = false;
    _jointDataLock.withWriteLock([&] {
        _jointTranslationsExplicitlySet = true;
        if (index >= 0 && index < _localJointData.size()) {
            auto& jointData = _localJointData[index];
            if (jointData.joint.translation != translation) {
                jointData.joint.translation = translation;
                jointData.joint.translationSet = true;
                jointData.translationDirty = true;
                result = true;
                _needsJointSimulation = true;
            }
        }
    });
    return result;
}

void RenderableModelEntityItem::setJointRotations(const QVector<glm::quat>& rotations) {
    ModelEntityItem::setJointRotations(rotations);
    _needsJointSimulation = true;
}

void RenderableModelEntityItem::setJointRotationsSet(const QVector<bool>& rotationsSet) {
    ModelEntityItem::setJointRotationsSet(rotationsSet);
    _needsJointSimulation = true;
}

void RenderableModelEntityItem::setJointTranslations(const QVector<glm::vec3>& translations) {
    ModelEntityItem::setJointTranslations(translations);
    _needsJointSimulation = true;
}

void RenderableModelEntityItem::setJointTranslationsSet(const QVector<bool>& translationsSet) {
    ModelEntityItem::setJointTranslationsSet(translationsSet);
    _needsJointSimulation = true;
}

void RenderableModelEntityItem::locationChanged(bool tellPhysics, bool tellChildren) {
    DETAILED_PERFORMANCE_TIMER("locationChanged");
    EntityItem::locationChanged(tellPhysics, tellChildren);
    auto model = getModel();
    if (model && model->isLoaded()) {
        model->updateRenderItems();
    }
}

int RenderableModelEntityItem::getJointIndex(const QString& name) const {
    auto model = getModel();
    return (model && model->isLoaded()) ? model->getRig().indexOfJoint(name) : -1;
}

QStringList RenderableModelEntityItem::getJointNames() const {
    QStringList result;
    auto model = getModel();
    if (model && model->isLoaded()) {
        const Rig& rig = model->getRig();
        int jointCount = rig.getJointStateCount();
        for (int jointIndex = 0; jointIndex < jointCount; jointIndex++) {
            result << rig.nameOfJoint(jointIndex);
        }
    }
    return result;
}

scriptable::ScriptableModelBase render::entities::ModelEntityRenderer::getScriptableModel() {
    auto model = resultWithReadLock<ModelPointer>([&] { return _model; });

    if (!model || !model->isLoaded()) {
        return scriptable::ScriptableModelBase();
    }

    auto result = model->getScriptableModel();
    result.objectID = getEntity()->getID();
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        result.appendMaterials(_materials);
    }
    return result;
}

bool render::entities::ModelEntityRenderer::canReplaceModelMeshPart(int meshIndex, int partIndex) {
    // TODO: for now this method is just used to indicate that this provider generally supports mesh updates
    auto model = resultWithReadLock<ModelPointer>([this]{ return _model; });
    return model && model->isLoaded();
}

bool render::entities::ModelEntityRenderer::replaceScriptableModelMeshPart(scriptable::ScriptableModelBasePointer newModel, int meshIndex, int partIndex) {
    auto model = resultWithReadLock<ModelPointer>([this]{ return _model; });

    if (!model || !model->isLoaded()) {
        return false;
    }

    return model->replaceScriptableModelMeshPart(newModel, meshIndex, partIndex);
}

void RenderableModelEntityItem::simulateRelayedJoints() {
    ModelPointer model = getModel();
    if (model && model->isLoaded()) {
        copyAnimationJointDataToModel();
        model->simulate(0.0f);
        model->updateRenderItems();
    }
}

void RenderableModelEntityItem::stopModelOverrideIfNoParent() {
    auto model = getModel();
    if (model) {
        bool overriding = model->isOverridingModelTransformAndOffset();
        QUuid parentID = getParentID();
        if (overriding && (!_relayParentJoints || parentID.isNull())) {
            model->stopTransformAndOffsetOverride();
        }
    }
}

void RenderableModelEntityItem::copyAnimationJointDataToModel() {
    auto model = getModel();
    if (!model || !model->isLoaded()) {
        return;
    }

    bool changed { false };
    // relay any inbound joint changes from scripts/animation/network to the model/rig
    _jointDataLock.withWriteLock([&] {
        for (int index = 0; index < _localJointData.size(); ++index) {
            auto& jointData = _localJointData[index];
            if (jointData.rotationDirty) {
                model->setJointRotation(index, true, jointData.joint.rotation, 1.0f);
                jointData.rotationDirty = false;
                changed = true;
            }
            if (jointData.translationDirty) {
                model->setJointTranslation(index, true, jointData.joint.translation, 1.0f);
                jointData.translationDirty = false;
                changed = true;
            }
        }
    });

    if (changed) {
        locationChanged();
    }
}

bool RenderableModelEntityItem::readyToAnimate() const {
    return resultWithReadLock<bool>([&] {
        float firstFrame = _animationProperties.getFirstFrame();
        return (firstFrame >= 0.0f) && (firstFrame <= _animationProperties.getLastFrame());
    });
}

using namespace render;
using namespace render::entities;

ModelEntityRenderer::ModelEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {

}

void ModelEntityRenderer::setKey(bool didVisualGeometryRequestSucceed, const ModelPointer& model) {
    auto builder = ItemKey::Builder().withTypeMeta().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (!_visible) {
        builder.withInvisible();
    }

    if (!_cullWithParent && model && model->isGroupCulled()) {
        builder.withMetaCullGroup();
    } else if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    if (didVisualGeometryRequestSucceed) {
        _itemKey = builder.build();
    } else {
        _itemKey = builder.withTypeShape().build();
    }
}

ItemKey ModelEntityRenderer::getKey() {
    return _itemKey;
}

uint32_t ModelEntityRenderer::metaFetchMetaSubItems(ItemIDs& subItems) const {
    auto model = resultWithReadLock<ModelPointer>([&] { return _model; });
    if (model) {
        auto metaSubItems = model->fetchRenderItemIDs();
        subItems.insert(subItems.end(), metaSubItems.begin(), metaSubItems.end());
        return (uint32_t)metaSubItems.size();
    }
    return 0;
}

void ModelEntityRenderer::handleBlendedVertices(int blendshapeNumber, const QVector<BlendshapeOffset>& blendshapeOffsets,
                                                const QVector<int>& blendedMeshSizes, const render::ItemIDs& subItemIDs) {
    setBlendedVertices(blendshapeNumber, blendshapeOffsets, blendedMeshSizes, subItemIDs);
}

void ModelEntityRenderer::removeFromScene(const ScenePointer& scene, Transaction& transaction) {
    auto model = resultWithReadLock<ModelPointer>([&] { return _model; });
    if (model) {
        model->removeFromScene(scene, transaction);
    }
    Parent::removeFromScene(scene, transaction);
}

void ModelEntityRenderer::onRemoveFromSceneTyped(const TypedEntityPointer& entity) {
    entity->setModel({});
}

void ModelEntityRenderer::animate(const TypedEntityPointer& entity, const ModelPointer& model) {
    if (!_animation || !_animation->isLoaded()) {
        return;
    }

    QVector<EntityJointData> jointsData;

    const QVector<HFMAnimationFrame>& frames = _animation->getFramesReference(); // NOTE: getFrames() is too heavy
    int frameCount = frames.size();
    if (frameCount <= 0) {
        return;
    }

    {
        float currentFrame = fmod(entity->getAnimationCurrentFrame(), (float)(frameCount));
        if (currentFrame < 0.0f) {
            currentFrame += (float)frameCount;
        }
        int currentIntegerFrame = (int)(glm::floor(currentFrame));
        if (currentIntegerFrame == _lastKnownCurrentFrame) {
            return;
        }
        _lastKnownCurrentFrame = currentIntegerFrame;
    }

    if (_jointMapping.size() != model->getJointStateCount()) {
        qCWarning(entitiesrenderer) << "RenderableModelEntityItem::getAnimationFrame -- joint count mismatch"
                    << _jointMapping.size() << model->getJointStateCount();
        return;
    }

    QStringList animationJointNames = _animation->getHFMModel().getJointNames();
    auto& hfmJoints = _animation->getHFMModel().joints;

    auto& originalHFMJoints = model->getHFMModel().joints;
    auto& originalHFMIndices = model->getHFMModel().jointIndices;

    bool allowTranslation = entity->getAnimationAllowTranslation();

    const QVector<glm::quat>& rotations = frames[_lastKnownCurrentFrame].rotations;
    const QVector<glm::vec3>& translations = frames[_lastKnownCurrentFrame].translations;

    jointsData.resize(_jointMapping.size());
    for (int j = 0; j < _jointMapping.size(); j++) {
        int index = _jointMapping[j];

        if (index >= 0) {
            glm::mat4 translationMat;

            if (allowTranslation) {
                if (index < translations.size()) {
                    translationMat = glm::translate(translations[index]);
                }
            } else if (index < animationJointNames.size()) {
                QString jointName = hfmJoints[index].name; // Pushing this here so its not done on every entity, with the exceptions of those allowing for translation
                if (originalHFMIndices.contains(jointName)) {
                    // Making sure the joint names exist in the original model the animation is trying to apply onto. If they do, then remap and get it's translation.
                    int remappedIndex = originalHFMIndices[jointName] - 1; // JointIndeces seem to always start from 1 and the found index is always 1 higher than actual.
                    translationMat = glm::translate(originalHFMJoints[remappedIndex].translation);
                }
            }
            glm::mat4 rotationMat;
            if (index < rotations.size()) {
                rotationMat = glm::mat4_cast(hfmJoints[index].preRotation * rotations[index] * hfmJoints[index].postRotation);
            } else {
                rotationMat = glm::mat4_cast(hfmJoints[index].preRotation * hfmJoints[index].postRotation);
            }

            glm::mat4 finalMat = (translationMat * hfmJoints[index].preTransform *
                rotationMat * hfmJoints[index].postTransform);
            auto& jointData = jointsData[j];
            jointData.translation = extractTranslation(finalMat);
            jointData.translationSet = true;
            jointData.rotation = glmExtractRotation(finalMat);
            jointData.rotationSet = true;
        }
    }
    // Set the data in the entity
    entity->setAnimationJointsData(jointsData);

    entity->copyAnimationJointDataToModel();
}

bool ModelEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (entity->blendshapesChanged()) {
        return true;
    }

    // Check to see if we need to update the model bounds
    if (entity->needsUpdateModelBounds()) {
        return true;
    }

    return Parent::needsRenderUpdateFromTypedEntity(entity);
}

void ModelEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);

    _hasModel = entity->hasModel();
    QUrl modelURL = QUrl(entity->getModelURL());
    if (_parsedModelURL != modelURL) {
        _parsedModelURL = modelURL;
    }

    ModelPointer model = resultWithReadLock<ModelPointer>([&] {
        return _model;
    });

    bool visuallyReady = model && model->isLoaded() && _didLastVisualGeometryRequestSucceed && _texturesLoaded;
    entity->setVisuallyReady(visuallyReady);

    const render::ScenePointer& scene = AbstractViewStateInterface::instance()->getMain3DScene();
    render::Transaction transaction;

    // Check for removal
    if (!_hasModel) {
        if (model) {
            model->removeFromScene(scene, transaction);
            entity->bumpAncestorChainRenderableVersion();
            emit DependencyManager::get<scriptable::ModelProviderFactory>()->
                modelRemovedFromScene(entity->getEntityItemID(), NestableType::Entity, model);
            withWriteLock([&] { _model.reset(); });
        }
        _didLastVisualGeometryRequestSucceed = false;
        setKey(_didLastVisualGeometryRequestSucceed, model);
        return;
    }

    // Check for addition
    if (_hasModel && !model) {
        model = std::make_shared<Model>(nullptr, entity.get(), _created);
        connect(model.get(), &Model::requestRenderUpdate, this, &ModelEntityRenderer::requestRenderUpdate);
        connect(model.get(), &Model::setURLFinished, this, [&](bool didVisualGeometryRequestSucceed) {
            _didLastVisualGeometryRequestSucceed = didVisualGeometryRequestSucceed;
            const render::ScenePointer& scene = AbstractViewStateInterface::instance()->getMain3DScene();
            render::Transaction transaction;
            transaction.updateItem<PayloadProxyInterface>(_renderItemID, [&](PayloadProxyInterface& self) {
                const render::ScenePointer& scene = AbstractViewStateInterface::instance()->getMain3DScene();
                withWriteLock([&] {
                    setKey(didVisualGeometryRequestSucceed, _model);
                    _model->setVisibleInScene(_visible, scene);
                    _model->setCauterized(_cauterized, scene);
                    _model->setCanCastShadow(_canCastShadow, scene);
                    _model->setGroupCulled(entity->getGroupCulled(), scene);
                    _model->setTagMask(getTagMask(), scene);
                    _model->setHifiRenderLayer(getHifiRenderLayer(), scene);
                    _model->setPrimitiveMode(_primitiveMode, scene);
                    _model->setBillboardMode(_billboardMode, scene);
                    _model->setCullWithParent(_cullWithParent, scene);
                    _model->setRenderWithZones(_renderWithZones, scene);
                });
                if (didVisualGeometryRequestSucceed) {
                    emit DependencyManager::get<scriptable::ModelProviderFactory>()->
                        modelAddedToScene(entity->getEntityItemID(), NestableType::Entity, model);
                }
                entity->_originalTexturesRead = false;
                entity->_needsJointSimulation = true;
                entity->_needsToRescaleModel = true;

                entity->markDirtyFlags(Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
                entity->locationChanged();
                emit requestRenderUpdate();
            });
            scene->enqueueTransaction(transaction);
        });
        entity->setModel(model);
        withWriteLock([&] { _model = model; });
    }

    // From here on, we are guaranteed a populated model
    if (_parsedModelURL != model->getURL()) {
        _texturesLoaded = false;
        _jointMappingCompleted = false;
        model->setLoadingPriority(EntityTreeRenderer::getEntityLoadingPriority(*entity));
        model->setURL(_parsedModelURL);
    }

    // Nothing else to do unless the model is loaded
    if (!model->isLoaded()) {
        return;
    }

    // Check for initializing the model
    if (!entity->_dimensionsInitialized) {
        EntityItemProperties properties;
        properties.setLastEdited(usecTimestampNow()); // we must set the edit time since we're editing it
        auto extents = model->getMeshExtents();
        properties.setDimensions(extents.maximum - extents.minimum);
        qCDebug(entitiesrenderer) << "Autoresizing"
            << (!entity->getName().isEmpty() ? entity->getName() : entity->getModelURL())
            << "from mesh extents";

        QMetaObject::invokeMethod(DependencyManager::get<EntityScriptingInterface>().data(), "editEntity",
            Qt::QueuedConnection, Q_ARG(QUuid, entity->getEntityItemID()), Q_ARG(EntityItemProperties, properties));
    }

    if (!entity->_originalTexturesRead) {
        // Default to _originalTextures to avoid remapping immediately and lagging on load
        entity->_originalTextures = model->getTextures();
        entity->_originalTexturesRead = true;
    }

    auto textures = entity->getTextures();
    if (_textures != textures) {
        QVariantMap newTextures;
        _texturesLoaded = false;
        _textures = textures;
        newTextures = parseTexturesToMap(_textures, entity->_originalTextures);
        model->setTextures(newTextures);
    }

    if (entity->_needsJointSimulation) {
        entity->copyAnimationJointDataToModel();
    }
    entity->updateModelBounds();
    entity->stopModelOverrideIfNoParent();

    withWriteLock([&] {
        setKey(_didLastVisualGeometryRequestSucceed, model);
        model->setVisibleInScene(_visible, scene);
        model->setCauterized(_cauterized, scene);
        model->setCanCastShadow(_canCastShadow, scene);
        model->setGroupCulled(entity->getGroupCulled(), scene);
        model->setTagMask(getTagMask(), scene);
        model->setHifiRenderLayer(getHifiRenderLayer(), scene);
        model->setPrimitiveMode(_primitiveMode, scene);
        model->setBillboardMode(_billboardMode, scene);
        model->setCullWithParent(_cullWithParent, scene);
        model->setRenderWithZones(_renderWithZones, scene);
    });

    if (entity->blendshapesChanged()) {
        model->setBlendshapeCoefficients(entity->getBlendshapeCoefficientVector());
        model->updateBlendshapes();
    }

    {
        DETAILED_PROFILE_RANGE(simulation_physics, "Fixup");
        if (model->needsFixupInScene()) {
            model->removeFromScene(scene, transaction);
            render::Item::Status::Getters statusGetters;
            makeStatusGetters(entity, statusGetters);
            using namespace std::placeholders;
            model->addToScene(scene, transaction, statusGetters, std::bind(&ModelEntityRenderer::metaBlendshapeOperator, _renderItemID, _1, _2, _3, _4));
            entity->bumpAncestorChainRenderableVersion();
            processMaterials();
        }
    }

    bool needsUpdate = false;
    if (!_texturesLoaded && model->getGeometry() && model->getGeometry()->areTexturesLoaded()) {
        _texturesLoaded = true;
        needsUpdate = true;
    } else if (!_texturesLoaded) {
        emit requestRenderUpdate();
    }

    if (!_allProceduralMaterialsLoaded) {
        std::lock_guard<std::mutex> lock(_materialsLock);
        bool allProceduralMaterialsLoaded = true;
        for (auto& shapeMaterialPair : _materials) {
            auto material = shapeMaterialPair.second;
            while (!material.empty()) {
                auto mat = material.top();
                if (mat.material && mat.material->isProcedural() && !mat.material->isReady()) {
                    allProceduralMaterialsLoaded = false;
                    break;
                }
                material.pop();
            }
            if (!allProceduralMaterialsLoaded) {
                break;
            }
        }
        if (!allProceduralMaterialsLoaded) {
            emit requestRenderUpdate();
        } else {
            _allProceduralMaterialsLoaded = true;
            needsUpdate = true;
        }
    }

    // When the individual mesh parts of a model finish fading, they will mark their Model as needing updating
    // we will watch for that and ask the model to update it's render items
    if (needsUpdate || model->getRenderItemsNeedUpdate()) {
        model->updateRenderItems();
    }

    scene->enqueueTransaction(transaction);
}

void ModelEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    // The code to deal with the change of properties is now in ModelEntityItem.cpp
    // That is where _currentFrame and _lastAnimated were updated.
    if (entity->isAnimatingSomething()) {
        DETAILED_PROFILE_RANGE(simulation_physics, "Animate");

        auto animationURL = entity->getAnimationURL();
        bool animationChanged = _animationURL != animationURL;
        if (animationChanged) {
            _animationURL = animationURL;

            if (_animation) {
                // the joints have been mapped before but we have a new animation to load
                _animation.reset();
                _jointMappingCompleted = false;
            }
        }
        
        ModelPointer model = resultWithReadLock<ModelPointer>([&] {
            return _model;
        });
        if (model && model->isLoaded()) {
            if (!_jointMappingCompleted) {
                mapJoints(entity, model);
            }
            if (entity->readyToAnimate()) {
                animate(entity, model);
            }
        }
        emit requestRenderUpdate();
    }
}

void ModelEntityRenderer::setIsVisibleInSecondaryCamera(bool value) {
    Parent::setIsVisibleInSecondaryCamera(value);
    // called within a lock so no need to lock for _model
    setKey(_didLastVisualGeometryRequestSucceed, _model);
}

void ModelEntityRenderer::setRenderLayer(RenderLayer value) {
    Parent::setRenderLayer(value);
    // called within a lock so no need to lock for _model
    setKey(_didLastVisualGeometryRequestSucceed, _model);
}

void ModelEntityRenderer::setCullWithParent(bool value) {
    Parent::setCullWithParent(value);
    // called within a lock so no need to lock for _model
    setKey(_didLastVisualGeometryRequestSucceed, _model);
}

// NOTE: this only renders the "meta" portion of the Model, namely it renders debugging items
void ModelEntityRenderer::doRender(RenderArgs* args) {
    DETAILED_PROFILE_RANGE(render_detail, "MetaModelRender");
    DETAILED_PERFORMANCE_TIMER("RMEIrender");

    // If the model doesn't have visual geometry, render our bounding box as green wireframe
    static glm::vec4 greenColor(0.0f, 1.0f, 0.0f, 1.0f);
    gpu::Batch& batch = *args->_batch;
    Transform transform = getModelTransform();
    batch.setModelTransform(transform, _prevRenderTransform); // we want to include the scale as well
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }
    auto geometryCache = DependencyManager::get<GeometryCache>();
    geometryCache->renderWireCubeInstance(args, batch, greenColor, geometryCache->getShapePipelinePointer(false, false, args->_renderMethod == Args::RenderMethod::FORWARD));

#if WANT_EXTRA_DEBUGGING
    ModelPointer model = resultWithReadLock<ModelPointer>([&] {
        return _model;
    });
    if (model) {
        model->renderDebugMeshBoxes(batch, args->_renderMethod == Args::RenderMethod::FORWARD);
    }
#endif
}

void ModelEntityRenderer::mapJoints(const TypedEntityPointer& entity, const ModelPointer& model) {
    // if we don't have animation, or we're already joint mapped then bail early
    if (!entity->hasAnimation()) {
        return;
    }

    if (!_animation) {
        _animation = DependencyManager::get<AnimationCache>()->getAnimation(_animationURL);
    }

    if (_animation && _animation->isLoaded()) {
        QStringList animationJointNames = _animation->getJointNames();

        auto modelJointNames = model->getJointNames();
        if (modelJointNames.size() > 0 && animationJointNames.size() > 0) {
            _jointMapping.resize(modelJointNames.size());
            for (int i = 0; i < modelJointNames.size(); i++) {
                _jointMapping[i] = animationJointNames.indexOf(modelJointNames[i]);
            }
            _jointMappingCompleted = true;
        }
    }
}

void ModelEntityRenderer::addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName) {
    Parent::addMaterial(material, parentMaterialName);
    if (_model && _model->fetchRenderItemIDs().size() > 0) {
        _model->addMaterial(material, parentMaterialName);
    }
    if (material.material && material.material->isProcedural()) {
        _allProceduralMaterialsLoaded = false;
        emit requestRenderUpdate();
    }
}

void ModelEntityRenderer::removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName) {
    Parent::removeMaterial(material, parentMaterialName);
    if (_model && _model->fetchRenderItemIDs().size() > 0) {
        _model->removeMaterial(material, parentMaterialName);
    }
}

void ModelEntityRenderer::processMaterials() {
    assert(_model);
    std::lock_guard<std::mutex> lock(_materialsLock);
    for (auto& shapeMaterialPair : _materials) {
        auto material = shapeMaterialPair.second;
        while (!material.empty()) {
            _model->addMaterial(material.top(), shapeMaterialPair.first);
            material.pop();
        }
    }
}

void ModelEntityRenderer::metaBlendshapeOperator(render::ItemID renderItemID, int blendshapeNumber, const QVector<BlendshapeOffset>& blendshapeOffsets,
                                                 const QVector<int>& blendedMeshSizes, const render::ItemIDs& subItemIDs) {
    render::Transaction transaction;
    transaction.updateItem<PayloadProxyInterface>(renderItemID, [blendshapeNumber, blendshapeOffsets, blendedMeshSizes, subItemIDs](PayloadProxyInterface& self) {
        self.handleBlendedVertices(blendshapeNumber, blendshapeOffsets, blendedMeshSizes, subItemIDs);
    });
    AbstractViewStateInterface::instance()->getMain3DScene()->enqueueTransaction(transaction);
}
