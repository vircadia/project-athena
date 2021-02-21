//
//  Created by Sam Gondelman on 1/22/19
//  Copyright 2019 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableGizmoEntityItem.h"

#include <DependencyManager.h>
#include <GeometryCache.h>

using namespace render;
using namespace render::entities;

GizmoEntityRenderer::GizmoEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {}

GizmoEntityRenderer::~GizmoEntityRenderer() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        if (_ringGeometryID) {
            geometryCache->releaseID(_ringGeometryID);
        }
        if (_majorTicksGeometryID) {
            geometryCache->releaseID(_majorTicksGeometryID);
        }
        if (_minorTicksGeometryID) {
            geometryCache->releaseID(_minorTicksGeometryID);
        }
    }
}

bool GizmoEntityRenderer::isTransparent() const {
    bool ringTransparent = _gizmoType == GizmoType::RING && (_ringProperties.getInnerStartAlpha() < 1.0f ||
        _ringProperties.getInnerEndAlpha() < 1.0f || _ringProperties.getOuterStartAlpha() < 1.0f ||
        _ringProperties.getOuterEndAlpha() < 1.0f);

    return Parent::isTransparent() || ringTransparent;
}

void GizmoEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this, entity] {
        withWriteLock([&] {
            _renderTransform = getModelTransform();
            _renderTransform.postScale(entity->getScaledDimensions());
        });
    });
}

void GizmoEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    bool dirty = false;
    RingGizmoPropertyGroup ringProperties = entity->getRingProperties();
    _gizmoType = entity->getGizmoType();
    if (_ringProperties != ringProperties) {
        _ringProperties = ringProperties;
        dirty = true;
    }

    if (dirty || _prevPrimitiveMode != _primitiveMode || !_ringGeometryID || !_majorTicksGeometryID || !_minorTicksGeometryID) {
        _prevPrimitiveMode = _primitiveMode;
        auto geometryCache = DependencyManager::get<GeometryCache>();
        if (!_ringGeometryID) {
            _ringGeometryID = geometryCache->allocateID();
        }

        const float FULL_CIRCLE = 360.0f;
        const float SLICES = 180.0f;
        const float SLICE_ANGLE_RADIANS = glm::radians(FULL_CIRCLE / SLICES);
        if (_primitiveMode == PrimitiveMode::SOLID) {

            QVector<glm::vec3> points;
            QVector<glm::vec4> colors;

            vec4 innerStartColor = vec4(toGlm(ringProperties.getInnerStartColor()), ringProperties.getInnerStartAlpha());
            vec4 outerStartColor = vec4(toGlm(ringProperties.getOuterStartColor()), ringProperties.getOuterStartAlpha());
            vec4 innerEndColor = vec4(toGlm(ringProperties.getInnerEndColor()), ringProperties.getInnerEndAlpha());
            vec4 outerEndColor = vec4(toGlm(ringProperties.getOuterEndColor()), ringProperties.getOuterEndAlpha());
            float startAtRadians = glm::radians(ringProperties.getStartAngle());
            float endAtRadians = glm::radians(ringProperties.getEndAngle());

            const auto totalRange = endAtRadians - startAtRadians;
            if (ringProperties.getInnerRadius() <= 0) {
                _solidPrimitive = gpu::TRIANGLE_FAN;
                points << vec3();
                colors << innerStartColor;
                for (float angleRadians = startAtRadians; angleRadians < endAtRadians; angleRadians += SLICE_ANGLE_RADIANS) {
                    float range = (angleRadians - startAtRadians) / totalRange;
                    points << 0.5f * glm::vec3(cosf(angleRadians), 0.0f, sinf(angleRadians));
                    colors << glm::mix(outerStartColor, outerEndColor, range);
                }
                points << 0.5f * glm::vec3(cosf(endAtRadians), 0.0f, sinf(endAtRadians));
                colors << outerEndColor;
            } else {
                _solidPrimitive = gpu::TRIANGLE_STRIP;
                for (float angleRadians = startAtRadians; angleRadians < endAtRadians; angleRadians += SLICE_ANGLE_RADIANS) {
                    float range = (angleRadians - startAtRadians) / totalRange;

                    points << 0.5f * glm::vec3(cosf(angleRadians) * ringProperties.getInnerRadius(), 0.0f, sinf(angleRadians) * ringProperties.getInnerRadius());
                    colors << glm::mix(innerStartColor, innerEndColor, range);

                    points << 0.5f * glm::vec3(cosf(angleRadians), 0.0f, sinf(angleRadians));
                    colors << glm::mix(outerStartColor, outerEndColor, range);
                }
                points << 0.5f * glm::vec3(cosf(endAtRadians) * ringProperties.getInnerRadius(), 0.0f, sinf(endAtRadians) * ringProperties.getInnerRadius());
                colors << innerEndColor;

                points << 0.5f * glm::vec3(cosf(endAtRadians), 0.0f, sinf(endAtRadians));
                colors << outerEndColor;
            }
            geometryCache->updateVertices(_ringGeometryID, points, colors);
        } else {
            _solidPrimitive = gpu::LINE_STRIP;
            QVector<glm::vec3> points;

            float startAtRadians = glm::radians(ringProperties.getStartAngle());
            float endAtRadians = glm::radians(ringProperties.getEndAngle());

            float angleRadians = startAtRadians;
            glm::vec3 firstPoint = 0.5f * glm::vec3(cosf(angleRadians), 0.0f, sinf(angleRadians));
            points << firstPoint;

            while (angleRadians < endAtRadians) {
                angleRadians += SLICE_ANGLE_RADIANS;
                glm::vec3 thisPoint = 0.5f * glm::vec3(cosf(angleRadians), 0.0f, sinf(angleRadians));
                points << thisPoint;
            }

            // get the last slice portion....
            angleRadians = endAtRadians;
            glm::vec3 lastPoint = 0.5f * glm::vec3(cosf(angleRadians), 0.0f, sinf(angleRadians));
            points << lastPoint;
            geometryCache->updateVertices(_ringGeometryID, points, glm::vec4(toGlm(ringProperties.getOuterStartColor()), ringProperties.getOuterStartAlpha()));
        }

        if (ringProperties.getHasTickMarks()) {
            if (ringProperties.getMajorTickMarksAngle() > 0.0f && ringProperties.getMajorTickMarksLength() != 0.0f) {
                QVector<glm::vec3> points;
                if (!_majorTicksGeometryID) {
                    _majorTicksGeometryID = geometryCache->allocateID();
                }

                float startAngle = ringProperties.getStartAngle();
                float tickMarkAngle = ringProperties.getMajorTickMarksAngle();
                float angle = startAngle - fmodf(startAngle, tickMarkAngle) + tickMarkAngle;

                float tickMarkLength = 0.5f * ringProperties.getMajorTickMarksLength();
                float startRadius = (tickMarkLength > 0.0f) ? 0.5f * ringProperties.getInnerRadius() : 0.5f;
                float endRadius = startRadius + tickMarkLength;

                while (angle <= ringProperties.getEndAngle()) {
                    float angleInRadians = glm::radians(angle);

                    glm::vec3 thisPointA = startRadius * glm::vec3(cosf(angleInRadians), 0.0f, sinf(angleInRadians));
                    glm::vec3 thisPointB = endRadius * glm::vec3(cosf(angleInRadians), 0.0f, sinf(angleInRadians));

                    points << thisPointA << thisPointB;

                    angle += tickMarkAngle;
                }

                glm::vec4 color(toGlm(ringProperties.getMajorTickMarksColor()), 1.0f); // TODO: alpha
                geometryCache->updateVertices(_majorTicksGeometryID, points, color);
            }
            if (ringProperties.getMinorTickMarksAngle() > 0.0f && ringProperties.getMinorTickMarksLength() != 0.0f) {
                QVector<glm::vec3> points;
                if (!_minorTicksGeometryID) {
                    _minorTicksGeometryID = geometryCache->allocateID();
                }

                float startAngle = ringProperties.getStartAngle();
                float tickMarkAngle = ringProperties.getMinorTickMarksAngle();
                float angle = startAngle - fmodf(startAngle, tickMarkAngle) + tickMarkAngle;

                float tickMarkLength = 0.5f * ringProperties.getMinorTickMarksLength();
                float startRadius = (tickMarkLength > 0.0f) ? 0.5f * ringProperties.getInnerRadius() : 0.5f;
                float endRadius = startRadius + tickMarkLength;

                while (angle <= ringProperties.getEndAngle()) {
                    float angleInRadians = glm::radians(angle);

                    glm::vec3 thisPointA = startRadius * glm::vec3(cosf(angleInRadians), 0.0f, sinf(angleInRadians));
                    glm::vec3 thisPointB = endRadius * glm::vec3(cosf(angleInRadians), 0.0f, sinf(angleInRadians));

                    points << thisPointA << thisPointB;

                    angle += tickMarkAngle;
                }

                glm::vec4 color(toGlm(ringProperties.getMinorTickMarksColor()), 1.0f); // TODO: alpha
                geometryCache->updateVertices(_minorTicksGeometryID, points, color);
            }
        }
    }
}

Item::Bound GizmoEntityRenderer::getBound(RenderArgs* args) {
    auto bound = Parent::getBound(args);
    if (_ringProperties.getHasTickMarks()) {
        glm::vec3 scale = bound.getScale();
        for (int i = 0; i < 3; i += 2) {
            if (_ringProperties.getMajorTickMarksLength() + _ringProperties.getInnerRadius() > 1.0f) {
                scale[i] *= _ringProperties.getMajorTickMarksLength() + _ringProperties.getInnerRadius();
            } else if (_ringProperties.getMajorTickMarksLength() < -2.0f) {
                scale[i] *= -_ringProperties.getMajorTickMarksLength() - 1.0f;
            }

            if (_ringProperties.getMinorTickMarksLength() + _ringProperties.getInnerRadius() > 1.0f) {
                scale[i] *= _ringProperties.getMinorTickMarksLength() + _ringProperties.getInnerRadius();
            } else if (_ringProperties.getMinorTickMarksLength() < -2.0f) {
                scale[i] *= -_ringProperties.getMinorTickMarksLength() - 1.0f;
            }
        }

        bound.setScaleStayCentered(scale);
        return bound;
    }
    return bound;
}

ShapeKey GizmoEntityRenderer::getShapeKey() {
    auto builder = render::ShapeKey::Builder().withoutCullFace();
    if (isTransparent()) {
        builder.withTranslucent();
    }
    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withUnlit().withDepthBias();
    }
    return builder.build();
}

void GizmoEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableGizmoEntityItem::render");
    Q_ASSERT(args->_batch);

    gpu::Batch& batch = *args->_batch;
    auto geometryCache = DependencyManager::get<GeometryCache>();

    if (_gizmoType == GizmoType::RING) {
        Transform transform;
        bool hasTickMarks = _ringProperties.getHasTickMarks();
        glm::vec4 tickProperties = glm::vec4(_ringProperties.getMajorTickMarksAngle(), _ringProperties.getMajorTickMarksLength(),
                                             _ringProperties.getMinorTickMarksAngle(), _ringProperties.getMinorTickMarksLength());

        bool transparent;
        withReadLock([&] {
            transform = _renderTransform;
            transparent = isTransparent();
        });

        bool wireframe = render::ShapeKey(args->_globalShapeKey).isWireframe() || _primitiveMode == PrimitiveMode::LINES;
        bool forward = _renderLayer != RenderLayer::WORLD || args->_renderMethod == Args::RenderMethod::FORWARD;

        geometryCache->bindSimpleProgram(batch, false, transparent, wireframe, true, true, forward, graphics::MaterialKey::CULL_NONE);

        transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
            args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition(), true));
        batch.setModelTransform(transform, _prevRenderTransform);
        if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
            _prevRenderTransform = transform;
        }

        // Background circle
        geometryCache->renderVertices(batch, wireframe ? gpu::LINE_STRIP : _solidPrimitive, _ringGeometryID);

        // Ticks
        if (hasTickMarks) {
            if (tickProperties.x > 0.0f && tickProperties.y != 0.0f) {
                geometryCache->renderVertices(batch, gpu::LINES, _majorTicksGeometryID);
            }
            if (tickProperties.z > 0.0f && tickProperties.w != 0.0f) {
                geometryCache->renderVertices(batch, gpu::LINES, _minorTicksGeometryID);
            }
        }
    }
}
