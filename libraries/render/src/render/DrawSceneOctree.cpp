//
//  DrawSceneOctree.cpp
//  render/src/render
//
//  Created by Sam Gateau on 1/25/16.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DrawSceneOctree.h"

#include <algorithm>
#include <assert.h>

#include <OctreeUtils.h>
#include <PerfStat.h>

#include <gpu/Context.h>
#include <shaders/Shaders.h>

#include "Args.h"

using namespace render;

const gpu::PipelinePointer DrawSceneOctree::getDrawCellBoundsPipeline() {
    if (!_drawCellBoundsPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render::program::drawCellBounds);
        auto state = std::make_shared<gpu::State>();

        state->setDepthTest(true, false, gpu::LESS_EQUAL);

        // Blend on transparent
        state->setBlendFunction(true,  gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawCellBoundsPipeline = gpu::Pipeline::create(program, state);
        _cellBoundsFormat = std::make_shared<gpu::Stream::Format>();
        _cellBoundsFormat->setAttribute(0, 0, gpu::Element(gpu::VEC4, gpu::INT32, gpu::XYZW), 0, gpu::Stream::PER_INSTANCE);
        _cellBoundsBuffer = std::make_shared<gpu::Buffer>();
    }
    return _drawCellBoundsPipeline;
}

const gpu::PipelinePointer DrawSceneOctree::getDrawLODReticlePipeline() {
    if (!_drawLODReticlePipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render::program::drawLODReticle);
        auto state = std::make_shared<gpu::State>();

        // Blend on transparent
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawLODReticlePipeline = gpu::Pipeline::create(program, state);
    }
    return _drawLODReticlePipeline;
}

void DrawSceneOctree::configure(const Config& config) {
    _showVisibleCells = config.showVisibleCells;
    _showEmptyCells = config.showEmptyCells;
}


void DrawSceneOctree::run(const RenderContextPointer& renderContext, const ItemSpatialTree::ItemSelection& inSelection) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;
    auto& scene = renderContext->_scene;

    std::static_pointer_cast<Config>(renderContext->jobConfig)->numAllocatedCells = (int)scene->getSpatialTree().getNumAllocatedCells();
    std::static_pointer_cast<Config>(renderContext->jobConfig)->numFreeCells = (int)scene->getSpatialTree().getNumFreeCells();

    gpu::doInBatch("DrawSceneOctree::run", args->_context, [&](gpu::Batch& batch) {
        batch.setViewportTransform(args->_viewport);
        batch.setSavedViewProjectionTransform(_transformSlot);
        batch.setModelTransform(Transform());

        // bind the one gpu::Pipeline we need
        batch.setPipeline(getDrawCellBoundsPipeline());
        batch.setInputFormat(_cellBoundsFormat);

        std::vector<ivec4> cellBounds;
        auto drawCellBounds = [this, &cellBounds, &scene](const std::vector<gpu::Stamp>& cells) {
            cellBounds.reserve(cellBounds.size() + cells.size());
            for (const auto& cellID : cells) {
                auto cell = scene->getSpatialTree().getConcreteCell(cellID);
                auto cellLoc = cell.getlocation();
                glm::ivec4 cellLocation(cellLoc.pos.x, cellLoc.pos.y, cellLoc.pos.z, cellLoc.depth);

                bool empty = cell.isBrickEmpty() || !cell.hasBrick();
                if (empty) {
                    if (!_showEmptyCells) {
                        continue;
                    }
                    cellLocation.w *= -1.0;
                } else if (!empty && !_showVisibleCells) {
                    continue;
                }
                cellBounds.push_back(cellLocation);
            }
        };

        drawCellBounds(inSelection.cellSelection.insideCells);
        drawCellBounds(inSelection.cellSelection.partialCells);
        auto size = cellBounds.size() * sizeof(ivec4);
        if (size > _cellBoundsBuffer->getSize()) {
            _cellBoundsBuffer->resize(size);
        }
        _cellBoundsBuffer->setSubData(0, cellBounds);
        batch.setInputBuffer(0, _cellBoundsBuffer, 0, sizeof(ivec4));
        batch.drawInstanced((uint32_t)cellBounds.size(), gpu::LINES, 24);

        // Draw the LOD Reticle
        {
            float angle = glm::degrees(getPerspectiveAccuracyHalfAngle(args->_sizeScale, args->_boundaryLevelAdjust));
            Transform crosshairModel;
            crosshairModel.setTranslation(glm::vec3(0.0, 0.0, -1000.0));
            crosshairModel.setScale(1000.0f * tanf(glm::radians(angle))); // Scaling at the actual tan of the lod angle => Multiplied by TWO
            batch.resetViewTransform();
            batch.setModelTransform(crosshairModel);
            batch.setPipeline(getDrawLODReticlePipeline());
            batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
        }
    });
}

const gpu::PipelinePointer DrawItemSelection::getDrawItemBoundPipeline() {
    if (!_drawItemBoundPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render::program::drawItemBounds);

        auto state = std::make_shared<gpu::State>();

        state->setDepthTest(true, false, gpu::LESS_EQUAL);

        // Blend on transparent
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);

        // Good to go add the brand new pipeline
        _drawItemBoundPipeline = gpu::Pipeline::create(program, state);
    }
    return _drawItemBoundPipeline;
}

void DrawItemSelection::configure(const Config& config) {
    _showInsideItems = config.showInsideItems;
    _showInsideSubcellItems = config.showInsideSubcellItems;
    _showPartialItems = config.showPartialItems;
    _showPartialSubcellItems = config.showPartialSubcellItems;
}


void DrawItemSelection::run(const RenderContextPointer& renderContext, const ItemSpatialTree::ItemSelection& inSelection) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;
    auto& scene = renderContext->_scene;

    if (!_boundsBufferInside) {
        _boundsBufferInside = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }
    if (!_boundsBufferInsideSubcell) {
        _boundsBufferInsideSubcell = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }
    if (!_boundsBufferPartial) {
        _boundsBufferPartial = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }
    if (!_boundsBufferPartialSubcell) {
        _boundsBufferPartialSubcell = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }

    gpu::doInBatch("DrawItemSelection::run", args->_context, [&](gpu::Batch& batch) {
        batch.setViewportTransform(args->_viewport);
        batch.setSavedViewProjectionTransform(_transformSlot);
        batch.setModelTransform(Transform());

        // bind the one gpu::Pipeline we need
        batch.setPipeline(getDrawItemBoundPipeline());

        auto drawItemBounds = [&](const render::ItemIDs itemIDs, const gpu::BufferPointer buffer) {
            render::ItemBounds itemBounds;
            for (const auto& itemID : itemIDs) {
                auto& item = scene->getItem(itemID);
                auto itemBound = item.getBound(args);
                if (!itemBound.isInvalid()) {
                    itemBounds.emplace_back(itemID, itemBound);
                }
            }

            if (itemBounds.size() > 0) {
                buffer->setData(itemBounds.size() * sizeof(render::ItemBound), (const gpu::Byte*) itemBounds.data());
                batch.setResourceBuffer(0, buffer);
                batch.draw(gpu::LINES, (gpu::uint32) itemBounds.size() * 24, 0);
            }
        };

        if (_showInsideItems) {
            drawItemBounds(inSelection.insideItems, _boundsBufferInside);
        }
        if (_showInsideSubcellItems) {
            drawItemBounds(inSelection.insideSubcellItems, _boundsBufferInsideSubcell);
        }
        if (_showPartialItems) {
            drawItemBounds(inSelection.partialItems, _boundsBufferPartial);
        }
        if (_showPartialSubcellItems) {
            drawItemBounds(inSelection.partialSubcellItems, _boundsBufferPartialSubcell);
        }
        batch.setResourceBuffer(0, 0);
    });
}
