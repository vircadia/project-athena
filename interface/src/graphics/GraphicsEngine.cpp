//
//  GraphicsEngine.cpp
//
//  Created by Sam Gateau on 29/6/2018.
//  Copyright 2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GraphicsEngine.h"

#include <shared/GlobalAppProperties.h>

#include "WorldBox.h"
#include "LODManager.h"

#include <GeometryCache.h>
#include <TextureCache.h>
#include <FramebufferCache.h>
#include <UpdateSceneTask.h>
#include <RenderViewTask.h>
#include <SecondaryCamera.h>

#include "RenderEventHandler.h"

#include <gpu/Batch.h>
#include <gpu/Context.h>
#include <gpu/gl/GLBackend.h>
#include <display-plugins/DisplayPlugin.h>

#include <display-plugins/CompositorHelper.h>
#include <QMetaObject>
#include "ui/Stats.h"
#include "Application.h"

GraphicsEngine::GraphicsEngine() {
    const QString SPLASH_SKYBOX { "{\"ProceduralEntity\":{ \"version\":2, \"shaderUrl\":\"qrc:///shaders/splashSkybox.frag\" } }" };
    _splashScreen->parse(SPLASH_SKYBOX);
}

GraphicsEngine::~GraphicsEngine() {
}

void GraphicsEngine::initializeGPU(GLWidget* glwidget) {

    _renderEventHandler = new RenderEventHandler(
        [this]() { return this->shouldPaint(); },
        [this]() { this->render_performFrame(); }
    );

    // Requires the window context, because that's what's used in the actual rendering
    // and the GPU backend will make things like the VAO which cannot be shared across
    // contexts
    glwidget->makeCurrent();
    gpu::Context::init<gpu::gl::GLBackend>();
    glwidget->makeCurrent();
    _gpuContext = std::make_shared<gpu::Context>();

#ifndef Q_OS_ANDROID
    _gpuContext->pushProgramsToSync(shader::allPrograms(), [this] {
        _programsCompiled.store(true);
    }, 1);
#endif

    DependencyManager::get<TextureCache>()->setGPUContext(_gpuContext);
}

void GraphicsEngine::initializeRender() {

    // Set up the render engine
    render::CullFunctor cullFunctor = LODManager::shouldRender;
    _renderEngine->addJob<UpdateSceneTask>("UpdateScene");
#ifndef Q_OS_ANDROID
    _renderEngine->addJob<SecondaryCameraRenderTask>("SecondaryCameraJob", cullFunctor);
#endif
    _renderEngine->addJob<RenderViewTask>("RenderMainView", cullFunctor, render::ItemKey::TAG_BITS_0, render::ItemKey::TAG_BITS_0);
    _renderEngine->load();
    _renderEngine->registerScene(_renderScene);

    // Now that OpenGL is initialized, we are sure we have a valid context and can create the various pipeline shaders with success.
    DependencyManager::get<GeometryCache>()->initializeShapePipelines();
}

void GraphicsEngine::startup() {
    static_cast<RenderEventHandler*>(_renderEventHandler)->resumeThread();
}

void GraphicsEngine::shutdown() {
    // The cleanup process enqueues the transactions but does not process them.  Calling this here will force the actual
    // removal of the items.
    // See https://highfidelity.fogbugz.com/f/cases/5328
    _renderScene->enqueueFrame(); // flush all the transactions
    _renderScene->processTransactionQueue(); // process and apply deletions

    _gpuContext->shutdown();


    // shutdown render engine
    _renderScene = nullptr;
    _renderEngine = nullptr;

    _renderEventHandler->deleteLater();
}


void GraphicsEngine::render_runRenderFrame(RenderArgs* renderArgs) {
    PROFILE_RANGE(render, __FUNCTION__);
    PerformanceTimer perfTimer("render");

    // Make sure the WorldBox is in the scene
    // For the record, this one RenderItem is the first one we created and added to the scene.
    // We could move that code elsewhere but you know...
    if (!render::Item::isValidID(WorldBoxRenderData::_item)) {
        render::Transaction transaction;
        auto worldBoxRenderData = std::make_shared<WorldBoxRenderData>();
        auto worldBoxRenderPayload = std::make_shared<WorldBoxRenderData::Payload>(worldBoxRenderData);

        WorldBoxRenderData::_item = _renderScene->allocateID();

        transaction.resetItem(WorldBoxRenderData::_item, worldBoxRenderPayload);
        _renderScene->enqueueTransaction(transaction);
    }

    {
        _renderEngine->getRenderContext()->args = renderArgs;
        _renderEngine->run();
    }
}

static const unsigned int THROTTLED_SIM_FRAMERATE = 15;
static const int THROTTLED_SIM_FRAME_PERIOD_MS = MSECS_PER_SECOND / THROTTLED_SIM_FRAMERATE;

bool GraphicsEngine::shouldPaint() const {
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    if (!displayPlugin) {
        // We're shutting down
        return false;
    }

#ifdef DEBUG_PAINT_DELAY
        static uint64_t paintDelaySamples{ 0 };
        static uint64_t paintDelayUsecs{ 0 };

        paintDelayUsecs += displayPlugin->getPaintDelayUsecs();

        static const int PAINT_DELAY_THROTTLE = 1000;
        if (++paintDelaySamples % PAINT_DELAY_THROTTLE == 0) {
            qCDebug(interfaceapp).nospace() <<
                "Paint delay (" << paintDelaySamples << " samples): " <<
                (float)paintDelaySamples / paintDelayUsecs << "us";
        }
#endif

    // Throttle if requested
    //if (displayPlugin->isThrottled() && (_graphicsEngine._renderEventHandler->_lastTimeRendered.elapsed() < THROTTLED_SIM_FRAME_PERIOD_MS)) {
    if (displayPlugin->isThrottled() &&
            (static_cast<RenderEventHandler*>(_renderEventHandler)->_lastTimeRendered.elapsed() < THROTTLED_SIM_FRAME_PERIOD_MS)) {
        return false;
    }

    return true;
}

bool GraphicsEngine::checkPendingRenderEvent() {
    bool expected = false;
    return (_renderEventHandler && static_cast<RenderEventHandler*>(_renderEventHandler)->_pendingRenderEvent.compare_exchange_strong(expected, true));
}

void GraphicsEngine::render_performFrame() {
    // Some plugins process message events, allowing paintGL to be called reentrantly.

    _renderFrameCount++;

    auto lastPaintBegin = usecTimestampNow();
    PROFILE_RANGE_EX(render, __FUNCTION__, 0xff0000ff, (uint64_t)_renderFrameCount);
    PerformanceTimer perfTimer("paintGL");

    DisplayPluginPointer displayPlugin;
    {
        PROFILE_RANGE(render, "/getActiveDisplayPlugin");
        displayPlugin = qApp->getActiveDisplayPlugin();
        if (!displayPlugin) {
            // We're shutting down
            return;
        }
    }

    {
        PROFILE_RANGE(render, "/pluginBeginFrameRender");
        // If a display plugin loses it's underlying support, it
        // needs to be able to signal us to not use it
        if (!displayPlugin->beginFrameRender(_renderFrameCount)) {
            QMetaObject::invokeMethod(qApp, "updateDisplayMode");
            return;
        }
    }

    RenderArgs renderArgs;
    glm::mat4  HMDSensorPose;
    glm::mat4  eyeToWorld;
    glm::mat4  sensorToWorld;
    ViewFrustum viewFrustum;

    bool isStereo;
    glm::mat4  stereoEyeOffsets[2];
    glm::mat4  stereoEyeProjections[2];

    {
        QMutexLocker viewLocker(&_renderArgsMutex);
        renderArgs = _appRenderArgs._renderArgs;

        // don't render if there is no context.
        if (!_appRenderArgs._renderArgs._context) {
            return;
        }

        HMDSensorPose = _appRenderArgs._headPose;
        eyeToWorld = _appRenderArgs._eyeToWorld;
        sensorToWorld = _appRenderArgs._sensorToWorld;
        isStereo = _appRenderArgs._isStereo;
        for_each_eye([&](Eye eye) {
            stereoEyeOffsets[eye] = _appRenderArgs._eyeOffsets[eye];
            stereoEyeProjections[eye] = _appRenderArgs._eyeProjections[eye];
        });
        viewFrustum = _appRenderArgs._renderArgs.getViewFrustum();
    }

    {
        PROFILE_RANGE(render, "/gpuContextReset");
        getGPUContext()->beginFrame(_appRenderArgs._view, HMDSensorPose);
        // Reset the gpu::Context Stages
        // Back to the default framebuffer;
        gpu::doInBatch("Application_render::gpuContextReset", getGPUContext(), [&](gpu::Batch& batch) {
            batch.resetStages();
        });

        if (isStereo) {
            renderArgs._context->enableStereo(true);
            renderArgs._context->setStereoProjections(stereoEyeProjections);
            renderArgs._context->setStereoViews(stereoEyeOffsets);
        }
    }

    gpu::FramebufferPointer finalFramebuffer;
    QSize finalFramebufferSize;
    {
        PROFILE_RANGE(render, "/getOutputFramebuffer");
        // Primary rendering pass
        auto framebufferCache = DependencyManager::get<FramebufferCache>();
        finalFramebufferSize = framebufferCache->getFrameBufferSize();
        // Final framebuffer that will be handed to the display-plugin
        finalFramebuffer = framebufferCache->getFramebuffer();
    }

    std::queue<Application::SnapshotOperator> snapshotOperators;
    if (!_programsCompiled.load()) {
        gpu::doInBatch("splashFrame", _gpuContext, [&](gpu::Batch& batch) {
            batch.setFramebuffer(finalFramebuffer);
            batch.enableSkybox(true);
            batch.enableStereo(isStereo);
            batch.clearDepthStencilFramebuffer(1.0, 0);
            batch.setViewportTransform({ 0, 0, finalFramebuffer->getSize() });
            _splashScreen->render(batch, viewFrustum, renderArgs._renderMethod == RenderArgs::RenderMethod::FORWARD, render::RenderEngine::TS_BACKGROUND_VIEW);
        });
    } else {
        {
            PROFILE_RANGE(render, "/renderOverlay");
            PerformanceTimer perfTimer("renderOverlay");
            // NOTE: There is no batch associated with this renderArgs
            // the ApplicationOverlay class assumes it's viewport is setup to be the device size
            renderArgs._viewport = glm::ivec4(0, 0, qApp->getDeviceSize());
            qApp->getApplicationOverlay().renderOverlay(&renderArgs);
        }

        {
            PROFILE_RANGE(render, "/updateCompositor");
            qApp->getApplicationCompositor().setFrameInfo(_renderFrameCount, eyeToWorld, sensorToWorld);
        }

        {
            PROFILE_RANGE(render, "/runRenderFrame");
            renderArgs._hudOperator = displayPlugin->getHUDOperator();
            renderArgs._hudTexture = qApp->getApplicationOverlay().getOverlayTexture();
            renderArgs._takingSnapshot = qApp->takeSnapshotOperators(snapshotOperators);
            renderArgs._blitFramebuffer = finalFramebuffer;
            render_runRenderFrame(&renderArgs);
        }
    }

    auto frame = getGPUContext()->endFrame();
    frame->frameIndex = _renderFrameCount;
    frame->framebuffer = finalFramebuffer;
    frame->framebufferRecycler = [](const gpu::FramebufferPointer& framebuffer) {
        auto frameBufferCache = DependencyManager::get<FramebufferCache>();
        if (frameBufferCache) {
            frameBufferCache->releaseFramebuffer(framebuffer);
        }
    };
    frame->snapshotOperators = snapshotOperators;
    // deliver final scene rendering commands to the display plugin
    {
        PROFILE_RANGE(render, "/pluginOutput");
        PerformanceTimer perfTimer("pluginOutput");
        _renderLoopCounter.increment();
        displayPlugin->submitFrame(frame);
    }

    // Reset the framebuffer and stereo state
    renderArgs._blitFramebuffer.reset();
    renderArgs._context->enableStereo(false);

#if !defined(DISABLE_QML)
    {
        auto stats = Stats::getInstance();
        if (stats) {
            stats->setRenderDetails(renderArgs._details);
        }
    }
#endif

    uint64_t lastPaintDuration = usecTimestampNow() - lastPaintBegin;
    _frameTimingsScriptingInterface.addValue(lastPaintDuration);
}

void GraphicsEngine::editRenderArgs(RenderArgsEditor editor) {
    QMutexLocker renderLocker(&_renderArgsMutex);
    editor(_appRenderArgs);
}
