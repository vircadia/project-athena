//
//  HighlightEffect.cpp
//  render-utils/src/
//
//  Created by Olivier Prat on 08/08/17.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "HighlightEffect.h"

#include <sstream>

#include <graphics/ShaderConstants.h>
#include <render/FilterTask.h>
#include <render/SortTask.h>

#include <gpu/Context.h>
#include <shaders/Shaders.h>

#include "GeometryCache.h"
#include "CubeProjectedPolygon.h"

#include "FadeEffect.h"

#include "render-utils/ShaderConstants.h"

using namespace render;
namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

#define OUTLINE_STENCIL_MASK    1

extern void initZPassPipelines(ShapePlumber& plumber, gpu::StatePointer state, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);

HighlightResources::HighlightResources() {
}

void HighlightResources::update(const gpu::FramebufferPointer& primaryFrameBuffer) {
    auto newFrameSize = glm::ivec2(primaryFrameBuffer->getSize());

    // If the buffer size changed, we need to delete our FBOs and recreate them at the
    // new correct dimensions.
    if (_frameSize != newFrameSize) {
        _frameSize = newFrameSize;
        allocateDepthBuffer(primaryFrameBuffer);
        allocateColorBuffer(primaryFrameBuffer);
    } else {
        if (!_depthFrameBuffer) {
            allocateDepthBuffer(primaryFrameBuffer);
        }
        if (!_colorFrameBuffer) {
            allocateColorBuffer(primaryFrameBuffer);
        }

        // The primaryFrameBuffer render buffer can change
        if (_colorFrameBuffer->getRenderBuffer(0) != primaryFrameBuffer->getRenderBuffer(0)) {
            _colorFrameBuffer->setRenderBuffer(0, primaryFrameBuffer->getRenderBuffer(0));
        }
    }
}

void HighlightResources::allocateColorBuffer(const gpu::FramebufferPointer& primaryFrameBuffer) {
    _colorFrameBuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("primaryWithStencil"));
    _colorFrameBuffer->setRenderBuffer(0, primaryFrameBuffer->getRenderBuffer(0));
    _colorFrameBuffer->setStencilBuffer(_depthStencilTexture, _depthStencilTexture->getTexelFormat());
}

void HighlightResources::allocateDepthBuffer(const gpu::FramebufferPointer& primaryFrameBuffer) {
    auto depthFormat = gpu::Element(gpu::SCALAR, gpu::UINT32, gpu::DEPTH_STENCIL);
    _depthStencilTexture = gpu::TexturePointer(gpu::Texture::createRenderBuffer(depthFormat, _frameSize.x, _frameSize.y));
    _depthFrameBuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("highlightDepth"));
    _depthFrameBuffer->setDepthStencilBuffer(_depthStencilTexture, depthFormat);
}

gpu::FramebufferPointer HighlightResources::getDepthFramebuffer() {
    assert(_depthFrameBuffer);
    return _depthFrameBuffer;
}

gpu::TexturePointer HighlightResources::getDepthTexture() {
    return _depthStencilTexture;
}

gpu::FramebufferPointer HighlightResources::getColorFramebuffer() {
    assert(_colorFrameBuffer);
    return _colorFrameBuffer;
}

HighlightSharedParameters::HighlightSharedParameters() {
    _highlightIds.fill(render::HighlightStage::INVALID_INDEX);
}

float HighlightSharedParameters::getBlurPixelWidth(const render::HighlightStyle& style, int frameBufferHeight) {
    return ceilf(style._outlineWidth * frameBufferHeight / 400.0f);
}

PrepareDrawHighlight::PrepareDrawHighlight() {
    _resources = std::make_shared<HighlightResources>();
}

void PrepareDrawHighlight::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    auto destinationFrameBuffer = inputs;

    _resources->update(destinationFrameBuffer);
    outputs = _resources;
}

gpu::PipelinePointer DrawHighlightMask::_stencilMaskPipeline;
gpu::PipelinePointer DrawHighlightMask::_stencilMaskFillPipeline;

DrawHighlightMask::DrawHighlightMask(unsigned int highlightIndex, render::ShapePlumberPointer shapePlumber,
    HighlightSharedParametersPointer parameters, uint transformSlot) :
    _highlightPassIndex(highlightIndex), _shapePlumber(shapePlumber), _sharedParameters(parameters), _transformSlot(transformSlot) {}

void DrawHighlightMask::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    auto& inShapes = inputs.get0();

    const int BOUNDS_SLOT = 0;
    const int PARAMETERS_SLOT = 0;

    if (!_stencilMaskPipeline || !_stencilMaskFillPipeline) {
        gpu::StatePointer state = std::make_shared<gpu::State>();
        state->setDepthTest(true, false, gpu::LESS_EQUAL);
        state->setStencilTest(true, 0xFF, gpu::State::StencilTest(OUTLINE_STENCIL_MASK, 0xFF, gpu::NOT_EQUAL, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_REPLACE));
        state->setColorWriteMask(false, false, false, false);
        state->setCullMode(gpu::State::CULL_FRONT);

        gpu::StatePointer fillState = std::make_shared<gpu::State>();
        fillState->setDepthTest(false, false, gpu::LESS_EQUAL);
        fillState->setStencilTest(true, 0xFF, gpu::State::StencilTest(OUTLINE_STENCIL_MASK, 0xFF, gpu::NOT_EQUAL, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_KEEP, gpu::State::STENCIL_OP_REPLACE));
        fillState->setColorWriteMask(false, false, false, false);
        fillState->setCullMode(gpu::State::CULL_FRONT);

        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render_utils::program::highlight_aabox);
        _stencilMaskPipeline = gpu::Pipeline::create(program, state);
        _stencilMaskFillPipeline = gpu::Pipeline::create(program, fillState);
    }

    if (!_boundsBuffer) {
        _boundsBuffer = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }

    auto highlightStage = renderContext->_scene->getStage<render::HighlightStage>(render::HighlightStage::getName());
    auto highlightId = _sharedParameters->_highlightIds[_highlightPassIndex];

    if (!inShapes.empty() && !render::HighlightStage::isIndexInvalid(highlightId)) {
        auto resources = inputs.get1();
        auto& highlight = highlightStage->getHighlight(highlightId);

        RenderArgs* args = renderContext->args;

        // Render full screen
        outputs = args->_viewport;

        // Clear the framebuffer without stereo
        // Needs to be distinct from the other batch because using the clear call 
        // while stereo is enabled triggers a warning
        gpu::doInBatch("DrawHighlightMask::run::begin", args->_context, [&](gpu::Batch& batch) {
            batch.enableStereo(false);
            batch.setFramebuffer(resources->getDepthFramebuffer());
            batch.clearDepthStencilFramebuffer(1.0f, 0);
        });

        render::ItemBounds itemBounds;

        gpu::doInBatch("DrawHighlightMask::run", args->_context, [&](gpu::Batch& batch) {
            args->_batch = &batch;

            // Setup camera, projection and viewport for all items
            glm::mat4 projMat;
            Transform viewMat;
            args->getViewFrustum().evalProjectionMatrix(projMat);
            args->getViewFrustum().evalViewTransform(viewMat);
            batch.setViewportTransform(args->_viewport);
            batch.setProjectionJitterEnabled(true);
            batch.setSavedViewProjectionTransform(_transformSlot);

            const std::vector<ShapeKey::Builder> keys = {
                ShapeKey::Builder(), ShapeKey::Builder().withFade(),
                ShapeKey::Builder().withDeformed(), ShapeKey::Builder().withDeformed().withFade(),
                ShapeKey::Builder().withDeformed().withDualQuatSkinned(), ShapeKey::Builder().withDeformed().withDualQuatSkinned().withFade(),
                ShapeKey::Builder().withOwnPipeline(), ShapeKey::Builder().withOwnPipeline().withFade(),
                ShapeKey::Builder().withDeformed().withOwnPipeline(), ShapeKey::Builder().withDeformed().withOwnPipeline().withFade(),
                ShapeKey::Builder().withDeformed().withDualQuatSkinned().withOwnPipeline(), ShapeKey::Builder().withDeformed().withDualQuatSkinned().withOwnPipeline().withFade(),
            };
            std::vector<std::vector<ShapeKey>> sortedShapeKeys(keys.size());

            const int OWN_PIPELINE_INDEX = 6;
            for (const auto& items : inShapes) {
                itemBounds.insert(itemBounds.end(), items.second.begin(), items.second.end());

                int index = items.first.hasOwnPipeline() ? OWN_PIPELINE_INDEX : 0;
                if (items.first.isDeformed()) {
                    index += 2;
                    if (items.first.isDualQuatSkinned()) {
                        index += 2;
                    }
                }

                if (items.first.isFaded()) {
                    index += 1;
                }

                sortedShapeKeys[index].push_back(items.first);
            }

            // Render non-withOwnPipeline things
            for (size_t i = 0; i < OWN_PIPELINE_INDEX; i++) {
                auto& shapeKeys = sortedShapeKeys[i];
                if (shapeKeys.size() > 0) {
                    const auto& shapePipeline = _shapePlumber->pickPipeline(args, keys[i]);
                    args->_shapePipeline = shapePipeline;
                    for (const auto& key : shapeKeys) {
                        renderShapes(renderContext, _shapePlumber, inShapes.at(key));
                    }
                }
            }

            // Render withOwnPipeline things
            for (size_t i = OWN_PIPELINE_INDEX; i < keys.size(); i++) {
                auto& shapeKeys = sortedShapeKeys[i];
                if (shapeKeys.size() > 0) {
                    args->_shapePipeline = nullptr;
                    for (const auto& key : shapeKeys) {
                        args->_itemShapeKey = key._flags.to_ulong();
                        renderShapes(renderContext, _shapePlumber, inShapes.at(key));
                    }
                }
            }

            args->_shapePipeline = nullptr;
            args->_batch = nullptr;
        });

        _boundsBuffer->setData(itemBounds.size() * sizeof(render::ItemBound), (const gpu::Byte*) itemBounds.data());

        const auto securityMargin = 2.0f;
        const float blurPixelWidth = 2.0f * securityMargin * HighlightSharedParameters::getBlurPixelWidth(highlight._style, args->_viewport.w);
        const auto framebufferSize = resources->getSourceFrameSize();
        const glm::vec2 highlightWidth = { blurPixelWidth / framebufferSize.x, blurPixelWidth / framebufferSize.y };

        if (highlightWidth != _outlineWidth.get()) {
            _outlineWidth.edit() = highlightWidth;
        }

        gpu::doInBatch("DrawHighlightMask::run::end", args->_context, [&](gpu::Batch& batch) {
            // Setup camera, projection and viewport for all items
            batch.setViewportTransform(args->_viewport);
            batch.setProjectionJitterEnabled(true);
            batch.setSavedViewProjectionTransform(_transformSlot);

            // Draw stencil mask with object bounding boxes
            auto stencilPipeline = highlight._style.isFilled() ? _stencilMaskFillPipeline : _stencilMaskPipeline;
            batch.setPipeline(stencilPipeline);
            batch.setResourceBuffer(BOUNDS_SLOT, _boundsBuffer);
            batch.setUniformBuffer(PARAMETERS_SLOT, _outlineWidth);
            static const int NUM_VERTICES_PER_CUBE = 36;
            batch.draw(gpu::TRIANGLES, NUM_VERTICES_PER_CUBE * (gpu::uint32) itemBounds.size(), 0);
        });
    } else {
        // Highlight rect should be null as there are no highlighted shapes
        outputs = glm::ivec4(0, 0, 0, 0);
    }
}

gpu::PipelinePointer DrawHighlight::_pipeline;
gpu::PipelinePointer DrawHighlight::_pipelineFilled;

DrawHighlight::DrawHighlight(unsigned int highlightIndex, HighlightSharedParametersPointer parameters) :
    _highlightPassIndex(highlightIndex), _sharedParameters(parameters) {
}

void DrawHighlight::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    auto highlightFrameBuffer = inputs.get1();
    auto highlightRect = inputs.get3();

    if (highlightFrameBuffer && highlightRect.z > 0 && highlightRect.w > 0) {
        auto sceneDepthBuffer = inputs.get2();
        const auto frameTransform = inputs.get0();
        auto highlightedDepthTexture = highlightFrameBuffer->getDepthTexture();
        auto destinationFrameBuffer = highlightFrameBuffer->getColorFramebuffer();
        auto framebufferSize = glm::ivec2(highlightedDepthTexture->getDimensions());

        if (sceneDepthBuffer) {
            auto args = renderContext->args;

            auto highlightStage = renderContext->_scene->getStage<render::HighlightStage>(render::HighlightStage::getName());
            auto highlightId = _sharedParameters->_highlightIds[_highlightPassIndex];
            if (!render::HighlightStage::isIndexInvalid(highlightId)) {
                auto& highlight = highlightStage->getHighlight(highlightId);
                auto pipeline = getPipeline(highlight._style);
                {
                    auto& shaderParameters = _configuration.edit();

                    shaderParameters._outlineUnoccludedColor = highlight._style._outlineUnoccluded.color;
                    shaderParameters._outlineUnoccludedAlpha = highlight._style._outlineUnoccluded.alpha * (highlight._style._isOutlineSmooth ? 2.0f : 1.0f);
                    shaderParameters._outlineOccludedColor = highlight._style._outlineOccluded.color;
                    shaderParameters._outlineOccludedAlpha = highlight._style._outlineOccluded.alpha * (highlight._style._isOutlineSmooth ? 2.0f : 1.0f);
                    shaderParameters._fillUnoccludedColor = highlight._style._fillUnoccluded.color;
                    shaderParameters._fillUnoccludedAlpha = highlight._style._fillUnoccluded.alpha;
                    shaderParameters._fillOccludedColor = highlight._style._fillOccluded.color;
                    shaderParameters._fillOccludedAlpha = highlight._style._fillOccluded.alpha;

                    shaderParameters._threshold = highlight._style._isOutlineSmooth ? 1.0f : 1e-3f;
                    shaderParameters._blurKernelSize = std::min(7, std::max(2, (int)floorf(highlight._style._outlineWidth * 3 + 0.5f)));
                    // Size is in normalized screen height. We decide that for highlight width = 1, this is equal to 1/400.
                    auto size = highlight._style._outlineWidth / 400.0f;
                    shaderParameters._size.x = (size * framebufferSize.y) / framebufferSize.x;
                    shaderParameters._size.y = size;
                }

                gpu::doInBatch("DrawHighlight::run", args->_context, [&](gpu::Batch& batch) {
                    batch.enableStereo(false);
                    batch.setFramebuffer(destinationFrameBuffer);

                    batch.setViewportTransform(args->_viewport);
                    batch.setProjectionTransform(glm::mat4());
                    batch.resetViewTransform();
                    batch.setModelTransform(gpu::Framebuffer::evalSubregionTexcoordTransform(framebufferSize, args->_viewport));
                    batch.setPipeline(pipeline);

                    batch.setUniformBuffer(ru::Buffer::HighlightParams, _configuration);
                    batch.setUniformBuffer(ru::Buffer::DeferredFrameTransform, frameTransform->getFrameTransformBuffer());
                    batch.setResourceTexture(ru::Texture::HighlightSceneDepth, sceneDepthBuffer->getPrimaryDepthTexture());
                    batch.setResourceTexture(ru::Texture::HighlightDepth, highlightedDepthTexture);
                    batch.draw(gpu::TRIANGLE_STRIP, 4);
                });
            }
        }
    }
}

const gpu::PipelinePointer& DrawHighlight::getPipeline(const render::HighlightStyle& style) {
    if (!_pipeline) {
        gpu::StatePointer state = std::make_shared<gpu::State>();
        state->setDepthTest(gpu::State::DepthTest(false, false));
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        state->setStencilTest(true, 0, gpu::State::StencilTest(OUTLINE_STENCIL_MASK, 0xFF, gpu::EQUAL));
        state->setColorWriteMask(true, true, true, true);

        auto program = gpu::Shader::createProgram(shader::render_utils::program::highlight);
        _pipeline = gpu::Pipeline::create(program, state);

        program = gpu::Shader::createProgram(shader::render_utils::program::highlight_filled);
        _pipelineFilled = gpu::Pipeline::create(program, state);
    }
    return style.isFilled() ? _pipelineFilled : _pipeline;
}

DebugHighlight::DebugHighlight(uint transformSlot) : _transformSlot(transformSlot) {
    _geometryDepthId = DependencyManager::get<GeometryCache>()->allocateID();
}

DebugHighlight::~DebugHighlight() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        geometryCache->releaseID(_geometryDepthId);
    }
}

void DebugHighlight::configure(const Config& config) {
    _isDisplayEnabled = config.viewMask;
}

void DebugHighlight::run(const render::RenderContextPointer& renderContext, const Inputs& input) {
    const auto highlightResources = input.get0();
    const auto highlightRect = input.get1();

    if (_isDisplayEnabled && highlightResources && highlightRect.z > 0 && highlightRect.w > 0) {
        assert(renderContext->args);
        assert(renderContext->args->hasViewFrustum());
        RenderArgs* args = renderContext->args;

        gpu::doInBatch("DebugHighlight::run", args->_context, [&](gpu::Batch& batch) {
            batch.setViewportTransform(args->_viewport);
            batch.setFramebuffer(highlightResources->getColorFramebuffer());

            const auto geometryBuffer = DependencyManager::get<GeometryCache>();

            batch.setProjectionJitterEnabled(true);
            batch.setSavedViewProjectionTransform(_transformSlot);
            batch.setModelTransform(Transform());

            const glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);

            batch.setPipeline(getDepthPipeline());
            batch.setResourceTexture(0, highlightResources->getDepthTexture());
            const glm::vec2 bottomLeft(-1.0f, -1.0f);
            const glm::vec2 topRight(1.0f, 1.0f);
            geometryBuffer->renderQuad(batch, bottomLeft, topRight, color, _geometryDepthId);

            batch.setResourceTexture(0, nullptr);
        });
    }
}

void DebugHighlight::initializePipelines() {
    static const std::string REPLACEMENT_MARKER{ "//SOURCE_PLACEHOLDER" };
    // Depth shader
    static const std::string DEPTH_SHADER{ R"SHADER(
            vec4 getFragmentColor() {
               float Zdb = texelFetch(depthMap, ivec2(gl_FragCoord.xy), 0).x;
               Zdb = 1.0-(1.0-Zdb)*100;
               return vec4(Zdb, Zdb, Zdb, 1.0); 
            }
        )SHADER" };
    static const auto& vs = gpu::Shader::createVertex(shader::render_utils::vertex::debug_deferred_buffer);


    gpu::Shader::Source fragmentSource;
    fragmentSource = gpu::Shader::Source::get(shader::render_utils::fragment::debug_deferred_buffer);
    fragmentSource.replacements[REPLACEMENT_MARKER] = DEPTH_SHADER;

    const auto ps = gpu::Shader::createPixel(fragmentSource);
    const auto program = gpu::Shader::createProgram(vs, ps);

    auto state = std::make_shared<gpu::State>();
    state->setDepthTest(gpu::State::DepthTest(false, false));
    state->setStencilTest(true, 0, gpu::State::StencilTest(OUTLINE_STENCIL_MASK, 0xFF, gpu::EQUAL));
    state->setColorWriteMask(true, true, true, true);

    _depthPipeline = gpu::Pipeline::create(program, state);
}

const gpu::PipelinePointer& DebugHighlight::getDepthPipeline() {
    if (!_depthPipeline) {
        initializePipelines();
    }

    return _depthPipeline;
}

void SelectionToHighlight::run(const render::RenderContextPointer& renderContext, Outputs& outputs) {
    auto scene = renderContext->_scene;
    auto highlightStage = scene->getStage<render::HighlightStage>(render::HighlightStage::getName());

    outputs.clear();
    _sharedParameters->_highlightIds.fill(render::HighlightStage::INVALID_INDEX);

    int numLayers = 0;
    auto highlightList = highlightStage->getActiveHighlightIds();

    for (auto styleId : highlightList) {
        auto highlight = highlightStage->getHighlight(styleId);

        if (!scene->isSelectionEmpty(highlight._selectionName)) {
            auto highlightId = highlightStage->getHighlightIdBySelection(highlight._selectionName);
            _sharedParameters->_highlightIds[outputs.size()] = highlightId;
            outputs.emplace_back(highlight._selectionName);
            numLayers++;

            if (numLayers == HighlightSharedParameters::MAX_PASS_COUNT) {
                break;
            }
        }
    }

    if (numLayers == 0) {
        renderContext->taskFlow.abortTask();
    }
}

void ExtractSelectionName::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs) {
    if (_highlightPassIndex < inputs.size()) {
        outputs = inputs[_highlightPassIndex];
    } else {
        outputs.clear();
    }
}

DrawHighlightTask::DrawHighlightTask() {

}

void DrawHighlightTask::configure(const Config& config) {

}

void DrawHighlightTask::build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, uint transformSlot) {
    const auto items = inputs.getN<Inputs>(0).get<RenderFetchCullSortTask::BucketList>();
    const auto sceneFrameBuffer = inputs.getN<Inputs>(1);
    const auto primaryFramebuffer = inputs.getN<Inputs>(2);
    const auto deferredFrameTransform = inputs.getN<Inputs>(3);

    // Prepare the ShapePipeline
    auto shapePlumber = std::make_shared<ShapePlumber>();
    {
        auto state = std::make_shared<gpu::State>();
        state->setDepthTest(true, true, gpu::LESS_EQUAL);
        state->setColorWriteMask(false, false, false, false);


        auto fadeEffect = DependencyManager::get<FadeEffect>();
        initZPassPipelines(*shapePlumber, state, fadeEffect->getBatchSetter(), fadeEffect->getItemUniformSetter());
    }
    auto sharedParameters = std::make_shared<HighlightSharedParameters>();

    const auto highlightSelectionNames = task.addJob<SelectionToHighlight>("SelectionToHighlight", sharedParameters);

    // Prepare for highlight group rendering.
    const auto highlightResources = task.addJob<PrepareDrawHighlight>("PrepareHighlight", primaryFramebuffer);
    render::Varying highlight0Rect;

    for (auto i = 0; i < HighlightSharedParameters::MAX_PASS_COUNT; i++) {
        const auto selectionName = task.addJob<ExtractSelectionName>("ExtractSelectionName", highlightSelectionNames, i);
        const auto groupItems = addSelectItemJobs(task, selectionName, items);
        const auto highlightedItemIDs = task.addJob<render::MetaToSubItems>("HighlightMetaToSubItemIDs", groupItems);
        const auto highlightedItems = task.addJob<render::IDsToBounds>("HighlightMetaToSubItems", highlightedItemIDs);

        // Sort
        const auto sortedPipelines = task.addJob<render::PipelineSortShapes>("HighlightPipelineSort", highlightedItems);
        const auto sortedBounds = task.addJob<render::DepthSortShapes>("HighlightDepthSort", sortedPipelines);

        // Draw depth of highlighted objects in separate buffer
        std::string name;
        {
            std::ostringstream stream;
            stream << "HighlightMask" << i;
            name = stream.str();
        }
        const auto drawMaskInputs = DrawHighlightMask::Inputs(sortedBounds, highlightResources).asVarying();
        const auto highlightedRect = task.addJob<DrawHighlightMask>(name, drawMaskInputs, i, shapePlumber, sharedParameters, transformSlot);
        if (i == 0) {
            highlight0Rect = highlightedRect;
        }

        // Draw highlight
        {
            std::ostringstream stream;
            stream << "HighlightEffect" << i;
            name = stream.str();
        }
        const auto drawHighlightInputs = DrawHighlight::Inputs(deferredFrameTransform, highlightResources, sceneFrameBuffer, highlightedRect).asVarying();
        task.addJob<DrawHighlight>(name, drawHighlightInputs, i, sharedParameters);
    }

    // Debug highlight
    const auto debugInputs = DebugHighlight::Inputs(highlightResources, const_cast<const render::Varying&>(highlight0Rect)).asVarying();
    task.addJob<DebugHighlight>("HighlightDebug", debugInputs, transformSlot);
}

const render::Varying DrawHighlightTask::addSelectItemJobs(JobModel& task, const render::Varying& selectionName,
                                                         const RenderFetchCullSortTask::BucketList& items) {
    const auto& opaques = items[RenderFetchCullSortTask::OPAQUE_SHAPE];
    const auto& transparents = items[RenderFetchCullSortTask::TRANSPARENT_SHAPE];
    const auto& metas = items[RenderFetchCullSortTask::META];

    const auto selectMetaInput = SelectItems::Inputs(metas, Varying(), selectionName).asVarying();
    const auto selectedMetas = task.addJob<SelectItems>("MetaSelection", selectMetaInput);
    const auto selectMetaAndOpaqueInput = SelectItems::Inputs(opaques, selectedMetas, selectionName).asVarying();
    const auto selectedMetasAndOpaques = task.addJob<SelectItems>("OpaqueSelection", selectMetaAndOpaqueInput);
    const auto selectItemInput = SelectItems::Inputs(transparents, selectedMetasAndOpaques, selectionName).asVarying();
    return task.addJob<SelectItems>("TransparentSelection", selectItemInput);
}

