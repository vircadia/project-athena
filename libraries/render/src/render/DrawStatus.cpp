//
//  DrawStatus.cpp
//  render/src/render
//
//  Created by Niraj Venkat on 6/29/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DrawStatus.h"

#include <algorithm>
#include <assert.h>

#include <PerfStat.h>
#include <ViewFrustum.h>

#include "TransitionStage.h"

#include <gpu/Context.h>

#include <shaders/Shaders.h>

#include "Args.h"

using namespace render;

void DrawStatusConfig::dirtyHelper() {
    _isEnabled = showNetwork || showDisplay || showFade;
    emit dirty();
}

const gpu::PipelinePointer DrawStatus::getDrawItemBoundsPipeline() {
    if (!_drawItemBoundsPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render::program::drawItemBounds);

        auto state = std::make_shared<gpu::State>();

        state->setDepthTest(true, false, gpu::LESS_EQUAL);

        // Blend on transparent
        state->setBlendFunction(true,
            gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
            gpu::State::DEST_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ZERO);

        // Good to go add the brand new pipeline
        _drawItemBoundsPipeline = gpu::Pipeline::create(program, state);
    }
    return _drawItemBoundsPipeline;
}

const gpu::PipelinePointer DrawStatus::getDrawItemStatusPipeline() {
    if (!_drawItemStatusPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::render::program::drawItemStatus);

        auto state = std::make_shared<gpu::State>();

        state->setDepthTest(false, false, gpu::LESS_EQUAL);

        // Blend on transparent
        state->setBlendFunction(true,
            gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
            gpu::State::DEST_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ZERO);

        // Good to go add the brand new pipeline
        _drawItemStatusPipeline = gpu::Pipeline::create(program, state);
    }
    return _drawItemStatusPipeline;
}

void DrawStatus::setStatusIconMap(const gpu::TexturePointer& map) {
    _statusIconMap = map;
}

const gpu::TexturePointer DrawStatus::getStatusIconMap() const {
    return _statusIconMap;
}

void DrawStatus::configure(const Config& config) {
    _showDisplay = config.showDisplay;
    _showNetwork = config.showNetwork;
    _showFade = config.showFade;
}

void DrawStatus::run(const RenderContextPointer& renderContext, const Input& inItems) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    RenderArgs* args = renderContext->args;
    auto& scene = renderContext->_scene;
    const int NUM_STATUS_VEC4_PER_ITEM = 2;
    const int VEC4_LENGTH = 4;

    // First thing, we collect the bound and the status for all the items we want to render
    int nbItems = 0;
    render::ItemBounds itemBounds;
    std::vector<std::pair<glm::ivec4, glm::ivec4>> itemStatus;

    for (size_t i = 0; i < inItems.size(); ++i) {
        const auto& item = inItems[i];
        if (!item.bound.isInvalid()) {
            if (!item.bound.isNull()) {
                itemBounds.emplace_back(render::ItemBound(item.id, item.bound));
            } else {
                itemBounds.emplace_back(item.id, AABox(item.bound.getCorner(), 0.1f));
            }

            auto& itemScene = scene->getItem(item.id);

            if (_showNetwork || _showFade) {
                const static auto invalid = glm::ivec4(Item::Status::Value::INVALID.getPackedData());
                itemStatus.emplace_back(invalid, invalid);
                int vec4Num = 0;
                int vec4Component = 0;

                if (_showNetwork) {
                    auto itemStatusPointer = itemScene.getStatus();
                    if (itemStatusPointer) {
                        // Query the current status values, this is where the statusGetter lambda get called
                        auto&& currentStatusValues = itemStatusPointer->getCurrentValues();
                        for (const auto& statusValue : currentStatusValues) {
                            if (vec4Num == NUM_STATUS_VEC4_PER_ITEM) {
                                // Ran out of space
                                break;
                            }

                            auto& value = (vec4Num == 0 ? itemStatus[nbItems].first : itemStatus[nbItems].second);
                            value[vec4Component] = statusValue.getPackedData();

                            ++vec4Component;
                            if (vec4Component == VEC4_LENGTH) {
                                vec4Component = 0;
                                ++vec4Num;
                            }
                        }
                    }
                }

                if (_showFade && vec4Num != NUM_STATUS_VEC4_PER_ITEM) {
                    auto& value = (vec4Num == 0 ? itemStatus[nbItems].first : itemStatus[nbItems].second);

                    Item::Status::Value status;
                    auto transitionID = itemScene.getTransitionId();
                    if (transitionID != INVALID_INDEX) {
                        // We have a transition. Show this icon.
                        status.setScale(1.0f);
                        // Is this a valid transition ID according to FadeJob?
                        auto transitionStage = scene->getStage<TransitionStage>(TransitionStage::getName());
                        if (transitionStage) {
                            if (transitionStage->isTransitionUsed(transitionID)) {
                                // Valid, active transition
                                status.setColor(Item::Status::Value::CYAN);
                            } else {
                                // Render item has a defined transition ID, but it's unallocated and isn't being processed
                                status.setColor(Item::Status::Value::RED);
                            }
                            // Set icon based on transition type
                            auto& transition = transitionStage->getTransition(transitionID);
                            switch (transition.eventType) {
                            case Transition::Type::USER_ENTER_DOMAIN:
                                status.setIcon((unsigned char)Item::Status::Icon::USER_TRANSITION_IN);
                                break;
                            case Transition::Type::USER_LEAVE_DOMAIN:
                                status.setIcon((unsigned char)Item::Status::Icon::USER_TRANSITION_OUT);
                                break;
                            case Transition::ELEMENT_ENTER_DOMAIN:
                                status.setIcon((unsigned char)Item::Status::Icon::GENERIC_TRANSITION_IN);
                                break;
                            case Transition::ELEMENT_LEAVE_DOMAIN:
                                status.setIcon((unsigned char)Item::Status::Icon::GENERIC_TRANSITION_OUT);
                                break;
                            default:
                                status.setIcon((unsigned char)Item::Status::Icon::GENERIC_TRANSITION);
                                break;
                            }
                        } else {
                            // No way to determine transition
                            status.setScale(0.0f);
                        }
                    } else {
                        status.setScale(0.0f);
                    }
                    value[vec4Component] = status.getPackedData();

                    ++vec4Component;
                    if (vec4Component == VEC4_LENGTH) {
                        vec4Component = 0;
                        ++vec4Num;
                    }
                }
            }

            nbItems++;
        }
    }

    if (nbItems == 0) {
        return;
    }

    if (!_boundsBuffer) {
        _boundsBuffer = std::make_shared<gpu::Buffer>(sizeof(render::ItemBound));
    }

    // Alright, something to render let's do it
    gpu::doInBatch("DrawStatus::run", args->_context, [&](gpu::Batch& batch) {
        batch.setViewportTransform(args->_viewport);
        batch.setSavedViewProjectionTransform(_transformSlot);
        batch.setModelTransform(Transform());

        // bind the one gpu::Pipeline we need
        batch.setPipeline(getDrawItemBoundsPipeline());

        _boundsBuffer->setData(itemBounds.size() * sizeof(render::ItemBound), (const gpu::Byte*) itemBounds.data());

        if (_showDisplay) {
            batch.setResourceBuffer(0, _boundsBuffer);
            batch.draw(gpu::LINES, (gpu::uint32) itemBounds.size() * 24, 0);
        }
        batch.setResourceBuffer(0, 0);

        batch.setResourceTexture(0, gpu::TextureView(getStatusIconMap(), 0));

        batch.setPipeline(getDrawItemStatusPipeline());

        if (_showNetwork || _showFade) {
            if (!_instanceBuffer) {
                _instanceBuffer = std::make_shared<gpu::Buffer>();
            }

            struct InstanceData {
                vec4 boundPos;
                vec4 boundDim;
                ivec4 status0;
                ivec4 status1;
            };

            if (!_vertexFormat) {
                _vertexFormat = std::make_shared<gpu::Stream::Format>();
                _vertexFormat->setAttribute(0, 0, gpu::Element(gpu::VEC4, gpu::FLOAT, gpu::XYZW), offsetof(InstanceData, boundPos), gpu::Stream::PER_INSTANCE);
                _vertexFormat->setAttribute(1, 0, gpu::Element(gpu::VEC4, gpu::FLOAT, gpu::XYZW), offsetof(InstanceData, boundDim), gpu::Stream::PER_INSTANCE);
                _vertexFormat->setAttribute(2, 0, gpu::Element(gpu::VEC4, gpu::INT32, gpu::XYZW), offsetof(InstanceData, status0), gpu::Stream::PER_INSTANCE);
                _vertexFormat->setAttribute(3, 0, gpu::Element(gpu::VEC4, gpu::INT32, gpu::XYZW), offsetof(InstanceData, status1), gpu::Stream::PER_INSTANCE);
            }

            batch.setInputFormat(_vertexFormat);
            std::vector<InstanceData> instanceData;
            instanceData.resize(itemBounds.size());
            for (size_t i = 0; i < itemBounds.size(); i++) {
                InstanceData& item = instanceData[i];
                const auto& bound = itemBounds[i].bound;
                const auto& status = itemStatus[i];
                item.boundPos = vec4(bound.getCorner(), 1.0f);
                item.boundDim = vec4(bound.getScale(), 1.0f);
                item.status0 = status.first;
                item.status1 = status.second;
            }

            auto instanceBufferSize = sizeof(InstanceData) * instanceData.size();
            if (_instanceBuffer->getSize() < instanceBufferSize) {
                _instanceBuffer->resize(instanceBufferSize);
            }
            _instanceBuffer->setSubData(0, instanceData);
            batch.setInputBuffer(0, _instanceBuffer, 0, sizeof(InstanceData));
            batch.drawInstanced((uint32_t)instanceData.size(), gpu::TRIANGLES, 24 * NUM_STATUS_VEC4_PER_ITEM);
        }
        batch.setResourceTexture(0, 0);
    });
}
