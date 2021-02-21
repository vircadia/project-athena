//
//  StencilMaskPass.h
//  render-utils/src/
//
//  Created by Sam Gateau on 5/31/17.
//  Copyright 20154 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_StencilMaskPass_h
#define hifi_StencilMaskPass_h

#include <render/Engine.h>
#include <gpu/Pipeline.h>
#include <graphics/Geometry.h>
#include <StencilMaskMode.h>

class PrepareStencilConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(StencilMaskMode maskMode MEMBER maskMode NOTIFY dirty)

public:
    PrepareStencilConfig(bool enabled = true) : JobConfig(enabled) {}

    // -1 -> don't force drawing (fallback to render args mode)
    // 0 -> force draw without mesh
    // 1 -> force draw with mesh
    StencilMaskMode maskMode { StencilMaskMode::NONE };

signals:
    void dirty();
};

class PrepareStencil {
public:
    using Config = PrepareStencilConfig;

    using JobModel = render::Job::ModelI<PrepareStencil, gpu::FramebufferPointer, Config>;

    void configure(const Config& config);

    void run(const render::RenderContextPointer& renderContext, const gpu::FramebufferPointer& dstFramebuffer);

    // Always use 0 to clear the stencil buffer to set it to background
    static const gpu::int8 STENCIL_BACKGROUND = 0; // must match values in Skybox.cpp and ProceduralSkybox.cpp
    static const gpu::int8 STENCIL_MASK =       1 << 0;
    static const gpu::int8 STENCIL_SHAPE =      1 << 1;
    static const gpu::int8 STENCIL_NO_AA =      1 << 2;

    static void drawMask(gpu::State& state);
    static void drawBackground(gpu::State& state);
    static void testMask(gpu::State& state);
    static void testMaskResetNoAA(gpu::State& state);
    static void testNoAA(gpu::State& state);
    static void testBackground(gpu::State& state);
    static void testShape(gpu::State& state);
    static void testMaskDrawShape(gpu::State& state);
    static void testMaskDrawShapeNoAA(gpu::State& state);

private:
    gpu::PipelinePointer _meshStencilPipeline;
    gpu::PipelinePointer getMeshStencilPipeline();

    gpu::PipelinePointer _paintStencilPipeline;
    gpu::PipelinePointer getPaintStencilPipeline();

    graphics::MeshPointer _mesh;
    graphics::MeshPointer getMesh();

    StencilMaskMode _maskMode { StencilMaskMode::NONE };
};


#endif // hifi_StencilMaskPass_h
