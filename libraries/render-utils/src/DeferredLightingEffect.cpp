//
//  DeferredLightingEffect.cpp
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 9/11/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DeferredLightingEffect.h"
#include <QtCore/QFileSelector>

#include <GLMHelpers.h>
#include <PathUtils.h>
#include <ViewFrustum.h>
#include <shared/FileUtils.h>
#include <gpu/Batch.h>
#include <gpu/Context.h>
#include <shaders/Shaders.h>

#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"
#include "AbstractViewStateInterface.h"
#include "GeometryCache.h"
#include "TextureCache.h"
#include "FramebufferCache.h"

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

using namespace render;

static void loadLightProgram(int programId, bool lightVolume, gpu::PipelinePointer& program);

void DeferredLightingEffect::init() {
    loadLightProgram(shader::render_utils::program::directional_skybox_light_ambient, false, _directionalAmbientSphereLight);
    loadLightProgram(shader::render_utils::program::directional_skybox_light, false, _directionalSkyboxLight);

    loadLightProgram(shader::render_utils::program::directional_skybox_light_ambient_shadow, false, _directionalAmbientSphereLightShadow);
    loadLightProgram(shader::render_utils::program::directional_skybox_light_shadow, false, _directionalSkyboxLightShadow);

    loadLightProgram(shader::render_utils::program::local_lights_shading, true, _localLight);
    loadLightProgram(shader::render_utils::program::local_lights_drawOutline, true, _localLightOutline);
}

// FIXME: figure out how to move lightFrame into a varying in GeometryCache and RenderPipelines
void DeferredLightingEffect::setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch) {
    setupKeyLightBatch(args, batch, args->_scene->getStage<LightStage>()->_currentFrame);
}

void DeferredLightingEffect::setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch, const LightStage::Frame& lightFrame) {
    PerformanceTimer perfTimer("DLE->setupBatch()");
    graphics::LightPointer keySunLight;
    auto lightStage = args->_scene->getStage<LightStage>();
    if (lightStage) {
        keySunLight = lightStage->getCurrentKeyLight(lightFrame);
    }

    graphics::LightPointer keyAmbiLight;
    if (lightStage) {
        keyAmbiLight = lightStage->getCurrentAmbientLight(lightFrame);
    }

    if (keySunLight) {
        batch.setUniformBuffer(gr::Buffer::KeyLight, keySunLight->getLightSchemaBuffer());
    }

    if (keyAmbiLight) {
        batch.setUniformBuffer(gr::Buffer::AmbientLight, keyAmbiLight->getAmbientSchemaBuffer());

        if (keyAmbiLight->getAmbientMap() ) {
            batch.setResourceTexture(ru::Texture::Skybox, keyAmbiLight->getAmbientMap());
        }
    }
}

void DeferredLightingEffect::unsetKeyLightBatch(gpu::Batch& batch) {
    batch.setUniformBuffer(gr::Buffer::KeyLight, nullptr);
    batch.setUniformBuffer(gr::Buffer::AmbientLight, nullptr);
    batch.setResourceTexture(ru::Texture::Skybox, nullptr);
}

void DeferredLightingEffect::setupLocalLightsBatch(gpu::Batch& batch, const LightClustersPointer& lightClusters) {
    // Bind the global list of lights and the visible lights this frame
    batch.setUniformBuffer(gr::Buffer::Light, lightClusters->_lightStage->getLightArrayBuffer());

    batch.setUniformBuffer(ru::Buffer::LightClusterFrustumGrid, lightClusters->_frustumGridBuffer);
    batch.setUniformBuffer(ru::Buffer::LightClusterGrid, lightClusters->_clusterGridBuffer);
    batch.setUniformBuffer(ru::Buffer::LightClusterContent, lightClusters->_clusterContentBuffer);
}

void DeferredLightingEffect::unsetLocalLightsBatch(gpu::Batch& batch) {
    batch.setUniformBuffer(gr::Buffer::Light, nullptr);
    batch.setUniformBuffer(ru::Buffer::LightClusterGrid, nullptr);
    batch.setUniformBuffer(ru::Buffer::LightClusterContent, nullptr);
    batch.setUniformBuffer(ru::Buffer::LightClusterFrustumGrid, nullptr);
}

static void loadLightProgram(int programId, bool lightVolume, gpu::PipelinePointer& pipeline) {

    gpu::ShaderPointer program = gpu::Shader::createProgram(programId);

    auto state = std::make_shared<gpu::State>();
    state->setColorWriteMask(true, true, true, false);

    if (lightVolume) {
        PrepareStencil::testShape(*state);
       
        state->setCullMode(gpu::State::CULL_BACK);
        //state->setCullMode(gpu::State::CULL_FRONT);
        //state->setDepthTest(true, false, gpu::GREATER_EQUAL);
        //state->setDepthClampEnable(true);
        // TODO: We should use DepthClamp and avoid changing geometry for inside /outside cases
        // additive blending
        state->setBlendFunction(true, gpu::State::ONE, gpu::State::BLEND_OP_ADD, gpu::State::ONE);

    } else {
        // Stencil test all the light passes for objects pixels only, not the background
        PrepareStencil::testShape(*state);

        state->setCullMode(gpu::State::CULL_BACK);
        // additive blending
        state->setBlendFunction(true, gpu::State::ONE, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
    }
    pipeline = gpu::Pipeline::create(program, state);

}

#include <shared/Shapes.h>

graphics::MeshPointer DeferredLightingEffect::getPointLightMesh() {
    if (!_pointLightMesh) {
        _pointLightMesh = std::make_shared<graphics::Mesh>();

        // let's use a icosahedron
        auto solid = geometry::icosahedron();
        solid.fitDimension(1.05f); // scaled to 1.05 meters, it will be scaled by the shader accordingly to the light size

        int verticesSize = (int) (solid.vertices.size() * 3 * sizeof(float));
        float* vertexData = (float*) solid.vertices.data();

        _pointLightMesh->setVertexBuffer(gpu::BufferView(new gpu::Buffer(verticesSize, (gpu::Byte*) vertexData), gpu::Element::VEC3F_XYZ));

        int nbIndices = (int) solid.faces.size() * 3;

        gpu::uint16* indexData = new gpu::uint16[nbIndices];
        gpu::uint16* index = indexData;
        for (auto face : solid.faces) {
            *(index++) = face[0];
            *(index++) = face[1];
            *(index++) = face[2];
        }

        _pointLightMesh->setIndexBuffer(gpu::BufferView(new gpu::Buffer(sizeof(unsigned short) * nbIndices, (gpu::Byte*) indexData), gpu::Element::INDEX_UINT16));
        delete[] indexData;


        std::vector<graphics::Mesh::Part> parts;
        parts.push_back(graphics::Mesh::Part(0, nbIndices, 0, graphics::Mesh::TRIANGLES));
        parts.push_back(graphics::Mesh::Part(0, nbIndices, 0, graphics::Mesh::LINE_STRIP)); // outline version


        _pointLightMesh->setPartBuffer(gpu::BufferView(new gpu::Buffer(parts.size() * sizeof(graphics::Mesh::Part), (gpu::Byte*) parts.data()), gpu::Element::PART_DRAWCALL));
    }
    return _pointLightMesh;
}

graphics::MeshPointer DeferredLightingEffect::getSpotLightMesh() {
    if (!_spotLightMesh) {
        _spotLightMesh = std::make_shared<graphics::Mesh>();

        int slices = 16;
        int rings = 3;
        int vertices = 2 + rings * slices;
        int originVertex = vertices - 2;
        int capVertex = vertices - 1;
        int verticesSize = vertices * 3 * sizeof(float);
        int indices = 3 * slices * (1 + 1 + 2 * (rings -1));
        int ringFloatOffset = slices * 3;


        float* vertexData = new float[verticesSize];
        float* vertexRing0 = vertexData;
        float* vertexRing1 = vertexRing0 + ringFloatOffset;
        float* vertexRing2 = vertexRing1 + ringFloatOffset;
        
        for (int i = 0; i < slices; i++) {
            float theta = TWO_PI * i / slices;
            auto cosin = glm::vec2(cosf(theta), sinf(theta));

            *(vertexRing0++) = cosin.x;
            *(vertexRing0++) = cosin.y;
            *(vertexRing0++) = 0.0f;

            *(vertexRing1++) = cosin.x;
            *(vertexRing1++) = cosin.y;
            *(vertexRing1++) = 0.33f;

            *(vertexRing2++) = cosin.x;
            *(vertexRing2++) = cosin.y;
            *(vertexRing2++) = 0.66f;
        }
        
        *(vertexRing2++) = 0.0f;
        *(vertexRing2++) = 0.0f;
        *(vertexRing2++) = -1.0f;
        
        *(vertexRing2++) = 0.0f;
        *(vertexRing2++) = 0.0f;
        *(vertexRing2++) = 1.0f;
        
        _spotLightMesh->setVertexBuffer(gpu::BufferView(new gpu::Buffer(verticesSize, (gpu::Byte*) vertexData), gpu::Element::VEC3F_XYZ));
        delete[] vertexData;

        gpu::uint16* indexData = new gpu::uint16[indices];
        gpu::uint16* index = indexData;
        for (int i = 0; i < slices; i++) {
            *(index++) = originVertex;
            
            int s0 = i;
            int s1 = ((i + 1) % slices);
            *(index++) = s0;
            *(index++) = s1;

            int s2 = s0 + slices;
            int s3 = s1 + slices;
            *(index++) = s1;
            *(index++) = s0;
            *(index++) = s2;

            *(index++) = s1;
            *(index++) = s2;
            *(index++) = s3;

            int s4 = s2 + slices;
            int s5 = s3 + slices;
            *(index++) = s3;
            *(index++) = s2;
            *(index++) = s4;

            *(index++) = s3;
            *(index++) = s4;
            *(index++) = s5;


            *(index++) = s5;
            *(index++) = s4;
            *(index++) = capVertex;
        }

        _spotLightMesh->setIndexBuffer(gpu::BufferView(new gpu::Buffer(sizeof(unsigned short) * indices, (gpu::Byte*) indexData), gpu::Element::INDEX_UINT16));
        delete[] indexData;

        
        std::vector<graphics::Mesh::Part> parts;
        parts.push_back(graphics::Mesh::Part(0, indices, 0, graphics::Mesh::TRIANGLES));
        parts.push_back(graphics::Mesh::Part(0, indices, 0, graphics::Mesh::LINE_STRIP)); // outline version

        
        _spotLightMesh->setPartBuffer(gpu::BufferView(new gpu::Buffer(parts.size() * sizeof(graphics::Mesh::Part), (gpu::Byte*) parts.data()), gpu::Element::PART_DRAWCALL));
    }
    return _spotLightMesh;
}

void PrepareDeferred::run(const RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    auto args = renderContext->args;

    auto primaryFramebuffer = inputs.get0();
    auto lightingModel = inputs.get1();

    if (!_deferredFramebuffer) {
        _deferredFramebuffer = std::make_shared<DeferredFramebuffer>();
    }
    _deferredFramebuffer->updatePrimaryDepth(primaryFramebuffer->getDepthStencilBuffer());

    outputs.edit0() = _deferredFramebuffer;
    outputs.edit1() = _deferredFramebuffer->getLightingFramebuffer();
    outputs.edit2() = _deferredFramebuffer->getLightingWithVelocityFramebuffer();

    gpu::doInBatch("PrepareDeferred::run", args->_context, [&](gpu::Batch& batch) {
        batch.enableStereo(false);
        batch.setViewportTransform(args->_viewport);
        batch.setStateScissorRect(args->_viewport);

        // Clear deferred
        auto deferredFbo = _deferredFramebuffer->getDeferredFramebuffer();
        batch.setFramebuffer(deferredFbo);

        // Clear Color, Depth and Stencil for deferred buffer
        batch.clearFramebuffer(
            gpu::Framebuffer::BUFFER_COLOR0 | gpu::Framebuffer::BUFFER_COLOR1 |
            gpu::Framebuffer::BUFFER_COLOR2 | gpu::Framebuffer::BUFFER_COLOR3 |
            gpu::Framebuffer::BUFFER_COLOR4 | gpu::Framebuffer::BUFFER_DEPTH |
            gpu::Framebuffer::BUFFER_STENCIL,
            vec4(vec3(0), 0), 1.0, 0, true);

        // For the rest of the rendering, bind the lighting model
        batch.setUniformBuffer(ru::Buffer::LightModel, lightingModel->getParametersBuffer());
        batch.setResourceTexture(ru::Texture::AmbientFresnel, lightingModel->getAmbientFresnelLUT());
    });
}


void RenderDeferredSetup::run(const render::RenderContextPointer& renderContext,
    const DeferredFrameTransformPointer& frameTransform,
    const DeferredFramebufferPointer& deferredFramebuffer,
    const LightingModelPointer& lightingModel,
    const LightStage::FramePointer& lightFrame,
    const LightStage::ShadowFramePointer& shadowFrame,
    const HazeStage::FramePointer& hazeFrame,
    const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer,
    const AmbientOcclusionFramebufferPointer& ambientOcclusionFramebuffer,
    const SubsurfaceScatteringResourcePointer& subsurfaceScatteringResource) {

    auto args = renderContext->args;
    auto& batch = (*args->_batch);
    {
        // Framebuffer copy operations cannot function as multipass stereo operations.
        batch.enableStereo(false);

        auto textureCache = DependencyManager::get<TextureCache>();
        auto deferredLightingEffect = DependencyManager::get<DeferredLightingEffect>();

        // binding the first framebuffer
        auto lightingFBO = deferredFramebuffer->getLightingFramebuffer();
        batch.setFramebuffer(lightingFBO);
        
        batch.setViewportTransform(args->_viewport);
        batch.setStateScissorRect(args->_viewport);
        
        
        // Bind the G-Buffer surfaces
        batch.setResourceTexture(ru::Texture::DeferredColor, deferredFramebuffer->getDeferredColorTexture());
        batch.setResourceTexture(ru::Texture::DeferredNormal, deferredFramebuffer->getDeferredNormalTexture());
        batch.setResourceTexture(ru::Texture::DeferredSpecular, deferredFramebuffer->getDeferredSpecularTexture());
        batch.setResourceTexture(ru::Texture::DeferredDepth, deferredFramebuffer->getPrimaryDepthTexture());
        
        // FIXME: Different render modes should have different tasks
        if (lightingModel->isAmbientOcclusionEnabled() && ambientOcclusionFramebuffer) {
            batch.setResourceTexture(ru::Texture::DeferredObscurance, ambientOcclusionFramebuffer->getOcclusionTexture());
        } else {
            // need to assign the white texture if ao is off
            batch.setResourceTexture(ru::Texture::DeferredObscurance, textureCache->getWhiteTexture());
        }

        // The Deferred Frame Transform buffer
        batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, frameTransform->getFrameTransformBuffer());

        // THe lighting model
        batch.setUniformBuffer(ru::Buffer::LightModel, lightingModel->getParametersBuffer());
        batch.setResourceTexture(ru::Texture::AmbientFresnel, lightingModel->getAmbientFresnelLUT());

        // Subsurface scattering specific
        if (surfaceGeometryFramebuffer) {
            batch.setResourceTexture(ru::Texture::DeferredLinearZEye, surfaceGeometryFramebuffer->getLinearDepthTexture());
            batch.setResourceTexture(ru::Texture::DeferredCurvature, surfaceGeometryFramebuffer->getCurvatureTexture());
            batch.setResourceTexture(ru::Texture::DeferredDiffusedCurvature, surfaceGeometryFramebuffer->getLowCurvatureTexture());
        }
        if (subsurfaceScatteringResource) {
            batch.setUniformBuffer(ru::Buffer::SsscParams, subsurfaceScatteringResource->getParametersBuffer());
            batch.setResourceTexture(ru::Texture::SsscLut, subsurfaceScatteringResource->getScatteringTable());
            batch.setResourceTexture(ru::Texture::SsscSpecularBeckmann, subsurfaceScatteringResource->getScatteringSpecular());
        }

        // Global directional light, maybe shadow and ambient pass
        auto lightStage = renderContext->_scene->getStage<LightStage>();
        assert(lightStage);
        assert(lightStage->getNumLights() > 0);
        auto keyLight = lightStage->getCurrentKeyLight(*lightFrame);

        // Check if keylight casts shadows
        bool keyLightCastShadows{ false };
        LightStage::ShadowPointer globalShadow;
        if (lightingModel->isShadowEnabled() && shadowFrame && !shadowFrame->_objects.empty()) {
            globalShadow = shadowFrame->_objects.front();
            if (globalShadow) {
                keyLightCastShadows = true;
            }
        }

        // Global Ambient light
        graphics::LightPointer ambientLight;
        if (lightStage && lightFrame->_ambientLights.size()) {
            ambientLight = lightStage->getLight(lightFrame->_ambientLights.front());
        }
        bool hasAmbientMap = (ambientLight != nullptr);

        // Setup the global directional pass pipeline
        auto program = deferredLightingEffect->_directionalSkyboxLight;
        {
            if (keyLightCastShadows) {

                // If the keylight has an ambient Map then use the Skybox version of the pass
                // otherwise use the ambient sphere version
                if (hasAmbientMap) {
                    program = deferredLightingEffect->_directionalSkyboxLightShadow;
                } else {
                    program = deferredLightingEffect->_directionalAmbientSphereLightShadow;
                }
            } else {
                // If the keylight has an ambient Map then use the Skybox version of the pass
                // otherwise use the ambient sphere version
                if (hasAmbientMap) {
                    program = deferredLightingEffect->_directionalSkyboxLight;
                } else {
                    program = deferredLightingEffect->_directionalAmbientSphereLight;
                }
            }

            if (keyLightCastShadows && globalShadow) {
                batch.setResourceTexture(ru::Texture::Shadow, globalShadow->map);
                batch.setUniformBuffer(ru::Buffer::ShadowParams, globalShadow->getBuffer());
            }

            batch.setPipeline(program);
        }

        // Setup the global lighting
        deferredLightingEffect->setupKeyLightBatch(args, batch, *lightFrame);

        // Haze
        const auto& hazeStage = args->_scene->getStage<HazeStage>();
        if (hazeStage && hazeFrame->_hazes.size() > 0) {
            const auto& hazePointer = hazeStage->getHaze(hazeFrame->_hazes.front());
            if (hazePointer) {
                batch.setUniformBuffer(graphics::slot::buffer::Buffer::HazeParams, hazePointer->getHazeParametersBuffer());
            }
        }

        batch.draw(gpu::TRIANGLE_STRIP, 4);

        deferredLightingEffect->unsetKeyLightBatch(batch);
        batch.setResourceTexture(ru::Texture::Shadow, nullptr);
    }
}

RenderDeferredLocals::RenderDeferredLocals() :
    _localLightsBuffer(std::make_shared<gpu::Buffer>()) {

}

void RenderDeferredLocals::run(const render::RenderContextPointer& renderContext,
    const DeferredFrameTransformPointer& frameTransform,
    const DeferredFramebufferPointer& deferredFramebuffer,
    const LightingModelPointer& lightingModel,
    const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer, 
    const LightClustersPointer& lightClusters) {

    bool points = lightingModel->isPointLightEnabled();
    bool spots = lightingModel->isSpotLightEnabled();

    if (!points && !spots) {
        return;
    }
    auto args = renderContext->args;
    auto& batch = (*args->_batch);
    {
        // THe main viewport is assumed to be the mono viewport (or the 2 stereo faces side by side within that viewport)
        auto viewport = args->_viewport;

        // The view frustum is the mono frustum base
        auto viewFrustum = args->getViewFrustum();

        // Eval the mono projection
        mat4 projMat;
        viewFrustum.evalProjectionMatrix(projMat);

        // The view transform
        Transform viewTransform;
        viewFrustum.evalViewTransform(viewTransform);


        auto deferredLightingEffect = DependencyManager::get<DeferredLightingEffect>();

        // Render in this viewport
        batch.setViewportTransform(viewport);
        batch.setStateScissorRect(viewport);

        auto& lightIndices = lightClusters->_visibleLightIndices;
        if (!lightIndices.empty() && lightIndices[0] > 0) {
            deferredLightingEffect->setupLocalLightsBatch(batch, lightClusters);

            // Local light pipeline
            batch.setPipeline(deferredLightingEffect->_localLight);

            batch.draw(gpu::TRIANGLE_STRIP, 4);

             // Draw outline as well ?
            if (lightingModel->isShowLightContourEnabled()) {
                batch.setPipeline(deferredLightingEffect->_localLightOutline);

                batch.draw(gpu::TRIANGLE_STRIP, 4);
            }
        }
    }
}

void RenderDeferredCleanup::run(const render::RenderContextPointer& renderContext) {
    auto args = renderContext->args;
    auto& batch = (*args->_batch);
    {
        // Probably not necessary in the long run because the gpu layer would unbound this texture if used as render target
        batch.setResourceTexture(ru::Texture::DeferredColor, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredNormal, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredSpecular, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredDepth, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredObscurance, nullptr);

        batch.setResourceTexture(ru::Texture::DeferredLinearZEye, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredCurvature, nullptr);
        batch.setResourceTexture(ru::Texture::DeferredDiffusedCurvature, nullptr);
        batch.setResourceTexture(ru::Texture::SsscLut, nullptr);
        batch.setResourceTexture(ru::Texture::SsscSpecularBeckmann, nullptr);

        batch.setUniformBuffer(ru::Buffer::SsscParams, nullptr);
        //     batch.setUniformBuffer(LIGHTING_MODEL_BUFFER_SLOT, nullptr);
        batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, nullptr);

        batch.setUniformBuffer(ru::Buffer::LightClusterFrustumGrid, nullptr);
        batch.setUniformBuffer(ru::Buffer::LightClusterGrid, nullptr);
        batch.setUniformBuffer(ru::Buffer::LightClusterContent, nullptr);

    }
}

RenderDeferred::RenderDeferred() {
    DependencyManager::get<DeferredLightingEffect>()->init();
}

void RenderDeferred::configure(const Config& config) {
}

void RenderDeferred::run(const RenderContextPointer& renderContext, const Inputs& inputs) {
    auto args = renderContext->args;

    auto deferredTransform = inputs.get0();
    auto deferredFramebuffer = inputs.get1();
    auto extraRenderBuffers = inputs.get2();
        auto surfaceGeometryFramebuffer = extraRenderBuffers.get0();
        auto ssaoFramebuffer = extraRenderBuffers.get1();
        auto subsurfaceScatteringResource = extraRenderBuffers.get2();

    auto lightingModel = inputs.get3();
    auto lightClusters = inputs.get4();
    
    const auto& lightFrame = inputs.get5();
    const auto& shadowFrame = inputs.get6();
    const auto& hazeFrame = inputs.get7();

    if (!_gpuTimer) {
        _gpuTimer = std::make_shared < gpu::RangeTimer>(__FUNCTION__);
    }

    auto previousBatch = args->_batch;
    gpu::doInBatch(nullptr, args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;
        _gpuTimer->begin(batch);

        setupJob.run(renderContext, deferredTransform, deferredFramebuffer, lightingModel, lightFrame, shadowFrame, hazeFrame, surfaceGeometryFramebuffer, ssaoFramebuffer, subsurfaceScatteringResource);

        lightsJob.run(renderContext, deferredTransform, deferredFramebuffer, lightingModel, surfaceGeometryFramebuffer, lightClusters);

        cleanupJob.run(renderContext);

        _gpuTimer->end(batch);
    });
     args->_batch = previousBatch;

    auto config = std::static_pointer_cast<Config>(renderContext->jobConfig);
    config->setGPUBatchRunTime(_gpuTimer->getGPUAverage(), _gpuTimer->getBatchAverage());
}

void DefaultLightingSetup::run(const RenderContextPointer& renderContext) {

    if (!_defaultLight || !_defaultBackground) {
        auto defaultSkyboxURL = PathUtils::resourcesUrl() + "images/Default-Sky-9-cubemap/Default-Sky-9-cubemap.texmeta.json";
        auto defaultAmbientURL = PathUtils::resourcesUrl() + "images/Default-Sky-9-cubemap/Default-Sky-9-cubemap-ambient.texmeta.json";

        if (!_defaultSkyboxNetworkTexture) {
            PROFILE_RANGE(render, "Process Default Skybox");
            _defaultSkyboxNetworkTexture = DependencyManager::get<TextureCache>()->getTexture(
                defaultSkyboxURL, image::TextureUsage::SKY_TEXTURE);
        }

        if (!_defaultAmbientNetworkTexture) {
            PROFILE_RANGE(render, "Process Default Ambient map");
            _defaultAmbientNetworkTexture = DependencyManager::get<TextureCache>()->getTexture(
                defaultAmbientURL, image::TextureUsage::AMBIENT_TEXTURE);
        }

        if (_defaultSkyboxNetworkTexture && _defaultSkyboxNetworkTexture->isLoaded() && _defaultSkyboxNetworkTexture->getGPUTexture()) {
            _defaultSkybox->setCubemap(_defaultSkyboxNetworkTexture->getGPUTexture());
        } else {
            // Don't do anything until the skybox has loaded
            return;
        }

        if (_defaultAmbientNetworkTexture && _defaultAmbientNetworkTexture->isLoaded() && _defaultAmbientNetworkTexture->getGPUTexture()) {
            _defaultAmbientTexture = _defaultAmbientNetworkTexture->getGPUTexture();
        } else {
            // Don't do anything until the ambient box has been loaded
            return;
        }

        auto lightStage = renderContext->_scene->getStage<LightStage>();
        if (lightStage) { 

            // Allocate a default global light directional and ambient
            auto lp = std::make_shared<graphics::Light>();
            lp->setType(graphics::Light::SUN);
            lp->setDirection(glm::vec3(-1.0f));
            lp->setColor(glm::vec3(1.0f));
            lp->setIntensity(1.0f);
            lp->setType(graphics::Light::SUN);
            lp->setAmbientSpherePreset(gpu::SphericalHarmonics::Preset::OLD_TOWN_SQUARE);

            lp->setAmbientIntensity(0.5f);
            lp->setAmbientMap(_defaultAmbientTexture);
            auto irradianceSH = _defaultAmbientTexture->getIrradiance();
            if (irradianceSH) {
                lp->setAmbientSphere((*irradianceSH));
            }

            // capture default light
            _defaultLight = lp;

            // Add the global light to the light stage (for later shadow rendering)
            // Set this light to be the default
            _defaultLightID = lightStage->addLight(lp, true);
        }

        auto backgroundStage = renderContext->_scene->getStage<BackgroundStage>();
        if (backgroundStage) {

            auto background = std::make_shared<graphics::SunSkyStage>();
            background->setSkybox(_defaultSkybox);

            // capture deault background
            _defaultBackground = background;

            // Add the global light to the light stage (for later shadow rendering)
            _defaultBackgroundID = backgroundStage->addBackground(_defaultBackground);
        }
    }

    if (!_defaultHaze) {
        auto hazeStage = renderContext->_scene->getStage<HazeStage>();
        if (hazeStage) {
            auto haze = std::make_shared<graphics::Haze>();

            _defaultHaze = haze;
            _defaultHazeID = hazeStage->addHaze(_defaultHaze);
        }
    }
}

