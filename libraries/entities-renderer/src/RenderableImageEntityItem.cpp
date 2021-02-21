//
//  Created by Sam Gondelman on 11/29/18
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableImageEntityItem.h"

#include <DependencyManager.h>
#include <GeometryCache.h>

using namespace render;
using namespace render::entities;

ImageEntityRenderer::ImageEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _geometryId = DependencyManager::get<GeometryCache>()->allocateID();
}

ImageEntityRenderer::~ImageEntityRenderer() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        geometryCache->releaseID(_geometryId);
    }
}

bool ImageEntityRenderer::isTransparent() const {
    return Parent::isTransparent() || (_textureIsLoaded && _texture->getGPUTexture() && _texture->getGPUTexture()->getUsage().isAlpha()) || _alpha < 1.0f || _pulseProperties.getAlphaMode() != PulseMode::NONE;
}

void ImageEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this, entity] {
        withWriteLock([&] {
            _renderTransform = getModelTransform();
            _renderTransform.postScale(entity->getScaledDimensions());
        });
    });
}

void ImageEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    auto imageURL = entity->getImageURL();
    if (_imageURL != imageURL) {
        _imageURL = imageURL;
        if (imageURL.isEmpty()) {
            _texture.reset();
        } else {
            _texture = DependencyManager::get<TextureCache>()->getTexture(_imageURL);
        }
        _textureIsLoaded = false;
    }

    _emissive = entity->getEmissive();
    _keepAspectRatio = entity->getKeepAspectRatio();
    _subImage = entity->getSubImage();

    _color = entity->getColor();
    _alpha = entity->getAlpha();
    _pulseProperties = entity->getPulseProperties();

    if (!_textureIsLoaded) {
        emit requestRenderUpdate();
    }
    _textureIsLoaded = _texture && (_texture->isLoaded() || _texture->isFailed());
}

ShapeKey ImageEntityRenderer::getShapeKey() {
    auto builder = render::ShapeKey::Builder().withoutCullFace().withDepthBias();
    if (isTransparent()) {
        builder.withTranslucent();
    }

    if (_emissive) {
        builder.withUnlit();
    }

    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }

    return builder.build();
}

void ImageEntityRenderer::doRender(RenderArgs* args) {
    glm::vec4 color = glm::vec4(toGlm(_color), _alpha);
    color = EntityRenderer::calculatePulseColor(color, _pulseProperties, _created);
    Transform transform;
    withReadLock([&] {
        transform = _renderTransform;
    });

    if (!_visible || !_texture || !_texture->isLoaded() || color.a == 0.0f) {
        return;
    }

    Q_ASSERT(args->_batch);
    gpu::Batch* batch = args->_batch;

    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));

    batch->setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }
    batch->setResourceTexture(0, _texture->getGPUTexture());

    float imageWidth = _texture->getWidth();
    float imageHeight = _texture->getHeight();

    QRect fromImage;
    if (_subImage.width() <= 0) {
        fromImage.setX(0);
        fromImage.setWidth(imageWidth);
    } else {
        float scaleX = imageWidth / _texture->getOriginalWidth();
        fromImage.setX(scaleX * _subImage.x());
        fromImage.setWidth(scaleX * _subImage.width());
    }

    if (_subImage.height() <= 0) {
        fromImage.setY(0);
        fromImage.setHeight(imageHeight);
    } else {
        float scaleY = imageHeight / _texture->getOriginalHeight();
        fromImage.setY(scaleY * _subImage.y());
        fromImage.setHeight(scaleY * _subImage.height());
    }

    float maxSize = glm::max(fromImage.width(), fromImage.height());
    float x = _keepAspectRatio ? fromImage.width() / (2.0f * maxSize) : 0.5f;
    float y = _keepAspectRatio ? fromImage.height() / (2.0f * maxSize) : 0.5f;

    glm::vec2 texCoordBottomLeft((fromImage.x() + 0.5f) / imageWidth, (fromImage.y() + fromImage.height() - 0.5f) / imageHeight);
    glm::vec2 texCoordTopRight((fromImage.x() + fromImage.width() - 0.5f) / imageWidth, (fromImage.y() + 0.5f) / imageHeight);

    DependencyManager::get<GeometryCache>()->renderQuad(
        *batch, glm::vec2(-x, -y), glm::vec2(x, y), texCoordBottomLeft, texCoordTopRight,
        color, _geometryId
    );

    batch->setResourceTexture(0, nullptr);
}
