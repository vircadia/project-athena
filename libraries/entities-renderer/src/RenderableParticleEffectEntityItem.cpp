//
//  RenderableParticleEffectEntityItem.cpp
//  interface/src
//
//  Created by Jason Rickwald on 3/2/15.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableParticleEffectEntityItem.h"
#include <StencilMaskPass.h>

#include <GeometryCache.h>
#include <shaders/Shaders.h>

#include <glm/gtx/transform.hpp>

using namespace render;
using namespace render::entities;

static uint8_t CUSTOM_PIPELINE_NUMBER = 0;
static gpu::Stream::FormatPointer _vertexFormat;
static std::weak_ptr<gpu::Pipeline> _texturedPipeline;
static std::weak_ptr<gpu::Pipeline> _texturedForwardPipeline;

static ShapePipelinePointer shapePipelineFactory(const ShapePlumber& plumber, const ShapeKey& key, RenderArgs* args) {
    auto texturedPipeline = _texturedPipeline.lock();
    auto texturedForwardPipeline = _texturedForwardPipeline.lock();
    if (!texturedPipeline || !texturedForwardPipeline) {
        auto state = std::make_shared<gpu::State>();
        state->setCullMode(gpu::State::CULL_BACK);
        state->setDepthTest(true, false, gpu::LESS_EQUAL);
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE,
            gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
        PrepareStencil::testMaskResetNoAA(*state);

        auto program = gpu::Shader::createProgram(shader::entities_renderer::program::textured_particle);
        _texturedPipeline = texturedPipeline = gpu::Pipeline::create(program, state);

        auto forwardProgram = gpu::Shader::createProgram(shader::entities_renderer::program::textured_particle_forward);
        _texturedForwardPipeline = texturedPipeline = gpu::Pipeline::create(program, state);
    }

    return std::make_shared<render::ShapePipeline>(args->_renderMethod == render::Args::FORWARD ? texturedForwardPipeline : texturedPipeline, nullptr, nullptr, nullptr);
}

struct GpuParticle {
    GpuParticle(const glm::vec3& xyzIn, const glm::vec2& uvIn) : xyz(xyzIn), uv(uvIn) {}
    glm::vec3 xyz; // Position
    glm::vec2 uv; // Lifetime + seed
};

using GpuParticles = std::vector<GpuParticle>;

ParticleEffectEntityRenderer::ParticleEffectEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    ParticleUniforms uniforms;
    _uniformBuffer = std::make_shared<Buffer>(sizeof(ParticleUniforms), (const gpu::Byte*) &uniforms);

    static std::once_flag once;
    std::call_once(once, [] {
        // As we create the first ParticuleSystem entity, let s register its special shapePIpeline factory:
        CUSTOM_PIPELINE_NUMBER = render::ShapePipeline::registerCustomShapePipelineFactory(shapePipelineFactory);
        _vertexFormat = std::make_shared<Format>();
        _vertexFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element::VEC3F_XYZ,
            offsetof(GpuParticle, xyz), gpu::Stream::PER_INSTANCE);
        _vertexFormat->setAttribute(gpu::Stream::COLOR, 0, gpu::Element::VEC2F_UV,
            offsetof(GpuParticle, uv), gpu::Stream::PER_INSTANCE);
    });
}

void ParticleEffectEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this] {
        withWriteLock([&] {
            _renderTransform = getModelTransform();
        });
    });
}

void ParticleEffectEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    auto newParticleProperties = entity->getParticleProperties();
    if (!newParticleProperties.valid()) {
        qCWarning(entitiesrenderer) << "Bad particle properties";
    }

    if (_particleProperties != newParticleProperties) {
        _timeUntilNextEmit = 0;
        _particleProperties = newParticleProperties;
        if (!_prevEmitterShouldTrailInitialized) {
            _prevEmitterShouldTrailInitialized = true;
            _prevEmitterShouldTrail = _particleProperties.emission.shouldTrail;
        }
    }

    _pulseProperties = entity->getPulseProperties();
    _shapeType = entity->getShapeType();
    QString compoundShapeURL = entity->getCompoundShapeURL();
    if (_compoundShapeURL != compoundShapeURL) {
        _compoundShapeURL = compoundShapeURL;
        _hasComputedTriangles = false;
        fetchGeometryResource();
    }
    _emitting = entity->getIsEmitting();

    if (_particleProperties.textures.isEmpty()) {
        if (_networkTexture) {
            _networkTexture.reset();
        }

        _textureLoaded = true;
        entity->setVisuallyReady(true);
    } else {
        if (!_networkTexture || _networkTexture->getURL() != QUrl(_particleProperties.textures)) {
            _networkTexture = DependencyManager::get<TextureCache>()->getTexture(_particleProperties.textures);
            _textureLoaded = false;
            entity->setVisuallyReady(false);
        }

        if (!_textureLoaded) {
            emit requestRenderUpdate();
        }

        if (_networkTexture && (_networkTexture->isLoaded() || _networkTexture->isFailed())) {
            entity->setVisuallyReady(true);
            _textureLoaded = true;
        }
    }

    // Fill in Uniforms structure
    ParticleUniforms particleUniforms;
    particleUniforms.radius.start = _particleProperties.radius.range.start;
    particleUniforms.radius.middle = _particleProperties.radius.gradient.target;
    particleUniforms.radius.finish = _particleProperties.radius.range.finish;
    particleUniforms.radius.spread = _particleProperties.radius.gradient.spread;
    particleUniforms.spin.start = _particleProperties.spin.range.start;
    particleUniforms.spin.middle = _particleProperties.spin.gradient.target;
    particleUniforms.spin.finish = _particleProperties.spin.range.finish;
    particleUniforms.spin.spread = _particleProperties.spin.gradient.spread;
    particleUniforms.lifespan = _particleProperties.lifespan;
    particleUniforms.rotateWithEntity = _particleProperties.rotateWithEntity ? 1 : 0;
    // Update particle uniforms
    _uniformBuffer.edit<ParticleUniforms>() = particleUniforms;
}

ItemKey ParticleEffectEntityRenderer::getKey() {
    // FIXME: implement isTransparent() for particles and an opaque pipeline
    auto builder = ItemKey::Builder::transparentShape().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (!_visible) {
        builder.withInvisible();
    }

    if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    return builder.build();
}

ShapeKey ParticleEffectEntityRenderer::getShapeKey() {
    auto builder = ShapeKey::Builder().withCustom(CUSTOM_PIPELINE_NUMBER).withTranslucent();
    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }
    return builder.build();
}

Item::Bound ParticleEffectEntityRenderer::getBound(RenderArgs* args) {
    return _bound;
}

// FIXME: these methods assume uniform emitDimensions, need to importance sample based on dimensions
float importanceSample2DDimension(float startDim) {
    float dimension = 1.0f;
    if (startDim < 1.0f) {
        float innerDimensionSquared = startDim * startDim;
        float outerDimensionSquared = 1.0f;  // pow(particle::MAXIMUM_EMIT_RADIUS_START, 2);
        float randDimensionSquared = randFloatInRange(innerDimensionSquared, outerDimensionSquared);
        dimension = std::sqrt(randDimensionSquared);
    }
    return dimension;
}

float importanceSample3DDimension(float startDim) {
    float dimension = 1.0f;
    if (startDim < 1.0f) {
        float innerDimensionCubed = startDim * startDim * startDim;
        float outerDimensionCubed = 1.0f;  // pow(particle::MAXIMUM_EMIT_RADIUS_START, 3);
        float randDimensionCubed = randFloatInRange(innerDimensionCubed, outerDimensionCubed);
        dimension = std::cbrt(randDimensionCubed);
    }
    return dimension;
}

ParticleEffectEntityRenderer::CpuParticle ParticleEffectEntityRenderer::createParticle(const Transform& baseTransform, const particle::Properties& particleProperties,
                                                                                       const ShapeType& shapeType, const GeometryResource::Pointer& geometryResource,
                                                                                       const TriangleInfo& triangleInfo) {
    CpuParticle particle;

    const auto& accelerationSpread = particleProperties.emission.acceleration.spread;
    const auto& azimuthStart = particleProperties.azimuth.start;
    const auto& azimuthFinish = particleProperties.azimuth.finish;
    const auto& emitDimensions = particleProperties.emission.dimensions;
    const auto& emitAcceleration = particleProperties.emission.acceleration.target;
    auto emitOrientation = baseTransform.getRotation() * particleProperties.emission.orientation;
    const auto& emitRadiusStart = glm::max(particleProperties.radiusStart, EPSILON); // Avoid math complications at center
    const auto& emitSpeed = particleProperties.emission.speed.target;
    const auto& speedSpread = particleProperties.emission.speed.spread;
    const auto& polarStart = particleProperties.polar.start;
    const auto& polarFinish = particleProperties.polar.finish;

    particle.seed = randFloatInRange(-1.0f, 1.0f);
    particle.expiration = (uint64_t)(particleProperties.lifespan * USECS_PER_SECOND);

    particle.relativePosition = glm::vec3(0.0f);
    particle.basePosition = baseTransform.getTranslation();

    // Position, velocity, and acceleration
    glm::vec3 emitDirection;
    if (polarStart == 0.0f && polarFinish == 0.0f && emitDimensions.z == 0.0f) {
        // Emit along z-axis from position
        emitDirection = Vectors::UNIT_Z;
    } else {
        // Emit around point or from ellipsoid
        // - Distribute directions evenly around point
        // - Distribute points relatively evenly over ellipsoid surface
        // - Distribute points relatively evenly within ellipsoid volume

        float elevationMinZ = sinf(PI_OVER_TWO - polarFinish);
        float elevationMaxZ = sinf(PI_OVER_TWO - polarStart);
        float elevation = asinf(elevationMinZ + (elevationMaxZ - elevationMinZ) * randFloat());

        float azimuth;
        if (azimuthFinish >= azimuthStart) {
            azimuth = azimuthStart + (azimuthFinish - azimuthStart) * randFloat();
        } else {
            azimuth = azimuthStart + (TWO_PI + azimuthFinish - azimuthStart) * randFloat();
        }
        // TODO: azimuth and elevation are only used for ellipsoids/circles, but could be used for other shapes too

        if (emitDimensions == Vectors::ZERO) {
            // Point
            emitDirection = glm::quat(glm::vec3(PI_OVER_TWO - elevation, 0.0f, azimuth)) * Vectors::UNIT_Z;
        } else {
            glm::vec3 emitPosition;
            switch (shapeType) {
                case SHAPE_TYPE_BOX: {
                    glm::vec3 dim = importanceSample3DDimension(emitRadiusStart) * 0.5f * emitDimensions;

                    int side = randIntInRange(0, 5);
                    int axis = side % 3;
                    float direction = side > 2 ? 1.0f : -1.0f;

                    emitDirection[axis] = direction;
                    emitPosition[axis] = direction * dim[axis];
                    axis = (axis + 1) % 3;
                    emitPosition[axis] = dim[axis] * randFloatInRange(-1.0f, 1.0f);
                    axis = (axis + 1) % 3;
                    emitPosition[axis] = dim[axis] * randFloatInRange(-1.0f, 1.0f);
                    break;
                }

                case SHAPE_TYPE_CYLINDER_X:
                case SHAPE_TYPE_CYLINDER_Y:
                case SHAPE_TYPE_CYLINDER_Z: {
                    glm::vec3 radii = importanceSample2DDimension(emitRadiusStart) * 0.5f * emitDimensions;
                    int axis = shapeType - SHAPE_TYPE_CYLINDER_X;

                    emitPosition[axis] = emitDimensions[axis] * randFloatInRange(-0.5f, 0.5f);
                    emitDirection[axis] = 0.0f;
                    axis = (axis + 1) % 3;
                    emitPosition[axis] = radii[axis] * glm::cos(azimuth);
                    emitDirection[axis] = radii[axis] > 0.0f ? emitPosition[axis] / (radii[axis] * radii[axis]) : 0.0f;
                    axis = (axis + 1) % 3;
                    emitPosition[axis] = radii[axis] * glm::sin(azimuth);
                    emitDirection[axis] = radii[axis] > 0.0f ? emitPosition[axis] / (radii[axis] * radii[axis]) : 0.0f;
                    emitDirection = glm::normalize(emitDirection);
                    break;
                }

                case SHAPE_TYPE_CIRCLE: {
                    glm::vec2 radii = importanceSample2DDimension(emitRadiusStart) * 0.5f * glm::vec2(emitDimensions.x, emitDimensions.z);
                    float x = radii.x * glm::cos(azimuth);
                    float z = radii.y * glm::sin(azimuth);
                    emitPosition = glm::vec3(x, 0.0f, z);
                    emitDirection = Vectors::UP;
                    break;
                }
                case SHAPE_TYPE_PLANE: {
                    glm::vec2 dim = importanceSample2DDimension(emitRadiusStart) * 0.5f * glm::vec2(emitDimensions.x, emitDimensions.z);

                    int side = randIntInRange(0, 3);
                    int axis = side % 2;
                    float direction = side > 1 ? 1.0f : -1.0f;

                    glm::vec2 pos;
                    pos[axis] = direction * dim[axis];
                    axis = (axis + 1) % 2;
                    pos[axis] = dim[axis] * randFloatInRange(-1.0f, 1.0f);

                    emitPosition = glm::vec3(pos.x, 0.0f, pos.y);
                    emitDirection = Vectors::UP;
                    break;
                }

                case SHAPE_TYPE_COMPOUND: {
                    // if we get here we know that geometryResource is loaded

                    size_t index = randFloat() * triangleInfo.totalSamples;
                    Triangle triangle;
                    for (size_t i = 0; i < triangleInfo.samplesPerTriangle.size(); i++) {
                        size_t numSamples = triangleInfo.samplesPerTriangle[i];
                        if (index < numSamples) {
                            triangle = triangleInfo.triangles[i];
                            break;
                        }
                        index -= numSamples;
                    }

                    float edgeLength1 = glm::length(triangle.v1 - triangle.v0);
                    float edgeLength2 = glm::length(triangle.v2 - triangle.v1);
                    float edgeLength3 = glm::length(triangle.v0 - triangle.v2);

                    float perimeter = edgeLength1 + edgeLength2 + edgeLength3;
                    float fraction1 = randFloatInRange(0.0f, 1.0f);
                    float fractionEdge1 = glm::min(fraction1 * perimeter / edgeLength1, 1.0f);
                    float fraction2 = fraction1 - edgeLength1 / perimeter;
                    float fractionEdge2 = glm::clamp(fraction2 * perimeter / edgeLength2, 0.0f, 1.0f);
                    float fraction3 = fraction2 - edgeLength2 / perimeter;
                    float fractionEdge3 = glm::clamp(fraction3 * perimeter / edgeLength3, 0.0f, 1.0f);

                    float dim = importanceSample2DDimension(emitRadiusStart);
                    triangle = triangle * (glm::scale(emitDimensions) * triangleInfo.transform);
                    glm::vec3 center = (triangle.v0 + triangle.v1 + triangle.v2) / 3.0f;
                    glm::vec3 v0 = (dim * (triangle.v0 - center)) + center;
                    glm::vec3 v1 = (dim * (triangle.v1 - center)) + center;
                    glm::vec3 v2 = (dim * (triangle.v2 - center)) + center;

                    emitPosition = glm::mix(v0, glm::mix(v1, glm::mix(v2, v0, fractionEdge3), fractionEdge2), fractionEdge1);
                    emitDirection = triangle.getNormal();
                    break;
                }

                case SHAPE_TYPE_SPHERE:
                case SHAPE_TYPE_ELLIPSOID:
                default: {
                    glm::vec3 radii = importanceSample3DDimension(emitRadiusStart) * 0.5f * emitDimensions;
                    float x = radii.x * glm::cos(elevation) * glm::cos(azimuth);
                    float y = radii.y * glm::cos(elevation) * glm::sin(azimuth);
                    float z = radii.z * glm::sin(elevation);
                    emitPosition = glm::vec3(x, y, z);
                    emitDirection = glm::normalize(glm::vec3(radii.x > 0.0f ? x / (radii.x * radii.x) : 0.0f,
                                                             radii.y > 0.0f ? y / (radii.y * radii.y) : 0.0f,
                                                             radii.z > 0.0f ? z / (radii.z * radii.z) : 0.0f));
                    break;
                }
            }

            particle.relativePosition += emitOrientation * emitPosition;
        }
    }
    particle.velocity = (emitSpeed + randFloatInRange(-1.0f, 1.0f) * speedSpread) * (emitOrientation * emitDirection);
    particle.acceleration = emitAcceleration +
        glm::vec3(randFloatInRange(-1.0f, 1.0f), randFloatInRange(-1.0f, 1.0f), randFloatInRange(-1.0f, 1.0f)) * accelerationSpread;

    return particle;
}

void ParticleEffectEntityRenderer::stepSimulation() {
    if (_lastSimulated == 0) {
        _lastSimulated = usecTimestampNow();
        return;
    }

    const auto now = usecTimestampNow();
    const auto interval = std::min<uint64_t>(USECS_PER_SECOND / 60, now - _lastSimulated);
    _lastSimulated = now;

    const auto& modelTransform = getModelTransform();
    if (_emitting && _particleProperties.emitting() &&
        (_shapeType != SHAPE_TYPE_COMPOUND || (_geometryResource && _geometryResource->isLoaded()))) {
        uint64_t emitInterval = _particleProperties.emitIntervalUsecs();
        if (emitInterval > 0 && interval >= _timeUntilNextEmit) {
            auto timeRemaining = interval;
            while (timeRemaining > _timeUntilNextEmit) {
                if (_shapeType == SHAPE_TYPE_COMPOUND && !_hasComputedTriangles) {
                    computeTriangles(_geometryResource->getHFMModel());
                }
                // emit particle
                _cpuParticles.push_back(createParticle(modelTransform, _particleProperties, _shapeType, _geometryResource, _triangleInfo));
                _timeUntilNextEmit = emitInterval;
                if (emitInterval < timeRemaining) {
                    timeRemaining -= emitInterval;
                }
            }
        } else {
            _timeUntilNextEmit -= interval;
        }
    }

    // Kill any particles that have expired or are over the max size
    while (_cpuParticles.size() > _particleProperties.maxParticles || (!_cpuParticles.empty() && _cpuParticles.front().expiration == 0)) {
        _cpuParticles.pop_front();
    }

    const float deltaTime = (float)interval / (float)USECS_PER_SECOND;
    // update the particles
    for (auto& particle : _cpuParticles) {
        if (_prevEmitterShouldTrail != _particleProperties.emission.shouldTrail) {
            if (_prevEmitterShouldTrail) {
                particle.relativePosition = particle.relativePosition + particle.basePosition - modelTransform.getTranslation();
            }
            particle.basePosition = modelTransform.getTranslation();
        }
        particle.expiration = particle.expiration >= interval ? particle.expiration - interval : 0;
        particle.integrate(deltaTime);
    }
    _prevEmitterShouldTrail = _particleProperties.emission.shouldTrail;

    // Build particle primitives
    static GpuParticles gpuParticles;
    gpuParticles.clear();
    gpuParticles.reserve(_cpuParticles.size()); // Reserve space
    std::transform(_cpuParticles.begin(), _cpuParticles.end(), std::back_inserter(gpuParticles), [this, &modelTransform] (const CpuParticle& particle) {
        glm::vec3 position = particle.relativePosition + (_particleProperties.emission.shouldTrail ? particle.basePosition : modelTransform.getTranslation());
        return GpuParticle(position, glm::vec2(particle.lifetime, particle.seed));
    });

    // Update particle buffer
    auto& particleBuffer = _particleBuffer;
    size_t numBytes = sizeof(GpuParticle) * gpuParticles.size();
    particleBuffer->resize(numBytes);
    if (numBytes != 0) {
        particleBuffer->setData(numBytes, (const gpu::Byte*)gpuParticles.data());
    }
}

void ParticleEffectEntityRenderer::doRender(RenderArgs* args) {
    if (!_visible || !(_networkTexture && _networkTexture->isLoaded())) {
        return;
    }

    // FIXME migrate simulation to a compute stage
    stepSimulation();

    gpu::Batch& batch = *args->_batch;
    batch.setResourceTexture(0, _networkTexture->getGPUTexture());

    Transform transform;
    // The particles are in world space, so the transform is unused, except for the rotation, which we use
    // if the particles are marked rotateWithEntity
    withReadLock([&] {
        transform.setRotation(_renderTransform.getRotation());
    });

    auto& color = _uniformBuffer.edit<ParticleUniforms>().color;
    color.start = EntityRenderer::calculatePulseColor(_particleProperties.getColorStart(), _pulseProperties, _created);
    color.middle = EntityRenderer::calculatePulseColor(_particleProperties.getColorMiddle(), _pulseProperties, _created);
    color.finish = EntityRenderer::calculatePulseColor(_particleProperties.getColorFinish(), _pulseProperties, _created);
    color.spread = EntityRenderer::calculatePulseColor(_particleProperties.getColorSpread(), _pulseProperties, _created);

    batch.setModelTransform(transform); // particles are currently always transparent so we don't worry about TAA right now

    batch.setUniformBuffer(0, _uniformBuffer);
    batch.setInputFormat(_vertexFormat);
    batch.setInputBuffer(0, _particleBuffer, 0, sizeof(GpuParticle));

    auto numParticles = _particleBuffer->getSize() / sizeof(GpuParticle);
    static const size_t VERTEX_PER_PARTICLE = 4;
    batch.drawInstanced((gpu::uint32)numParticles, gpu::TRIANGLE_STRIP, (gpu::uint32)VERTEX_PER_PARTICLE);
}

void ParticleEffectEntityRenderer::fetchGeometryResource() {
    QUrl hullURL(_compoundShapeURL);
    if (hullURL.isEmpty()) {
        _geometryResource.reset();
    } else {
        _geometryResource = DependencyManager::get<ModelCache>()->getCollisionGeometryResource(hullURL);
    }
}

// FIXME: this is very similar to Model::calculateTriangleSets
void ParticleEffectEntityRenderer::computeTriangles(const hfm::Model& hfmModel) {
    PROFILE_RANGE(render, __FUNCTION__);

    int numberOfMeshes = hfmModel.meshes.size();

    _hasComputedTriangles = true;
    _triangleInfo.triangles.clear();
    _triangleInfo.samplesPerTriangle.clear();

    std::vector<float> areas;
    float minArea = FLT_MAX;
    AABox bounds;

    for (int i = 0; i < numberOfMeshes; i++) {
        const HFMMesh& mesh = hfmModel.meshes.at(i);

        const int numberOfParts = mesh.parts.size();
        for (int j = 0; j < numberOfParts; j++) {
            const HFMMeshPart& part = mesh.parts.at(j);

            const int INDICES_PER_TRIANGLE = 3;
            const int INDICES_PER_QUAD = 4;
            const int TRIANGLES_PER_QUAD = 2;

            // tell our triangleSet how many triangles to expect.
            int numberOfQuads = part.quadIndices.size() / INDICES_PER_QUAD;
            int numberOfTris = part.triangleIndices.size() / INDICES_PER_TRIANGLE;
            int totalTriangles = (numberOfQuads * TRIANGLES_PER_QUAD) + numberOfTris;
            _triangleInfo.triangles.reserve(_triangleInfo.triangles.size() + totalTriangles);
            areas.reserve(areas.size() + totalTriangles);

            auto meshTransform = hfmModel.offset * mesh.modelTransform;

            if (part.quadIndices.size() > 0) {
                int vIndex = 0;
                for (int q = 0; q < numberOfQuads; q++) {
                    int i0 = part.quadIndices[vIndex++];
                    int i1 = part.quadIndices[vIndex++];
                    int i2 = part.quadIndices[vIndex++];
                    int i3 = part.quadIndices[vIndex++];

                    // track the model space version... these points will be transformed by the FST's offset, 
                    // which includes the scaling, rotation, and translation specified by the FST/FBX, 
                    // this can't change at runtime, so we can safely store these in our TriangleSet
                    glm::vec3 v0 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i0], 1.0f));
                    glm::vec3 v1 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i1], 1.0f));
                    glm::vec3 v2 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i2], 1.0f));
                    glm::vec3 v3 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i3], 1.0f));

                    Triangle tri1 = { v0, v1, v3 };
                    Triangle tri2 = { v1, v2, v3 };
                    _triangleInfo.triangles.push_back(tri1);
                    _triangleInfo.triangles.push_back(tri2);

                    float area1 = tri1.getArea();
                    areas.push_back(area1);
                    if (area1 > EPSILON) {
                        minArea = std::min(minArea, area1);
                    }

                    float area2 = tri2.getArea();
                    areas.push_back(area2);
                    if (area2 > EPSILON) {
                        minArea = std::min(minArea, area2);
                    }

                    bounds += v0;
                    bounds += v1;
                    bounds += v2;
                    bounds += v3;
                }
            }

            if (part.triangleIndices.size() > 0) {
                int vIndex = 0;
                for (int t = 0; t < numberOfTris; t++) {
                    int i0 = part.triangleIndices[vIndex++];
                    int i1 = part.triangleIndices[vIndex++];
                    int i2 = part.triangleIndices[vIndex++];

                    // track the model space version... these points will be transformed by the FST's offset, 
                    // which includes the scaling, rotation, and translation specified by the FST/FBX, 
                    // this can't change at runtime, so we can safely store these in our TriangleSet
                    glm::vec3 v0 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i0], 1.0f));
                    glm::vec3 v1 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i1], 1.0f));
                    glm::vec3 v2 = glm::vec3(meshTransform * glm::vec4(mesh.vertices[i2], 1.0f));

                    Triangle tri = { v0, v1, v2 };
                    _triangleInfo.triangles.push_back(tri);

                    float area = tri.getArea();
                    areas.push_back(area);
                    if (area > EPSILON) {
                        minArea = std::min(minArea, area);
                    }

                    bounds += v0;
                    bounds += v1;
                    bounds += v2;
                }
            }
        }
    }

    _triangleInfo.totalSamples = 0;
    for (auto& area : areas) {
        size_t numSamples = area / minArea;
        _triangleInfo.samplesPerTriangle.push_back(numSamples);
        _triangleInfo.totalSamples += numSamples;
    }

    glm::vec3 scale = bounds.getScale();
    _triangleInfo.transform = glm::scale(1.0f / scale) * glm::translate(-bounds.calcCenter());
}