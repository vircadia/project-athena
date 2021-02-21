
//
//  RenderPipelines.cpp
//  render-utils/src/
//
//  Created by Zach Pomerantz on 1/28/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderPipelines.h"

#include <functional>

#include <gpu/Context.h>
#include <material-networking/TextureCache.h>
#include <render/DrawTask.h>
#include <shaders/Shaders.h>
#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include "StencilMaskPass.h"
#include "DeferredLightingEffect.h"
#include "TextureCache.h"

using namespace render;
using namespace std::placeholders;

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

void initDeferredPipelines(ShapePlumber& plumber, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);
void initForwardPipelines(ShapePlumber& plumber);
void initZPassPipelines(ShapePlumber& plumber, gpu::StatePointer state, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);

void addPlumberPipeline(ShapePlumber& plumber,
        const ShapeKey& key, int programId,
        const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);

void batchSetter(const ShapePipeline& pipeline, gpu::Batch& batch, RenderArgs* args);
void lightBatchSetter(const ShapePipeline& pipeline, gpu::Batch& batch, RenderArgs* args);
static bool forceLightBatchSetter{ false };

void initDeferredPipelines(render::ShapePlumber& plumber, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter) {
    using namespace shader::render_utils::program;
    using Key = render::ShapeKey;
    auto addPipeline = std::bind(&addPlumberPipeline, std::ref(plumber), _1, _2, _3, _4);

    // TOOD: build this list algorithmically so we don't have to maintain it
    std::vector<std::pair<render::ShapeKey::Builder, uint32_t>> pipelines = {
        // Simple
        { Key::Builder(), simple },
        { Key::Builder().withTranslucent(), simple_translucent },
        { Key::Builder().withUnlit(), simple_unlit },
        { Key::Builder().withTranslucent().withUnlit(), simple_translucent_unlit },
        // Simple Fade
        { Key::Builder().withFade(), simple_fade },
        { Key::Builder().withTranslucent().withFade(), simple_translucent_fade },
        { Key::Builder().withUnlit().withFade(), simple_unlit_fade },
        { Key::Builder().withTranslucent().withUnlit().withFade(), simple_translucent_unlit_fade },

        // Unskinned
        { Key::Builder().withMaterial(), model },
        { Key::Builder().withMaterial().withTangents(), model_normalmap },
        { Key::Builder().withMaterial().withTranslucent(), model_translucent },
        { Key::Builder().withMaterial().withTangents().withTranslucent(), model_normalmap_translucent },
        // Unskinned Unlit
        { Key::Builder().withMaterial().withUnlit(), model_unlit },
        { Key::Builder().withMaterial().withTangents().withUnlit(), model_normalmap_unlit },
        { Key::Builder().withMaterial().withTranslucent().withUnlit(), model_translucent_unlit },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit(), model_normalmap_translucent_unlit },
        // Unskinned Lightmapped
        { Key::Builder().withMaterial().withLightMap(), model_lightmap },
        { Key::Builder().withMaterial().withTangents().withLightMap(), model_normalmap_lightmap },
        { Key::Builder().withMaterial().withTranslucent().withLightMap(), model_translucent_lightmap },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap(), model_normalmap_translucent_lightmap },
        // Unskinned Fade
        { Key::Builder().withMaterial().withFade(), model_fade },
        { Key::Builder().withMaterial().withTangents().withFade(), model_normalmap_fade },
        { Key::Builder().withMaterial().withTranslucent().withFade(), model_translucent_fade },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withFade(), model_normalmap_translucent_fade },
        // Unskinned Unlit Fade
        { Key::Builder().withMaterial().withUnlit().withFade(), model_unlit_fade },
        { Key::Builder().withMaterial().withTangents().withUnlit().withFade(), model_normalmap_unlit_fade },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withFade(), model_translucent_unlit_fade },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withFade(), model_normalmap_translucent_unlit_fade },
        // Unskinned Lightmapped Fade
        { Key::Builder().withMaterial().withLightMap().withFade(), model_lightmap_fade },
        { Key::Builder().withMaterial().withTangents().withLightMap().withFade(), model_normalmap_lightmap_fade },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withFade(), model_translucent_lightmap_fade },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withFade(), model_normalmap_translucent_lightmap_fade },

        // Matrix palette skinned
        { Key::Builder().withMaterial().withDeformed(), model_deformed },
        { Key::Builder().withMaterial().withTangents().withDeformed(), model_normalmap_deformed },
        { Key::Builder().withMaterial().withTranslucent().withDeformed(), model_translucent_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withDeformed(), model_normalmap_translucent_deformed },
        // Matrix palette skinned Unlit
        { Key::Builder().withMaterial().withUnlit().withDeformed(), model_unlit_deformed },
        { Key::Builder().withMaterial().withTangents().withUnlit().withDeformed(), model_normalmap_unlit_deformed },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withDeformed(), model_translucent_unlit_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withDeformed(), model_normalmap_translucent_unlit_deformed },
        // Matrix palette skinned Lightmapped
        { Key::Builder().withMaterial().withLightMap().withDeformed(), model_lightmap_deformed },
        { Key::Builder().withMaterial().withTangents().withLightMap().withDeformed(), model_normalmap_lightmap_deformed },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withDeformed(), model_translucent_lightmap_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withDeformed(), model_normalmap_translucent_lightmap_deformed },
        // Matrix palette skinned Fade
        { Key::Builder().withMaterial().withFade().withDeformed(), model_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withFade().withDeformed(), model_normalmap_fade_deformed },
        { Key::Builder().withMaterial().withTranslucent().withFade().withDeformed(), model_translucent_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withFade().withDeformed(), model_normalmap_translucent_fade_deformed },
        // Matrix palette skinned Unlit Fade
        { Key::Builder().withMaterial().withUnlit().withFade().withDeformed(), model_unlit_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withUnlit().withFade().withDeformed(), model_normalmap_unlit_fade_deformed },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withFade().withDeformed(), model_translucent_unlit_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withFade().withDeformed(), model_normalmap_translucent_unlit_fade_deformed },
        // Matrix palette skinned Lightmapped Fade
        { Key::Builder().withMaterial().withLightMap().withFade().withDeformed(), model_lightmap_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withLightMap().withFade().withDeformed(), model_normalmap_lightmap_fade_deformed },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withFade().withDeformed(), model_translucent_lightmap_fade_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withFade().withDeformed(), model_normalmap_translucent_lightmap_fade_deformed },

        // Dual quaternion skinned
        { Key::Builder().withMaterial().withDeformed().withDualQuatSkinned(), model_deformeddq },
        { Key::Builder().withMaterial().withTangents().withDeformed().withDualQuatSkinned(), model_normalmap_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withDeformed().withDualQuatSkinned(), model_translucent_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_deformeddq },
        // Dual quaternion skinned Unlit
        { Key::Builder().withMaterial().withUnlit().withDeformed().withDualQuatSkinned(), model_unlit_deformeddq },
        { Key::Builder().withMaterial().withTangents().withUnlit().withDeformed().withDualQuatSkinned(), model_normalmap_unlit_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withDeformed().withDualQuatSkinned(), model_translucent_unlit_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_unlit_deformeddq },
        // Dual quaternion skinned Lightmapped
        { Key::Builder().withMaterial().withLightMap().withDeformed().withDualQuatSkinned(), model_lightmap_deformeddq },
        { Key::Builder().withMaterial().withTangents().withLightMap().withDeformed().withDualQuatSkinned(), model_normalmap_lightmap_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withDeformed().withDualQuatSkinned(), model_translucent_lightmap_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_lightmap_deformeddq },
        // Dual quaternion skinned Fade
        { Key::Builder().withMaterial().withFade().withDeformed().withDualQuatSkinned(), model_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_fade_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withFade().withDeformed().withDualQuatSkinned(), model_translucent_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_fade_deformeddq },
        // Dual quaternion skinned Unlit Fade
        { Key::Builder().withMaterial().withUnlit().withFade().withDeformed().withDualQuatSkinned(), model_unlit_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withUnlit().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_unlit_fade_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withFade().withDeformed().withDualQuatSkinned(), model_translucent_unlit_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_unlit_fade_deformeddq },
        // Dual quaternion skinned Lightmapped Fade
        { Key::Builder().withMaterial().withLightMap().withFade().withDeformed().withDualQuatSkinned(), model_lightmap_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withLightMap().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_lightmap_fade_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withFade().withDeformed().withDualQuatSkinned(), model_translucent_lightmap_fade_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withFade().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_lightmap_fade_deformeddq },
    };

    for (auto& pipeline : pipelines) {
        if (pipeline.first.build().isFaded()) {
            addPipeline(pipeline.first, pipeline.second, batchSetter, itemSetter);
        } else {
            addPipeline(pipeline.first, pipeline.second, nullptr, nullptr);
        }
    }
}

void initForwardPipelines(ShapePlumber& plumber) {
    using namespace shader::render_utils::program;
    using Key = render::ShapeKey;
    auto addPipelineBind = std::bind(&addPlumberPipeline, std::ref(plumber), _1, _2, _3, _4);

    // Disable fade on the forward pipeline, all shaders get added twice, once with the fade key and once without
    auto addPipeline = [&](const ShapeKey& key, int programId) {
        addPipelineBind(key, programId, nullptr, nullptr);
        addPipelineBind(Key::Builder(key).withFade(), programId, nullptr, nullptr);
    };

    // Forward pipelines need the lightBatchSetter for opaques and transparents
    forceLightBatchSetter = true;

    // TOOD: build this list algorithmically so we don't have to maintain it
    std::vector<std::pair<render::ShapeKey::Builder, uint32_t>> pipelines = {
        // Simple
        { Key::Builder(), simple_forward },
        { Key::Builder().withTranslucent(), simple_translucent_forward },
        { Key::Builder().withUnlit(), simple_unlit_forward },
        { Key::Builder().withTranslucent().withUnlit(), simple_translucent_unlit_forward },

        // Unskinned
        { Key::Builder().withMaterial(), model_forward },
        { Key::Builder().withMaterial().withTangents(), model_normalmap_forward },
        { Key::Builder().withMaterial().withTranslucent(), model_translucent_forward },
        { Key::Builder().withMaterial().withTangents().withTranslucent(), model_normalmap_translucent_forward },
        // Unskinned Unlit
        { Key::Builder().withMaterial().withUnlit(), model_unlit_forward },
        { Key::Builder().withMaterial().withTangents().withUnlit(), model_normalmap_unlit_forward },
        { Key::Builder().withMaterial().withTranslucent().withUnlit(), model_translucent_unlit_forward },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit(), model_normalmap_translucent_unlit_forward },
        // Unskinned Lightmapped
        { Key::Builder().withMaterial().withLightMap(), model_lightmap_forward },
        { Key::Builder().withMaterial().withTangents().withLightMap(), model_normalmap_lightmap_forward },
        { Key::Builder().withMaterial().withTranslucent().withLightMap(), model_translucent_lightmap_forward },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap(), model_normalmap_translucent_lightmap_forward },

        // Matrix palette skinned
        { Key::Builder().withMaterial().withDeformed(), model_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withDeformed(), model_normalmap_forward_deformed },
        { Key::Builder().withMaterial().withTranslucent().withDeformed(), model_translucent_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withDeformed(), model_normalmap_translucent_forward_deformed },
        // Matrix palette skinned Unlit
        { Key::Builder().withMaterial().withUnlit().withDeformed(), model_unlit_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withUnlit().withDeformed(), model_normalmap_unlit_forward_deformed },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withDeformed(), model_translucent_unlit_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withDeformed(), model_normalmap_translucent_unlit_forward_deformed },
        // Matrix palette skinned Lightmapped
        { Key::Builder().withMaterial().withLightMap().withDeformed(), model_lightmap_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withLightMap().withDeformed(), model_normalmap_lightmap_forward_deformed },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withDeformed(), model_translucent_lightmap_forward_deformed },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withDeformed(), model_normalmap_translucent_lightmap_forward_deformed },

        // Dual quaternion skinned
        { Key::Builder().withMaterial().withDeformed().withDualQuatSkinned(), model_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withDeformed().withDualQuatSkinned(), model_normalmap_forward_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withDeformed().withDualQuatSkinned(), model_translucent_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_forward_deformeddq },
        // Dual quaternion skinned Unlit
        { Key::Builder().withMaterial().withUnlit().withDeformed().withDualQuatSkinned(), model_unlit_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withUnlit().withDeformed().withDualQuatSkinned(), model_normalmap_unlit_forward_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withUnlit().withDeformed().withDualQuatSkinned(), model_translucent_unlit_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withUnlit().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_unlit_forward_deformeddq },
        // Dual quaternion skinned Lightmapped
        { Key::Builder().withMaterial().withLightMap().withDeformed().withDualQuatSkinned(), model_lightmap_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withLightMap().withDeformed().withDualQuatSkinned(), model_normalmap_lightmap_forward_deformeddq },
        { Key::Builder().withMaterial().withTranslucent().withLightMap().withDeformed().withDualQuatSkinned(), model_translucent_lightmap_forward_deformeddq },
        { Key::Builder().withMaterial().withTangents().withTranslucent().withLightMap().withDeformed().withDualQuatSkinned(), model_normalmap_translucent_lightmap_forward_deformeddq },
    };

    for (auto& pipeline : pipelines) {
        addPipeline(pipeline.first, pipeline.second);
    }

    forceLightBatchSetter = false;
}

void addPlumberPipeline(ShapePlumber& plumber,
        const ShapeKey& key, int programId,
        const render::ShapePipeline::BatchSetter& extraBatchSetter, const render::ShapePipeline::ItemSetter& itemSetter) {
    // These key-values' pipelines are added by this functor in addition to the key passed
    assert(!key.isWireframe());
    assert(!key.isDepthBiased());
    assert(key.isCullFace());

    gpu::ShaderPointer program = gpu::Shader::createProgram(programId);

    for (int i = 0; i < 4; i++) {
        bool isBiased = (i & 1);
        bool isWireframed = (i & 2);
        for (int cullFaceMode = graphics::MaterialKey::CullFaceMode::CULL_NONE; cullFaceMode < graphics::MaterialKey::CullFaceMode::NUM_CULL_FACE_MODES; cullFaceMode++) {
            auto state = std::make_shared<gpu::State>();
            key.isTranslucent() ? PrepareStencil::testMaskResetNoAA(*state) : PrepareStencil::testMaskDrawShape(*state);

            // Depth test depends on transparency
            state->setDepthTest(true, !key.isTranslucent(), gpu::LESS_EQUAL);
            state->setBlendFunction(key.isTranslucent(),
                    gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
                    gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);

            ShapeKey::Builder builder(key);
            builder.withCullFaceMode((graphics::MaterialKey::CullFaceMode)cullFaceMode);
            state->setCullMode((gpu::State::CullMode)cullFaceMode);
            if (isWireframed) {
                builder.withWireframe();
                state->setFillMode(gpu::State::FILL_LINE);
            }
            if (isBiased) {
                builder.withDepthBias();
                state->setDepthBias(1.0f);
                state->setDepthBiasSlopeScale(1.0f);
            }

            auto baseBatchSetter = (forceLightBatchSetter || key.isTranslucent()) ? &lightBatchSetter : &batchSetter;
            render::ShapePipeline::BatchSetter finalBatchSetter;
            if (extraBatchSetter) {
                finalBatchSetter = [baseBatchSetter, extraBatchSetter](const ShapePipeline& pipeline, gpu::Batch& batch, render::Args* args) {
                    baseBatchSetter(pipeline, batch, args);
                    extraBatchSetter(pipeline, batch, args);
                };
            } else {
                finalBatchSetter = baseBatchSetter;
            }
            plumber.addPipeline(builder.build(), program, state, finalBatchSetter, itemSetter);
        }
    }
}

void batchSetter(const ShapePipeline& pipeline, gpu::Batch& batch, RenderArgs* args) {
    // Set a default albedo map
    batch.setResourceTexture(gr::Texture::MaterialAlbedo, DependencyManager::get<TextureCache>()->getWhiteTexture());

    // Set a default material
    if (pipeline.locations->materialBufferUnit) {
        // Create a default schema
        static gpu::BufferView schemaBuffer;
        static std::once_flag once;
        std::call_once(once, [] {
            graphics::MultiMaterial::Schema schema;
            graphics::MaterialKey schemaKey;

            schema._albedo = ColorUtils::sRGBToLinearVec3(vec3(1.0f));
            schema._opacity = 1.0f;
            schema._metallic = 0.1f;
            schema._roughness = 0.9f;

            schemaKey.setAlbedo(true);
            schemaKey.setTranslucentFactor(false);
            schemaKey.setMetallic(true);
            schemaKey.setGlossy(true);
            schema._key = (uint32_t)schemaKey._flags.to_ulong();

            auto schemaSize = sizeof(graphics::MultiMaterial::Schema);
            schemaBuffer = gpu::BufferView(std::make_shared<gpu::Buffer>(schemaSize, (const gpu::Byte*) &schema, schemaSize));
        });

        batch.setUniformBuffer(gr::Buffer::Material, schemaBuffer);
    }
}

void lightBatchSetter(const ShapePipeline& pipeline, gpu::Batch& batch, RenderArgs* args) {
    // Set the batch
    batchSetter(pipeline, batch, args);

    // Set the light
    if (pipeline.locations->keyLightBufferUnit) {
        DependencyManager::get<DeferredLightingEffect>()->setupKeyLightBatch(args, batch);
    }
}

void initZPassPipelines(ShapePlumber& shapePlumber, gpu::StatePointer state, const render::ShapePipeline::BatchSetter& extraBatchSetter, const render::ShapePipeline::ItemSetter& itemSetter) {
    using namespace shader::render_utils::program;

    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withoutDeformed().withoutFade(),
        gpu::Shader::createProgram(model_shadow), state);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withoutDeformed().withFade(),
        gpu::Shader::createProgram(model_shadow_fade), state, extraBatchSetter, itemSetter);

    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withDeformed().withoutDualQuatSkinned().withoutFade(),
        gpu::Shader::createProgram(model_shadow_deformed), state);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withDeformed().withoutDualQuatSkinned().withFade(),
        gpu::Shader::createProgram(model_shadow_fade_deformed), state, extraBatchSetter, itemSetter);

    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withDeformed().withDualQuatSkinned().withoutFade(),
        gpu::Shader::createProgram(model_shadow_deformeddq), state);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withDeformed().withDualQuatSkinned().withFade(),
        gpu::Shader::createProgram(model_shadow_fade_deformeddq), state, extraBatchSetter, itemSetter);
}

bool RenderPipelines::bindMaterial(graphics::MaterialPointer& material, gpu::Batch& batch, render::Args::RenderMode renderMode, bool enableTextures) {
    graphics::MultiMaterial multiMaterial;
    multiMaterial.push(graphics::MaterialLayer(material, 0));
    return bindMaterials(multiMaterial, batch, renderMode, enableTextures);
}

void RenderPipelines::updateMultiMaterial(graphics::MultiMaterial& multiMaterial) {
    auto& schemaBuffer = multiMaterial.getSchemaBuffer();

    auto& drawMaterialTextures = multiMaterial.getTextureTable();
    multiMaterial.setTexturesLoading(false);

    // The total list of things we need to look for
    static std::set<uint> allFlags;
    static std::once_flag once;
    std::call_once(once, [] {
        for (int i = 0; i < graphics::Material::NUM_TOTAL_FLAGS; i++) {
            // The opacity mask/map are derived from the albedo map
            // FIXME: OPACITY_MAP_MODE_BIT is supposed to support fallthrough
            if (i != graphics::MaterialKey::OPACITY_MASK_MAP_BIT &&
                    i != graphics::MaterialKey::OPACITY_TRANSLUCENT_MAP_BIT &&
                    i != graphics::MaterialKey::OPACITY_MAP_MODE_BIT) {
                allFlags.insert(i);
            }
        }
    });

    graphics::MultiMaterial materials = multiMaterial;
    graphics::MultiMaterial::Schema schema;
    graphics::MaterialKey schemaKey;

    std::set<uint> flagsToCheck = allFlags;
    std::set<uint> flagsToSetDefault;

    while (!materials.empty()) {
        auto material = materials.top().material;
        if (!material) {
            break;
        }
        materials.pop();

        bool defaultFallthrough = material->getDefaultFallthrough();
        const auto& materialKey = material->getKey();
        const auto& textureMaps = material->getTextureMaps();

        auto it = flagsToCheck.begin();
        while (it != flagsToCheck.end()) {
            auto flag = *it;
            bool fallthrough = defaultFallthrough || material->getPropertyFallthrough(flag);

            bool wasSet = false;
            bool forceDefault = false;
            switch (flag) {
                case graphics::MaterialKey::EMISSIVE_VAL_BIT:
                    if (materialKey.isEmissive()) {
                        schema._emissive = material->getEmissive(false);
                        schemaKey.setEmissive(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::UNLIT_VAL_BIT:
                    if (materialKey.isUnlit()) {
                        schemaKey.setUnlit(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::ALBEDO_VAL_BIT:
                    if (materialKey.isAlbedo()) {
                        schema._albedo = material->getAlbedo(false);
                        schemaKey.setAlbedo(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::METALLIC_VAL_BIT:
                    if (materialKey.isMetallic()) {
                        schema._metallic = material->getMetallic();
                        schemaKey.setMetallic(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::GLOSSY_VAL_BIT:
                    if (materialKey.isRough() || materialKey.isGlossy()) {
                        schema._roughness = material->getRoughness();
                        schemaKey.setGlossy(materialKey.isGlossy());
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::OPACITY_VAL_BIT:
                    if (materialKey.isTranslucentFactor()) {
                        schema._opacity = material->getOpacity();
                        schemaKey.setTranslucentFactor(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::OPACITY_CUTOFF_VAL_BIT:
                    if (materialKey.isOpacityCutoff()) {
                        schema._opacityCutoff = material->getOpacityCutoff();
                        schemaKey.setOpacityCutoff(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::SCATTERING_VAL_BIT:
                    if (materialKey.isScattering()) {
                        schema._scattering = material->getScattering();
                        schemaKey.setScattering(true);
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::ALBEDO_MAP_BIT:
                    if (materialKey.isAlbedoMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::ALBEDO_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                material->resetOpacityMap();
                                drawMaterialTextures->setTexture(gr::Texture::MaterialAlbedo, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setAlbedoMap(true);
                        schemaKey.setOpacityMaskMap(material->getKey().isOpacityMaskMap());
                        schemaKey.setTranslucentMap(material->getKey().isTranslucentMap());
                    }
                    break;
                case graphics::MaterialKey::METALLIC_MAP_BIT:
                    if (materialKey.isMetallicMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::METALLIC_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialMetallic, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setMetallicMap(true);
                    }
                    break;
                case graphics::MaterialKey::ROUGHNESS_MAP_BIT:
                    if (materialKey.isRoughnessMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::ROUGHNESS_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialRoughness, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setRoughnessMap(true);
                    }
                    break;
                case graphics::MaterialKey::NORMAL_MAP_BIT:
                    if (materialKey.isNormalMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::NORMAL_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialNormal, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setNormalMap(true);
                    }
                    break;
                case graphics::MaterialKey::OCCLUSION_MAP_BIT:
                    if (materialKey.isOcclusionMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::OCCLUSION_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialOcclusion, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setOcclusionMap(true);
                    }
                    break;
                case graphics::MaterialKey::SCATTERING_MAP_BIT:
                    if (materialKey.isScatteringMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::SCATTERING_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialScattering, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setScatteringMap(true);
                    }
                    break;
                case graphics::MaterialKey::EMISSIVE_MAP_BIT:
                    // Lightmap takes precendence over emissive map for legacy reasons
                    if (materialKey.isEmissiveMap() && !materialKey.isLightMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::EMISSIVE_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setEmissiveMap(true);
                    } else if (materialKey.isLightMap()) {
                        // We'll set this later when we check the lightmap
                        wasSet = true;
                    }
                    break;
                case graphics::MaterialKey::LIGHT_MAP_BIT:
                    if (materialKey.isLightMap()) {
                        auto itr = textureMaps.find(graphics::MaterialKey::LIGHT_MAP);
                        if (itr != textureMaps.end()) {
                            if (itr->second->isDefined()) {
                                drawMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, itr->second->getTextureView());
                                wasSet = true;
                            } else {
                                multiMaterial.setTexturesLoading(true);
                                forceDefault = true;
                            }
                        } else {
                            forceDefault = true;
                        }
                        schemaKey.setLightMap(true);
                    }
                    break;
                case graphics::Material::TEXCOORDTRANSFORM0:
                    if (!fallthrough) {
                        schema._texcoordTransforms[0] = material->getTexCoordTransform(0);
                        wasSet = true;
                    }
                    break;
                case graphics::Material::TEXCOORDTRANSFORM1:
                    if (!fallthrough) {
                        schema._texcoordTransforms[1] = material->getTexCoordTransform(1);
                        wasSet = true;
                    }
                    break;
                case graphics::Material::LIGHTMAP_PARAMS:
                    if (!fallthrough) {
                        schema._lightmapParams = material->getLightmapParams();
                        wasSet = true;
                    }
                    break;
                case graphics::Material::MATERIAL_PARAMS:
                    if (!fallthrough) {
                        schema._materialParams = material->getMaterialParams();
                        wasSet = true;
                    }
                    break;
                case graphics::Material::CULL_FACE_MODE:
                    if (!fallthrough) {
                        multiMaterial.setCullFaceMode(material->getCullFaceMode());
                        wasSet = true;
                    }
                    break;
                default:
                    break;
            }

            if (wasSet) {
                flagsToCheck.erase(it++);
            } else if (forceDefault || !fallthrough) {
                flagsToSetDefault.insert(flag);
                flagsToCheck.erase(it++);
            } else {
                ++it;
            }
        }

        if (flagsToCheck.empty()) {
            break;
        }
    }

    for (auto flagBit : flagsToCheck) {
        flagsToSetDefault.insert(flagBit);
    }

    auto textureCache = DependencyManager::get<TextureCache>();
    // Handle defaults
    for (auto flag : flagsToSetDefault) {
        switch (flag) {
            case graphics::MaterialKey::EMISSIVE_VAL_BIT:
            case graphics::MaterialKey::UNLIT_VAL_BIT:
            case graphics::MaterialKey::ALBEDO_VAL_BIT:
            case graphics::MaterialKey::METALLIC_VAL_BIT:
            case graphics::MaterialKey::GLOSSY_VAL_BIT:
            case graphics::MaterialKey::OPACITY_VAL_BIT:
            case graphics::MaterialKey::OPACITY_CUTOFF_VAL_BIT:
            case graphics::MaterialKey::SCATTERING_VAL_BIT:
            case graphics::Material::TEXCOORDTRANSFORM0:
            case graphics::Material::TEXCOORDTRANSFORM1:
            case graphics::Material::LIGHTMAP_PARAMS:
            case graphics::Material::MATERIAL_PARAMS:
                // these are initialized to the correct default values in Schema()
                break;
            case graphics::Material::CULL_FACE_MODE:
                multiMaterial.setCullFaceMode(graphics::Material::DEFAULT_CULL_FACE_MODE);
                break;
            case graphics::MaterialKey::ALBEDO_MAP_BIT:
                if (schemaKey.isAlbedoMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialAlbedo, textureCache->getWhiteTexture());
                }
                break;
            case graphics::MaterialKey::METALLIC_MAP_BIT:
                if (schemaKey.isMetallicMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialMetallic, textureCache->getBlackTexture());
                }
                break;
            case graphics::MaterialKey::ROUGHNESS_MAP_BIT:
                if (schemaKey.isRoughnessMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialRoughness, textureCache->getWhiteTexture());
                }
                break;
            case graphics::MaterialKey::NORMAL_MAP_BIT:
                if (schemaKey.isNormalMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialNormal, textureCache->getBlueTexture());
                }
                break;
            case graphics::MaterialKey::OCCLUSION_MAP_BIT:
                if (schemaKey.isOcclusionMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialOcclusion, textureCache->getWhiteTexture());
                }
                break;
            case graphics::MaterialKey::SCATTERING_MAP_BIT:
                if (schemaKey.isScatteringMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialScattering, textureCache->getWhiteTexture());
                }
                break;
            case graphics::MaterialKey::EMISSIVE_MAP_BIT:
                if (schemaKey.isEmissiveMap() && !schemaKey.isLightMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, textureCache->getGrayTexture());
                }
                break;
            case graphics::MaterialKey::LIGHT_MAP_BIT:
                if (schemaKey.isLightMap()) {
                    drawMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, textureCache->getBlackTexture());
                }
                break;
            default:
                break;
        }
    }

    schema._key = (uint32_t)schemaKey._flags.to_ulong();
    schemaBuffer.edit<graphics::MultiMaterial::Schema>() = schema;
    multiMaterial.setNeedsUpdate(false);
    multiMaterial.setInitialized();
}

bool RenderPipelines::bindMaterials(graphics::MultiMaterial& multiMaterial, gpu::Batch& batch, render::Args::RenderMode renderMode, bool enableTextures) {
    if (multiMaterial.shouldUpdate()) {
        updateMultiMaterial(multiMaterial);
    }

    auto textureCache = DependencyManager::get<TextureCache>();

    static gpu::TextureTablePointer defaultMaterialTextures = std::make_shared<gpu::TextureTable>();
    static gpu::BufferView defaultMaterialSchema;

    static std::once_flag once;
    std::call_once(once, [textureCache] {
        graphics::MultiMaterial::Schema schema;
        defaultMaterialSchema = gpu::BufferView(std::make_shared<gpu::Buffer>(sizeof(schema), (const gpu::Byte*) &schema, sizeof(schema)));

        defaultMaterialTextures->setTexture(gr::Texture::MaterialAlbedo, textureCache->getWhiteTexture());
        defaultMaterialTextures->setTexture(gr::Texture::MaterialMetallic, textureCache->getBlackTexture());
        defaultMaterialTextures->setTexture(gr::Texture::MaterialRoughness, textureCache->getWhiteTexture());
        defaultMaterialTextures->setTexture(gr::Texture::MaterialNormal, textureCache->getBlueTexture());
        defaultMaterialTextures->setTexture(gr::Texture::MaterialOcclusion, textureCache->getWhiteTexture());
        defaultMaterialTextures->setTexture(gr::Texture::MaterialScattering, textureCache->getWhiteTexture());
        // MaterialEmissiveLightmap has to be set later
    });

    // For shadows, we only need opacity mask information
    auto key = multiMaterial.getMaterialKey();
    if (renderMode != render::Args::RenderMode::SHADOW_RENDER_MODE || (key.isOpacityMaskMap() || key.isTranslucentMap())) {
        auto& schemaBuffer = multiMaterial.getSchemaBuffer();
        batch.setUniformBuffer(gr::Buffer::Material, schemaBuffer);
        if (enableTextures) {
            batch.setResourceTextureTable(multiMaterial.getTextureTable());
        } else {
            if (renderMode != render::Args::RenderMode::SHADOW_RENDER_MODE) {
                if (key.isLightMap()) {
                    defaultMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, textureCache->getBlackTexture());
                } else if (key.isEmissiveMap()) {
                    defaultMaterialTextures->setTexture(gr::Texture::MaterialEmissiveLightmap, textureCache->getGrayTexture());
                }
            }

            batch.setResourceTextureTable(defaultMaterialTextures);
        }
        return true;
    } else {
        batch.setResourceTextureTable(defaultMaterialTextures);
        batch.setUniformBuffer(gr::Buffer::Material, defaultMaterialSchema);
        return false;
    }
}

