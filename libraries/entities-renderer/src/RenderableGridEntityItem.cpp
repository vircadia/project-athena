//
//  Created by Sam Gondelman on 11/29/18
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableGridEntityItem.h"

#include <DependencyManager.h>
#include <GeometryCache.h>

using namespace render;
using namespace render::entities;

GridEntityRenderer::GridEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _geometryId = DependencyManager::get<GeometryCache>()->allocateID();
}

GridEntityRenderer::~GridEntityRenderer() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        geometryCache->releaseID(_geometryId);
    }
}

bool GridEntityRenderer::isTransparent() const {
    return Parent::isTransparent() || _alpha < 1.0f || _pulseProperties.getAlphaMode() != PulseMode::NONE;
}

void GridEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this, entity] {
        withWriteLock([&] {
            _dimensions = entity->getScaledDimensions();
            _renderTransform = getModelTransform();
        });
    });
}

void GridEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    _color = entity->getColor();
    _alpha = entity->getAlpha();
    _pulseProperties = entity->getPulseProperties();

    _followCamera = entity->getFollowCamera();
    _majorGridEvery = entity->getMajorGridEvery();
    _minorGridEvery = entity->getMinorGridEvery();
}

Item::Bound GridEntityRenderer::getBound(RenderArgs* args) {
    if (_followCamera) {
        // This is a UI element that should always be in view, lie to the octree to avoid culling
        const AABox DOMAIN_BOX = AABox(glm::vec3(-TREE_SCALE / 2), TREE_SCALE);
        return DOMAIN_BOX;
    }
    return Parent::getBound(args);
}

ShapeKey GridEntityRenderer::getShapeKey() {
    auto builder = render::ShapeKey::Builder().withOwnPipeline().withUnlit().withDepthBias();

    if (isTransparent()) {
        builder.withTranslucent();
    }

    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }

    return builder.build();
}

void GridEntityRenderer::doRender(RenderArgs* args) {
    glm::vec4 color = glm::vec4(toGlm(_color), _alpha);
    color = EntityRenderer::calculatePulseColor(color, _pulseProperties, _created);
    glm::vec3 dimensions;
    Transform renderTransform;
    withReadLock([&] {
        dimensions = _dimensions;
        renderTransform = _renderTransform;
    });

    if (!_visible || color.a == 0.0f) {
        return;
    }

    bool forward = _renderLayer != RenderLayer::WORLD || args->_renderMethod == Args::RenderMethod::FORWARD;

    auto batch = args->_batch;

    Transform transform;
    transform.setScale(dimensions);
    transform.setRotation(renderTransform.getRotation());
    if (_followCamera) {
        // Get the camera position rounded to the nearest major grid line
        // This grid is for UI and should lie on worldlines
        glm::vec3 localCameraPosition = glm::inverse(transform.getRotation()) * (args->getViewFrustum().getPosition() - renderTransform.getTranslation());
        localCameraPosition.z = 0;
        localCameraPosition = (float)_majorGridEvery * glm::round(localCameraPosition / (float)_majorGridEvery);
        transform.setTranslation(renderTransform.getTranslation() + transform.getRotation() * localCameraPosition);
    } else {
        transform.setTranslation(renderTransform.getTranslation());
    }

    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    batch->setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }

    auto minCorner = glm::vec2(-0.5f, -0.5f);
    auto maxCorner = glm::vec2(0.5f, 0.5f);
    float majorGridRowDivisions = dimensions.x / _majorGridEvery;
    float majorGridColDivisions = dimensions.y / _majorGridEvery;
    float minorGridRowDivisions = dimensions.x / _minorGridEvery;
    float minorGridColDivisions = dimensions.y / _minorGridEvery;

    const float MINOR_GRID_EDGE = 0.0025f;
    const float MAJOR_GRID_EDGE = 0.005f;
    DependencyManager::get<GeometryCache>()->renderGrid(*batch, minCorner, maxCorner,
        minorGridRowDivisions, minorGridColDivisions, MINOR_GRID_EDGE,
        majorGridRowDivisions, majorGridColDivisions, MAJOR_GRID_EDGE,
        color, forward, _geometryId);
}