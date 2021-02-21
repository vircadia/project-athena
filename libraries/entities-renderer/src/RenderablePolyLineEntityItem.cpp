//
//  RenderablePolyLineEntityItem.cpp
//  libraries/entities-renderer/src/
//
//  Created by Eric Levin on 8/10/15
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderablePolyLineEntityItem.h"
#include <ParticleEffectEntityItem.h>

#include <GeometryCache.h>
#include <StencilMaskPass.h>
#include <TextureCache.h>
#include <PathUtils.h>
#include <PerfStat.h>
#include <shaders/Shaders.h>

#include "paintStroke_Shared.slh"

using namespace render;
using namespace render::entities;

std::map<std::pair<render::Args::RenderMethod, bool>, gpu::PipelinePointer> PolyLineEntityRenderer::_pipelines;

static const QUrl DEFAULT_POLYLINE_TEXTURE = PathUtils::resourcesUrl("images/paintStroke.png");

PolyLineEntityRenderer::PolyLineEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _texture = DependencyManager::get<TextureCache>()->getTexture(DEFAULT_POLYLINE_TEXTURE);

    { // Initialize our buffers
        _polylineDataBuffer = std::make_shared<gpu::Buffer>();
        _polylineDataBuffer->resize(sizeof(PolylineData));
        PolylineData data { glm::vec2(_faceCamera, _glow), glm::vec2(0.0f) };
        _polylineDataBuffer->setSubData(0, data);

        _polylineGeometryBuffer = std::make_shared<gpu::Buffer>();
    }
}

void PolyLineEntityRenderer::updateModelTransformAndBound() {
    bool success = false;
    auto newModelTransform = _entity->getTransformToCenter(success);
    if (success) {
        _modelTransform = newModelTransform;

        auto lineEntity = std::static_pointer_cast<PolyLineEntityItem>(_entity);
        AABox bound;
        lineEntity->computeTightLocalBoundingBox(bound);
        bound.transform(newModelTransform);
        _bound = bound;
    }
}

bool PolyLineEntityRenderer::isTransparent() const {
    return _glow || (_textureLoaded && _texture->getGPUTexture() && _texture->getGPUTexture()->getUsage().isAlpha());
}

void PolyLineEntityRenderer::buildPipelines() {
    static const std::vector<std::pair<render::Args::RenderMethod, bool>> keys = {
        { render::Args::DEFERRED, false }, { render::Args::DEFERRED, true }, { render::Args::FORWARD, false }, { render::Args::FORWARD, true },
    };

    for (auto& key : keys) {
        gpu::ShaderPointer program;
        render::Args::RenderMethod renderMethod = key.first;
        bool transparent = key.second;

        if (renderMethod == render::Args::DEFERRED) {
            if (transparent) {
                program = gpu::Shader::createProgram(shader::entities_renderer::program::paintStroke_translucent);
            } else {
                program = gpu::Shader::createProgram(shader::entities_renderer::program::paintStroke);
            }
        } else { // render::Args::FORWARD
            program = gpu::Shader::createProgram(shader::entities_renderer::program::paintStroke_forward);
        }

        gpu::StatePointer state = gpu::StatePointer(new gpu::State());

        state->setCullMode(gpu::State::CullMode::CULL_NONE);
        state->setDepthTest(true, !transparent, gpu::LESS_EQUAL);
        if (transparent) {
            PrepareStencil::testMask(*state);
        } else {
            PrepareStencil::testMaskDrawShape(*state);
        }

        state->setBlendFunction(transparent, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
            gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);

        _pipelines[key] = gpu::Pipeline::create(program, state);
    }
}

ItemKey PolyLineEntityRenderer::getKey() {
    auto builder = ItemKey::Builder::opaqueShape().withTypeMeta().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (isTransparent()) {
        builder.withTransparent();
    }

    if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    return builder.build();
}

ShapeKey PolyLineEntityRenderer::getShapeKey() {
    auto builder = ShapeKey::Builder().withOwnPipeline().withoutCullFace();
    if (isTransparent()) {
        builder.withTranslucent();
    }
    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }
    return builder.build();
}

bool PolyLineEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (entity->pointsChanged() || entity->widthsChanged() || entity->normalsChanged() || entity->texturesChanged() || entity->colorsChanged()) {
        return true;
    }

    return Parent::needsRenderUpdateFromTypedEntity(entity);
}

void PolyLineEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this] {
        withWriteLock([&] {
            _renderTransform = getModelTransform();
        });
    });
}

void PolyLineEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    auto pointsChanged = entity->pointsChanged();
    auto widthsChanged = entity->widthsChanged();
    auto normalsChanged = entity->normalsChanged();
    auto colorsChanged = entity->colorsChanged();

    bool isUVModeStretch = entity->getIsUVModeStretch();
    bool glow = entity->getGlow();
    bool faceCamera = entity->getFaceCamera();

    entity->resetPolyLineChanged();

    // Textures
    if (entity->texturesChanged()) {
        entity->resetTexturesChanged();
        QUrl entityTextures = DEFAULT_POLYLINE_TEXTURE;
        auto textures = entity->getTextures();
        if (!textures.isEmpty()) {
            entityTextures = QUrl(textures);
        }
        _texture = DependencyManager::get<TextureCache>()->getTexture(entityTextures);
        _textureAspectRatio = 1.0f;
        _textureLoaded = false;
    }

    if (!_textureLoaded) {
        emit requestRenderUpdate();
    }

    bool textureChanged = false;
    if (!_textureLoaded && _texture && _texture->isLoaded()) {
        textureChanged = true;
        _textureAspectRatio = (float)_texture->getOriginalHeight() / (float)_texture->getOriginalWidth();
        _textureLoaded = true;
    }

    // Data
    bool faceCameraChanged = faceCamera != _faceCamera;
    if (faceCameraChanged || glow != _glow) {
        _faceCamera = faceCamera;
        _glow = glow;
        updateData();
    }

    // Geometry
    if (pointsChanged) {
        _points = entity->getLinePoints();
    }
    if (widthsChanged) {
        _widths = entity->getStrokeWidths();
    }
    if (normalsChanged) {
        _normals = entity->getNormals();
    }
    if (colorsChanged) {
        _colors = entity->getStrokeColors();
        _color = toGlm(entity->getColor());
    }

    bool uvModeStretchChanged = _isUVModeStretch != isUVModeStretch;
    _isUVModeStretch = isUVModeStretch;

    if (uvModeStretchChanged || pointsChanged || widthsChanged || normalsChanged || colorsChanged || textureChanged || faceCameraChanged) {
        updateGeometry();
    }
}

void PolyLineEntityRenderer::updateGeometry() {
    int maxNumVertices = std::min(_points.length(), _normals.length());
    if (maxNumVertices < 1) {
        return;
    }
    bool doesStrokeWidthVary = false;
    if (_widths.size() > 0) {
        float prevWidth = _widths[0];
        for (int i = 1; i < maxNumVertices; i++) {
            float width = i < _widths.length() ? _widths[i] : PolyLineEntityItem::DEFAULT_LINE_WIDTH;
            if (width != prevWidth) {
                doesStrokeWidthVary = true;
                break;
            }
            if (i > _widths.length() + 1) {
                break;
            }
            prevWidth = width;
        }
    }

    float uCoordInc = 1.0f / maxNumVertices;
    float uCoord = 0.0f;
    float accumulatedDistance = 0.0f;
    float accumulatedStrokeWidth = 0.0f;
    glm::vec3 binormal;

    std::vector<PolylineVertex> vertices;
    vertices.reserve(maxNumVertices);

    for (int i = 0; i < maxNumVertices; i++) {
        // Position
        glm::vec3 point = _points[i];
        // uCoord
        float width = i < _widths.size() ? _widths[i] : PolyLineEntityItem::DEFAULT_LINE_WIDTH;

        if (i > 0) { // First uCoord is 0.0f
            if (!_isUVModeStretch) {
                accumulatedDistance += glm::distance(point, _points[i - 1]);

                if (doesStrokeWidthVary) {
                    //If the stroke varies along the line the texture will stretch more or less depending on the speed
                    //because it looks better than using the same method as below
                    accumulatedStrokeWidth += width;
                    float increaseValue = 1;
                    if (accumulatedStrokeWidth != 0) {
                        float newUcoord = glm::ceil((_textureAspectRatio * accumulatedDistance) / (accumulatedStrokeWidth / i));
                        increaseValue = newUcoord - uCoord;
                    }

                    increaseValue = increaseValue > 0 ? increaseValue : 1;
                    uCoord += increaseValue;
                } else {
                    // If the stroke width is constant then the textures should keep the aspect ratio along the line
                    uCoord = (_textureAspectRatio * accumulatedDistance) / width;
                }
            } else {
                uCoord += uCoordInc;
            }
        }

        // Color
        glm::vec3 color = i < _colors.length() ? _colors[i] : _color;

        // Normal
        glm::vec3 normal = _normals[i];

        // Binormal
        // For last point we can assume binormals are the same since it represents the last two vertices of quad
        if (i < maxNumVertices - 1) {
            glm::vec3 tangent = _points[i + 1] - point;

            if (_faceCamera) {
                // In faceCamera mode, we actually pass the tangent, and recompute the binormal in the shader
                binormal = tangent;
            } else {
                binormal = glm::normalize(glm::cross(tangent, normal));

                // Check to make sure binormal is not a NAN. If it is, don't add to vertices vector
                if (glm::any(glm::isnan(binormal))) {
                    continue;
                }
            }
        }

        PolylineVertex vertex = { glm::vec4(point, uCoord), glm::vec4(color, 1.0f), glm::vec4(normal, 0.0f), glm::vec4(binormal, 0.5f * width) };
        vertices.push_back(vertex);
    }

    _numVertices = vertices.size();
    _polylineGeometryBuffer->setData(vertices.size() * sizeof(PolylineVertex), (const gpu::Byte*) vertices.data());
}

void PolyLineEntityRenderer::updateData() {
    PolylineData data { glm::vec2(_faceCamera, _glow), glm::vec2(0.0f) };
    _polylineDataBuffer->setSubData(0, data);
}

void PolyLineEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderablePolyLineEntityItem::render");
    Q_ASSERT(args->_batch);
    gpu::Batch& batch = *args->_batch;

    Transform transform;
    gpu::TexturePointer texture = _textureLoaded ? _texture->getGPUTexture() : DependencyManager::get<TextureCache>()->getWhiteTexture();
    withReadLock([&] {
        transform = _renderTransform;
    });

    batch.setResourceBuffer(0, _polylineGeometryBuffer);
    batch.setUniformBuffer(0, _polylineDataBuffer);

    if (_numVertices < 2) {
        return;
    }

    if (_pipelines.empty()) {
        buildPipelines();
    }

    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    batch.setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }

    batch.setPipeline(_pipelines[{args->_renderMethod, isTransparent()}]);
    batch.setResourceTexture(0, texture);
    batch.draw(gpu::TRIANGLE_STRIP, (gpu::uint32)(2 * _numVertices), 0);
}
