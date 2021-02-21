//
//  Created by Bradley Austin Davis on 2016/05/16
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TestWindow.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QtCore/QTimer>
#include <QtGui/QResizeEvent>

#include <gl/GLHelpers.h>

#include <gpu/gl/GLBackend.h>

#include <GeometryCache.h>
#include <DeferredLightingEffect.h>
#include <FramebufferCache.h>
#include <TextureCache.h>

#ifdef DEFERRED_LIGHTING
extern void initDeferredPipelines(render::ShapePlumber& plumber, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);
extern void initStencilPipeline(gpu::PipelinePointer& pipeline);
#endif

TestWindow::TestWindow() : _generateDeferredFrameTransform(render::RenderEngine::TS_MAIN_VIEW) {
    setSurfaceType(QSurface::OpenGLSurface);


    auto timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(5);
    connect(timer, &QTimer::timeout, [&] { draw(); });
    timer->start();

    connect(qApp, &QCoreApplication::aboutToQuit, [this, timer] {
        timer->stop();
        _aboutToQuit = true;
    });

#ifdef DEFERRED_LIGHTING
    _light->setType(graphics::Light::SUN);
    _light->setAmbientSpherePreset(gpu::SphericalHarmonics::Preset::OLD_TOWN_SQUARE);
    _light->setIntensity(1.0f);
    _light->setAmbientIntensity(0.5f);
    _light->setColor(vec3(1.0f));
    _light->setPosition(vec3(1, 1, 1));
    _renderContext->args = _renderArgs;
#endif

    QSurfaceFormat format = getDefaultOpenGLSurfaceFormat();
    format.setOption(QSurfaceFormat::DebugContext);
    //format.setSwapInterval(0);
    setFormat(format);
    _glContext.setFormat(format);
    _glContext.create();
    _glContext.makeCurrent(this);
    show();
}

void TestWindow::initGl() {
    _glContext.makeCurrent(this);
    gpu::Context::init<gpu::gl::GLBackend>();
    _renderArgs->_context = std::make_shared<gpu::Context>();
    _glContext.makeCurrent(this);
    DependencyManager::set<GeometryCache>();
    DependencyManager::set<TextureCache>();
    DependencyManager::set<FramebufferCache>();
    DependencyManager::set<DeferredLightingEffect>();
    resize(QSize(800, 600));

#ifdef DEFERRED_LIGHTING
    auto deferredLightingEffect = DependencyManager::get<DeferredLightingEffect>();
    deferredLightingEffect->init();
    initDeferredPipelines(*_shapePlumber, nullptr, nullptr);
#endif
}

void TestWindow::resizeWindow(const QSize& size) {
    _size = size;
    _renderArgs->_viewport = ivec4(0, 0, _size.width(), _size.height());
    auto fboCache = DependencyManager::get<FramebufferCache>();
    if (fboCache) {
        fboCache->setFrameBufferSize(_size);
    }
}

void TestWindow::beginFrame() {

#ifdef DEFERRED_LIGHTING

    gpu::FramebufferPointer primaryFramebuffer;
    _preparePrimaryFramebuffer.run(_renderContext, primaryFramebuffer);

    DeferredFrameTransformPointer frameTransform;
    _generateDeferredFrameTransform.run(_renderContext, frameTransform);

    LightingModelPointer lightingModel;
    _generateLightingModel.run(_renderContext, lightingModel);

    _prepareDeferredInputs.edit0() = primaryFramebuffer;
    _prepareDeferredInputs.edit1() = lightingModel;
    _prepareDeferred.run(_renderContext, _prepareDeferredInputs, _prepareDeferredOutputs);


    _renderDeferredInputs.edit0() = frameTransform; // Pass the deferredFrameTransform
    _renderDeferredInputs.edit1() = _prepareDeferredOutputs.get0(); // Pass the deferredFramebuffer
    _renderDeferredInputs.edit2() = lightingModel; // Pass the lightingModel
    // the rest of the renderDeferred inputs can be omitted

#else
    gpu::doInBatch("TestWindow::beginFrame", _renderArgs->_context, [&](gpu::Batch& batch) {
        batch.clearColorFramebuffer(gpu::Framebuffer::BUFFER_COLORS, { 0.0f, 0.1f, 0.2f, 1.0f });
        batch.clearDepthFramebuffer(1e4);
        batch.setViewportTransform({ 0, 0, _size.width() * devicePixelRatio(), _size.height() * devicePixelRatio() });
    });
#endif

    gpu::doInBatch("TestWindow::beginFrame", _renderArgs->_context, [&](gpu::Batch& batch) {
        batch.setViewportTransform(_renderArgs->_viewport);
        batch.setStateScissorRect(_renderArgs->_viewport);
        batch.setProjectionTransform(_projectionMatrix);
    });
}

void TestWindow::endFrame() {
#ifdef DEFERRED_LIGHTING
    RenderArgs* args = _renderContext->args;
    gpu::doInBatch("TestWindow::endFrame::begin", args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;
        auto deferredFboColorDepthStencil = _prepareDeferredOutputs.get0()->getDeferredFramebufferDepthColor();
        batch.setViewportTransform(args->_viewport);
        batch.setStateScissorRect(args->_viewport);
        batch.setFramebuffer(deferredFboColorDepthStencil);
        batch.setPipeline(_opaquePipeline);
        batch.draw(gpu::TRIANGLE_STRIP, 4);
        batch.setResourceTexture(0, nullptr);
    });

    _renderDeferred.run(_renderContext, _renderDeferredInputs);

    gpu::doInBatch("TestWindow::endFrame::blit", _renderArgs->_context, [&](gpu::Batch& batch) {
        PROFILE_RANGE_BATCH(batch, "blit");
        // Blit to screen
        auto framebufferCache = DependencyManager::get<FramebufferCache>();
       // auto framebuffer = framebufferCache->getLightingFramebuffer();
        auto framebuffer = _prepareDeferredOutputs.get0()->getLightingFramebuffer();
        batch.blit(framebuffer, _renderArgs->_viewport, nullptr, _renderArgs->_viewport);
    });
#endif

    gpu::doInBatch("TestWindow::endFrame::finish", _renderArgs->_context, [&](gpu::Batch& batch) {
        batch.resetStages();
    });
    _glContext.swapBuffers(this);
}

void TestWindow::draw() {
    if (_aboutToQuit) {
        return;
    }

    // Attempting to draw before we're visible and have a valid size will
    // produce GL errors.
    if (!isVisible() || _size.width() <= 0 || _size.height() <= 0) {
        return;
    }

    if (!_glContext.makeCurrent(this)) {
        return;
    }

    static std::once_flag once;
    std::call_once(once, [&] { initGl(); });
    beginFrame();

    renderFrame();

    endFrame();
}

void TestWindow::resizeEvent(QResizeEvent* ev) {
    resizeWindow(ev->size());
    float fov_degrees = 60.0f;
    float aspect_ratio = (float)_size.width() / _size.height();
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    _projectionMatrix = glm::perspective(glm::radians(fov_degrees), aspect_ratio, near_clip, far_clip);
}
