//
//  Created by Bradley Austin Davis on 2015/05/29
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "OpenGLDisplayPlugin.h"

#include <condition_variable>
#include <queue>

#include <gl/Config.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QBuffer>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>
#include <QtGui/QImageWriter>
#include <QtGui/QOpenGLFramebufferObject>

#include <NumericalConstants.h>
#include <DependencyManager.h>
#include <GLMHelpers.h>

#include <gl/QOpenGLContextWrapper.h>
#include <gl/GLWidget.h>
#include <gl/GLEscrow.h>
#include <gl/Context.h>
#include <gl/OffscreenGLCanvas.h>

#include <gpu/Texture.h>
#include <gpu/FrameIO.h>
#include <shaders/Shaders.h>
#include <gpu/gl/GLShared.h>
#include <gpu/gl/GLBackend.h>
#include <gpu/gl/GLTexelFormat.h>
#include <GeometryCache.h>

#include <CursorManager.h>
#include <FramebufferCache.h>
#include <shared/NsightHelpers.h>
#include <ui-plugins/PluginContainer.h>
#include <ui/Menu.h>
#include <CursorManager.h>
#include <TextureCache.h>
#include "CompositorHelper.h"
#include "Logging.h"
#include "RefreshRateController.h"
#include <ThreadHelpers.h>

using namespace shader::gpu::program;

extern QThread* RENDER_THREAD;

class PresentThread : public QThread, public Dependency {
    using Mutex = std::mutex;
    using Condition = std::condition_variable;
    using Lock = std::unique_lock<Mutex>;

public:

    PresentThread() {
        connect(qApp, &QCoreApplication::aboutToQuit, [this] {
            shutdown(); 
        });
        setObjectName("Present");

        _refreshRateController = std::make_shared<RefreshRateController>();
    }

    ~PresentThread() {
        shutdown();
    }

    auto getRefreshRateController() { return _refreshRateController; }

    void shutdown() {
        if (isRunning()) {
            // First ensure that we turn off any current display plugin
            setNewDisplayPlugin(nullptr);

            Lock lock(_mutex);
            _shutdown = true;
            _condition.wait(lock, [&] { return !_shutdown; });
            qCDebug(displayPlugins) << "Present thread shutdown";
        }
    }

    void setNewDisplayPlugin(OpenGLDisplayPlugin* plugin) {
        Lock lock(_mutex);
        if (isRunning()) {
            _newPluginQueue.push(plugin);
            _condition.wait(lock, [=]()->bool { return _newPluginQueue.empty(); });
        }
    }

    void setContext(gl::Context* context) {
        // Move the OpenGL context to the present thread
        // Extra code because of the widget 'wrapper' context
        _context = context;
        _context->doneCurrent();
        _context->moveToThread(this);
    }

    virtual void run() override {
        PROFILE_SET_THREAD_NAME("Present Thread");

        // FIXME determine the best priority balance between this and the main thread...
        // It may be dependent on the display plugin being used, since VR plugins should
        // have higher priority on rendering (although we could say that the Oculus plugin
        // doesn't need that since it has async timewarp).
        // A higher priority here
        setPriority(QThread::HighPriority);
        OpenGLDisplayPlugin* currentPlugin{ nullptr };
        Q_ASSERT(_context);
        _context->makeCurrent();
        CHECK_GL_ERROR();
        while (!_shutdown) {
            if (_pendingOtherThreadOperation) {
                PROFILE_RANGE(render, "MainThreadOp")
                {
                    Lock lock(_mutex);
                    _context->doneCurrent();
                    // Move the context to the main thread
                    _context->moveToThread(_targetOperationThread);
                    _pendingOtherThreadOperation = false;
                    // Release the main thread to do it's action
                    _condition.notify_one();
                }

                {
                    // Main thread does it's thing while we wait on the lock to release
                    Lock lock(_mutex);
                    _condition.wait(lock, [&] { return _finishedOtherThreadOperation; });
                }
                _context->makeCurrent();
            }

            // Check for a new display plugin
            {
                Lock lock(_mutex);
                // Check if we have a new plugin to activate
                while (!_newPluginQueue.empty()) {
                    auto newPlugin = _newPluginQueue.front();
                    if (newPlugin != currentPlugin) {
                        // Deactivate the old plugin
                        if (currentPlugin != nullptr) {
                            currentPlugin->uncustomizeContext();
                            CHECK_GL_ERROR();
                            // Force completion of all pending GL commands
                            glFinish();
                        }

                        if (newPlugin) {
                            bool hasVsync = true;
                            QThread::setPriority(newPlugin->getPresentPriority());
                            bool wantVsync = newPlugin->wantVsync();
#if defined(Q_OS_MAC)
                            newPlugin->swapBuffers();
#endif
                            gl::setSwapInterval(wantVsync ? 1 : 0);
#if defined(Q_OS_MAC)
                            newPlugin->swapBuffers();
#endif
                            hasVsync = gl::getSwapInterval() != 0;
                            newPlugin->setVsyncEnabled(hasVsync);
                            newPlugin->customizeContext();
                            CHECK_GL_ERROR();
                            // Force completion of all pending GL commands
                            glFinish();
                        }
                        currentPlugin = newPlugin;
                    }
                    _newPluginQueue.pop();
                    _condition.notify_one();
                }
            }

            // If there's no active plugin, just sleep
            if (currentPlugin == nullptr) {
                // Minimum sleep ends up being about 2 ms anyway
                QThread::msleep(1);
                continue;
            }

#if defined(Q_OS_MAC)
            _context->makeCurrent();
#endif
            // Execute the frame and present it to the display device.
            {
                PROFILE_RANGE(render, "PluginPresent")
                gl::globalLock();
                currentPlugin->present(_refreshRateController);
                gl::globalRelease(false);
                CHECK_GL_ERROR();
            }
#if defined(Q_OS_MAC)
            _context->doneCurrent();
#endif

            _refreshRateController->sleepThreadIfNeeded(this, currentPlugin->isHmd());
        }

        _context->doneCurrent();
        Lock lock(_mutex);
        _context->moveToThread(qApp->thread());
        _shutdown = false;
        _condition.notify_one();
    }

    void withOtherThreadContext(std::function<void()> f) {
        // Signal to the thread that there is work to be done on the main thread
        Lock lock(_mutex);
        _targetOperationThread = QThread::currentThread();
        _pendingOtherThreadOperation = true;
        _finishedOtherThreadOperation = false;
        _condition.wait(lock, [&] { return !_pendingOtherThreadOperation; });

        _context->makeCurrent();
        f();
        _context->doneCurrent();

        _targetOperationThread = nullptr;
        // Move the context back to the presentation thread
        _context->moveToThread(this);

        // restore control of the context to the presentation thread and signal
        // the end of the operation
        _finishedOtherThreadOperation = true;
        lock.unlock();
        _condition.notify_one();
    }

private:
    void makeCurrent();
    void doneCurrent();

    bool _shutdown { false };
    Mutex _mutex;
    // Used to allow the main thread to perform context operations
    Condition _condition;

    QThread* _targetOperationThread { nullptr };
    bool _pendingOtherThreadOperation { false };
    bool _finishedOtherThreadOperation { false };
    std::queue<OpenGLDisplayPlugin*> _newPluginQueue;
    gl::Context* _context { nullptr };
    std::shared_ptr<RefreshRateController> _refreshRateController { nullptr };
};

bool OpenGLDisplayPlugin::activate() {
    if (!_cursorsData.size()) {
        auto& cursorManager = Cursor::Manager::instance();
        for (const auto iconId : cursorManager.registeredIcons()) {
            auto& cursorData = _cursorsData[iconId];
            auto iconPath = cursorManager.getIconImage(iconId);
            auto image = QImage(iconPath);
            image = image.mirrored();
            image = image.convertToFormat(QImage::Format_RGBA8888);
            cursorData.image = image;
            cursorData.size = toGlm(image.size());
            cursorData.hotSpot = vec2(0.5f);
        }
    }
    if (!_container) {
        return false;
    }

    // Start the present thread if necessary
    QSharedPointer<PresentThread> presentThread;
    if (DependencyManager::isSet<PresentThread>()) {
        presentThread = DependencyManager::get<PresentThread>();
    } else {
        auto widget = _container->getPrimaryWidget();
        DependencyManager::set<PresentThread>();
        presentThread = DependencyManager::get<PresentThread>();
        presentThread->setObjectName("Presentation Thread");
        if (!widget->context()->makeCurrent()) {
            throw std::runtime_error("Failed to make context current");
        }
        CHECK_GL_ERROR();
        widget->context()->doneCurrent();

        presentThread->setContext(widget->context());
        connect(presentThread.data(), &QThread::started, [] { setThreadName("OpenGL Present Thread"); });
        // Start execution
        presentThread->start();
    }
    _presentThread = presentThread.data();
    if (!RENDER_THREAD) {
        RENDER_THREAD = _presentThread;
    }

    // Child classes may override this in order to do things like initialize
    // libraries, etc
    if (!internalActivate()) {
        return false;
    }

    // This should not return until the new context has been customized
    // and the old context (if any) has been uncustomized
    presentThread->setNewDisplayPlugin(this);

    auto compositorHelper = DependencyManager::get<CompositorHelper>();
    connect(compositorHelper.data(), &CompositorHelper::alphaChanged, [this] {
        auto compositorHelper = DependencyManager::get<CompositorHelper>();
        auto animation = new QPropertyAnimation(this, "hudAlpha");
        animation->setDuration(200);
        animation->setEndValue(compositorHelper->getAlpha());
        animation->start();
    });

    if (isHmd() && (getHmdScreen() >= 0)) {
        _container->showDisplayPluginsTools();
    }

    return Parent::activate();
}

void OpenGLDisplayPlugin::deactivate() {
    auto compositorHelper = DependencyManager::get<CompositorHelper>();
    disconnect(compositorHelper.data());

    auto presentThread = DependencyManager::get<PresentThread>();
    // Does not return until the GL transition has completeed
    presentThread->setNewDisplayPlugin(nullptr);
    internalDeactivate();

    _container->showDisplayPluginsTools(false);
    if (!_container->currentDisplayActions().isEmpty()) {
        foreach (auto itemInfo, _container->currentDisplayActions()) {
            _container->removeMenuItem(itemInfo.first, itemInfo.second);
        }
        _container->currentDisplayActions().clear();
    }
    Parent::deactivate();
}

bool OpenGLDisplayPlugin::startStandBySession() {
    if (!activateStandBySession()) {
        return false;
    }
    return Parent::startStandBySession();
}

void OpenGLDisplayPlugin::endSession() {
    deactivateSession();
    Parent::endSession();
}

void OpenGLDisplayPlugin::customizeContext() {
    auto presentThread = DependencyManager::get<PresentThread>();
    Q_ASSERT(thread() == presentThread->thread());

    getGLBackend()->updatePresentFrame(mat4());

    for (auto& cursorValue : _cursorsData) {
        auto& cursorData = cursorValue.second;
        if (!cursorData.texture) {
            auto image = cursorData.image;
            if (image.format() != QImage::Format_ARGB32) {
                image = image.convertToFormat(QImage::Format_ARGB32);
            }
            if ((image.width() > 0) && (image.height() > 0)) {
                cursorData.texture = gpu::Texture::createStrict(
                    gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA),
                    image.width(), image.height(),
                    gpu::Texture::MAX_NUM_MIPS,
                    gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR));
                cursorData.texture->setSource("cursor texture");
                auto usage = gpu::Texture::Usage::Builder().withColor().withAlpha();
                cursorData.texture->setUsage(usage.build());
                cursorData.texture->setStoredMipFormat(gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));
                cursorData.texture->assignStoredMip(0, image.sizeInBytes(), image.constBits());
                cursorData.texture->setAutoGenerateMips(true);
            }
        }
    }

    if (!_linearToSRGBPipeline) {
        gpu::StatePointer blendState = gpu::StatePointer(new gpu::State());
        blendState->setDepthTest(gpu::State::DepthTest(false));
        blendState->setBlendFunction(true,
                                     gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD,
                                     gpu::State::INV_SRC_ALPHA,
                                     gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD,
                                     gpu::State::ONE);

        gpu::StatePointer scissorState = gpu::StatePointer(new gpu::State());
        scissorState->setDepthTest(gpu::State::DepthTest(false));
        scissorState->setScissorEnable(true);

        _drawTexturePipeline = gpu::Pipeline::create(gpu::Shader::createProgram(DrawTexture), scissorState);

        _drawTextureSqueezePipeline =
            gpu::Pipeline::create(gpu::Shader::createProgram(shader::display_plugins::program::DrawTextureWithVisionSqueeze), scissorState);

        _linearToSRGBPipeline = gpu::Pipeline::create(gpu::Shader::createProgram(DrawTextureLinearToSRGB), scissorState);

        _SRGBToLinearPipeline = gpu::Pipeline::create(gpu::Shader::createProgram(DrawTextureSRGBToLinear), scissorState);

        _hudPipeline = gpu::Pipeline::create(gpu::Shader::createProgram(DrawTextureSRGBToLinear), blendState);

        _cursorPipeline = gpu::Pipeline::create(gpu::Shader::createProgram(DrawTransformedTexture), blendState);
    }

    updateCompositeFramebuffer();
}

void OpenGLDisplayPlugin::uncustomizeContext() {

    _drawTexturePipeline.reset();
    _drawTextureSqueezePipeline.reset();
    _linearToSRGBPipeline.reset();
    _SRGBToLinearPipeline.reset();
    _cursorPipeline.reset();
    _hudPipeline.reset();
    _compositeFramebuffer.reset();

    withPresentThreadLock([&] {
        _currentFrame.reset();
        _lastFrame = nullptr;
        while (!_newFrameQueue.empty()) {
            _gpuContext->consumeFrameUpdates(_newFrameQueue.front());
            _newFrameQueue.pop();
        }
    });
}

// Pressing Alt (and Meta) key alone activates the menubar because its style inherits the
// SHMenuBarAltKeyNavigation from QWindowsStyle. This makes it impossible for a scripts to
// receive keyPress events for the Alt (and Meta) key in a reliable manner.
//
// This filter catches events before QMenuBar can steal the keyboard focus.
// The idea was borrowed from
// http://www.archivum.info/qt-interest@trolltech.com/2006-09/00053/Re-(Qt4)-Alt-key-focus-QMenuBar-(solved).html

// Pass input events on to the application
bool OpenGLDisplayPlugin::eventFilter(QObject* receiver, QEvent* event) {
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:

        case QEvent::TouchBegin:
        case QEvent::TouchEnd:
        case QEvent::TouchUpdate:

        case QEvent::FocusIn:
        case QEvent::FocusOut:

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::ShortcutOverride:

        case QEvent::DragEnter:
        case QEvent::Drop:

        case QEvent::Resize:
            if (QCoreApplication::sendEvent(QCoreApplication::instance(), event)) {
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

void OpenGLDisplayPlugin::submitFrame(const gpu::FramePointer& newFrame) {
    withNonPresentThreadLock([&] {
        _newFrameQueue.push(newFrame);
    });
}

ktx::StoragePointer textureToKtx(const gpu::Texture& texture) {
    ktx::Header header;
    {
        auto gpuDims = texture.getDimensions();
        header.pixelWidth = gpuDims.x;
        header.pixelHeight = gpuDims.y;
        header.pixelDepth = 0;
    }

    {
        auto gltexelformat = gpu::gl::GLTexelFormat::evalGLTexelFormat(texture.getStoredMipFormat());
        header.glInternalFormat = gltexelformat.internalFormat;
        header.glFormat = gltexelformat.format;
        header.glBaseInternalFormat = gltexelformat.format;
        header.glType = gltexelformat.type;
        header.glTypeSize = 1;
        header.numberOfMipmapLevels = 1 + texture.getMaxMip();
    }

    auto memKtx = ktx::KTX::createBare(header);
    auto storage = memKtx->_storage;
    uint32_t faceCount = std::max(header.numberOfFaces, 1u);
    uint32_t mipCount = std::max(header.numberOfMipmapLevels, 1u);
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        for (uint32_t face = 0; face < faceCount; ++face) {
            const auto& image = memKtx->_images[mip];
            auto& faceBytes = const_cast<gpu::Byte*&>(image._faceBytes[face]);
            if (texture.isStoredMipFaceAvailable(mip, face)) {
                auto storedImage = texture.accessStoredMipFace(mip, face);
                auto storedSize = storedImage->size();
                memcpy(faceBytes, storedImage->data(), storedSize);
            }
        }
    }
    return storage;
}

void OpenGLDisplayPlugin::captureFrame(const std::string& filename) const {
    withOtherThreadContext([&] {
        using namespace gpu;
        TextureCapturer captureLambda = [&](const gpu::TexturePointer& texture)->storage::StoragePointer {
            return textureToKtx(*texture);
        };

        if (_currentFrame) {
            gpu::writeFrame(filename, _currentFrame, captureLambda);
        }
    });
}

void OpenGLDisplayPlugin::renderFromTexture(gpu::Batch& batch,
                                            const gpu::TexturePointer& texture,
                                            const glm::ivec4& viewport,
                                            const glm::ivec4& scissor) {
    renderFromTexture(batch, texture, viewport, scissor, nullptr);
}

void OpenGLDisplayPlugin::renderFromTexture(gpu::Batch& batch,
                                            const gpu::TexturePointer& texture,
                                            const glm::ivec4& viewport,
                                            const glm::ivec4& scissor,
                                            const gpu::FramebufferPointer& copyFbo /*=gpu::FramebufferPointer()*/) {
    auto fbo = gpu::FramebufferPointer();
    batch.enableStereo(false);
    batch.resetViewTransform();
    batch.setFramebuffer(fbo);
    batch.clearColorFramebuffer(gpu::Framebuffer::BUFFER_COLOR0, vec4(0));
    batch.setStateScissorRect(scissor);
    batch.setViewportTransform(viewport);
    batch.setResourceTexture(0, texture);

    batch.setPipeline(getRenderTexturePipeline());

    batch.draw(gpu::TRIANGLE_STRIP, 4);
    if (copyFbo) {
        gpu::Vec4i copyFboRect(0, 0, copyFbo->getWidth(), copyFbo->getHeight());
        gpu::Vec4i sourceRect(scissor.x, scissor.y, scissor.x + scissor.z, scissor.y + scissor.w);
        float aspectRatio = (float)scissor.w / (float)scissor.z;  // height/width
        // scale width first
        int xOffset = 0;
        int yOffset = 0;
        int newWidth = copyFbo->getWidth();
        int newHeight = std::round(aspectRatio * (float) copyFbo->getWidth());
        if (newHeight > copyFbo->getHeight()) {
            // ok, so now fill height instead
            newHeight = copyFbo->getHeight();
            newWidth = std::round((float)copyFbo->getHeight() / aspectRatio);
            xOffset = (copyFbo->getWidth() - newWidth) / 2;
        } else {
            yOffset = (copyFbo->getHeight() - newHeight) / 2;
        }
        gpu::Vec4i copyRect(xOffset, yOffset, xOffset + newWidth, yOffset + newHeight);
        batch.setFramebuffer(copyFbo);

        batch.resetViewTransform();
        batch.setViewportTransform(copyFboRect);
        batch.setStateScissorRect(copyFboRect);
        batch.clearColorFramebuffer(gpu::Framebuffer::BUFFER_COLOR0, {0.0f, 0.0f, 0.0f, 1.0f});
        batch.blit(fbo, sourceRect, copyFbo, copyRect);
    }
}

void OpenGLDisplayPlugin::updateFrameData() {
    PROFILE_RANGE(render, __FUNCTION__)
    if (_lockCurrentTexture) {
        return;
    }
    withPresentThreadLock([&] {
        if (!_newFrameQueue.empty()) {
            // We're changing frames, so we can cleanup any GL resources that might have been used by the old frame
            _gpuContext->recycle();
        }
        if (_newFrameQueue.size() > 1) {
            _droppedFrameRate.increment(_newFrameQueue.size() - 1);
        }

        _gpuContext->processProgramsToSync();

        while (!_newFrameQueue.empty()) {
            _currentFrame = _newFrameQueue.front();
            _newFrameQueue.pop();
            _gpuContext->consumeFrameUpdates(_currentFrame);
        }
    });
}

std::function<void(gpu::Batch&, const gpu::TexturePointer&)> OpenGLDisplayPlugin::getHUDOperator() {
    auto hudPipeline = _hudPipeline;
    auto hudCompositeFramebufferSize = getRecommendedRenderSize();
    return [=](gpu::Batch& batch, const gpu::TexturePointer& hudTexture) {
        if (hudPipeline && hudTexture) {
            batch.setPipeline(hudPipeline);
            batch.setResourceTexture(0, hudTexture);
            batch.setViewportTransform(ivec4(uvec2(0), hudCompositeFramebufferSize));
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        }
    };
}

void OpenGLDisplayPlugin::compositePointer() {
    auto& cursorManager = Cursor::Manager::instance();
    const auto& cursorData = _cursorsData[cursorManager.getCursor()->getIcon()];
    auto cursorTransform = DependencyManager::get<CompositorHelper>()->getReticleTransform(glm::mat4());
    render([&](gpu::Batch& batch) {
        batch.setProjectionTransform(mat4());
        batch.setFramebuffer(_compositeFramebuffer);
        batch.setPipeline(_cursorPipeline);
        batch.setResourceTexture(0, cursorData.texture);
        batch.resetViewTransform();
        batch.setModelTransform(cursorTransform);
        batch.setViewportTransform(ivec4(uvec2(0), _compositeFramebuffer->getSize()));
        batch.draw(gpu::TRIANGLE_STRIP, 4);
    });
}

void OpenGLDisplayPlugin::setupCompositeScenePipeline(gpu::Batch& batch) {
    batch.setPipeline(_drawTexturePipeline);
}

void OpenGLDisplayPlugin::compositeScene() {
    render([&](gpu::Batch& batch) {
        batch.enableStereo(false);
        batch.setFramebuffer(_compositeFramebuffer);
        batch.setViewportTransform(ivec4(uvec2(), _compositeFramebuffer->getSize()));
        batch.setStateScissorRect(ivec4(uvec2(), _compositeFramebuffer->getSize()));
        batch.resetViewTransform();
        batch.setProjectionTransform(mat4());
        batch.setResourceTexture(0, _currentFrame->framebuffer->getRenderBuffer(0));
        setupCompositeScenePipeline(batch);
        batch.draw(gpu::TRIANGLE_STRIP, 4);
    });
}

void OpenGLDisplayPlugin::compositeLayers() {
    updateCompositeFramebuffer();

    {
        PROFILE_RANGE_EX(render_detail, "compositeScene", 0xff0077ff, (uint64_t)presentCount())
        compositeScene();
    }

    {
        PROFILE_RANGE_EX(render_detail, "compositeExtra", 0xff0077ff, (uint64_t)presentCount())
        compositeExtra();
    }

    auto& cursorManager = Cursor::Manager::instance();
    if (isHmd() || cursorManager.getCursor()->getIcon() == Cursor::RETICLE) {
        auto compositorHelper = DependencyManager::get<CompositorHelper>();
        // Draw the pointer last so it's on top of everything
        if (compositorHelper->getReticleVisible()) {
            PROFILE_RANGE_EX(render_detail, "compositePointer", 0xff0077ff, (uint64_t)presentCount())
            compositePointer();
        }
    }
}

void OpenGLDisplayPlugin::internalPresent() {
    render([&](gpu::Batch& batch) {
        // Note: _displayTexture must currently be the same size as the display.
        uvec2 dims = _displayTexture ? uvec2(_displayTexture->getDimensions()) : getSurfacePixels();
        auto viewport = ivec4(uvec2(0), dims);
        renderFromTexture(batch, _displayTexture ? _displayTexture : _compositeFramebuffer->getRenderBuffer(0), viewport, viewport);
     });
    swapBuffers();
    _presentRate.increment();
}

void OpenGLDisplayPlugin::present(const std::shared_ptr<RefreshRateController>& refreshRateController) {
    auto frameId = (uint64_t)presentCount();
    PROFILE_RANGE_EX(render, __FUNCTION__, 0xffffff00, frameId)
    uint64_t startPresent = usecTimestampNow();
    refreshRateController->clockStartTime();
    {
        PROFILE_RANGE_EX(render, "updateFrameData", 0xff00ff00, frameId)
        updateFrameData();
    }
    incrementPresentCount();

    if (_currentFrame) {
        auto correction = getViewCorrection();
        getGLBackend()->updatePresentFrame(correction);
        {
            withPresentThreadLock([&] {
                _renderRate.increment();
                if (_currentFrame.get() != _lastFrame) {
                    _newFrameRate.increment();
                }
                _lastFrame = _currentFrame.get();
            });
            // Execute the frame rendering commands
            PROFILE_RANGE_EX(render, "execute", 0xff00ff00, frameId)
            _gpuContext->executeFrame(_currentFrame);
        }

        // Write all layers to a local framebuffer
        {
            PROFILE_RANGE_EX(render, "composite", 0xff00ffff, frameId)
            compositeLayers();
        }

        { // If we have any snapshots this frame, handle them
            PROFILE_RANGE_EX(render, "snapshotOperators", 0xffff00ff, frameId)
            while (!_currentFrame->snapshotOperators.empty()) {
                auto& snapshotOperator = _currentFrame->snapshotOperators.front();
                if (std::get<2>(snapshotOperator)) {
                    std::get<0>(snapshotOperator)(getScreenshot(std::get<1>(snapshotOperator)));
                } else {
                    std::get<0>(snapshotOperator)(getSecondaryCameraScreenshot());
                }
                _currentFrame->snapshotOperators.pop();
            }
        }

        // Take the composite framebuffer and send it to the output device
        refreshRateController->clockEndTime();
        {
            PROFILE_RANGE_EX(render, "internalPresent", 0xff00ffff, frameId)
            internalPresent();
        }

        gpu::Backend::freeGPUMemSize.set(gpu::gl::getFreeDedicatedMemory());
    } else if (alwaysPresent()) {
        refreshRateController->clockEndTime();
        internalPresent();
    } else {
        refreshRateController->clockEndTime();
    }
    _movingAveragePresent.addSample((float)(usecTimestampNow() - startPresent));
}

float OpenGLDisplayPlugin::newFramePresentRate() const {
    return _newFrameRate.rate();
}

float OpenGLDisplayPlugin::droppedFrameRate() const {
    return _droppedFrameRate.rate();
}

float OpenGLDisplayPlugin::presentRate() const {
    return _presentRate.rate();
}

std::function<void(int)> OpenGLDisplayPlugin::getRefreshRateOperator() {
    return [](int targetRefreshRate) {
        auto refreshRateController = DependencyManager::get<PresentThread>()->getRefreshRateController();
        refreshRateController->setRefreshRateLimitPeriod(targetRefreshRate);
    };
}

void OpenGLDisplayPlugin::resetPresentRate() {
    // FIXME
    // _presentRate = RateCounter<100>();
}

float OpenGLDisplayPlugin::renderRate() const {
    return _renderRate.rate();
}

void OpenGLDisplayPlugin::swapBuffers() {
    static auto context = _container->getPrimaryWidget()->context();
    context->swapBuffers();
}

void OpenGLDisplayPlugin::withOtherThreadContext(std::function<void()> f) const {
    static auto presentThread = DependencyManager::get<PresentThread>();
    presentThread->withOtherThreadContext(f);
    if (!OffscreenGLCanvas::restoreThreadContext()) {
        qWarning("Unable to restore original OpenGL context");
    }
}

bool OpenGLDisplayPlugin::setDisplayTexture(const QString& name) {
    // Note: it is the caller's responsibility to keep the network texture in cache.
    if (name.isEmpty()) {
        _displayTexture.reset();
        onDisplayTextureReset();
        return true;
    }
    auto textureCache = DependencyManager::get<TextureCache>();
    auto displayNetworkTexture = textureCache->getTexture(name);
    if (!displayNetworkTexture) {
        return false;
    }
    _displayTexture = displayNetworkTexture->getGPUTexture();
    return !!_displayTexture;
}

QImage OpenGLDisplayPlugin::getScreenshot(float aspectRatio) {
    auto size = _compositeFramebuffer->getSize();
    if (isHmd()) {
        size.x /= 2;
    }
    auto bestSize = size;
    uvec2 corner(0);
    if (aspectRatio != 0.0f) { // Pick out the largest piece of the center that produces the requested width/height aspectRatio
        if (ceil(size.y * aspectRatio) < size.x) {
            bestSize.x = round(size.y * aspectRatio);
        } else {
            bestSize.y = round(size.x / aspectRatio);
        }
        corner.x = round((size.x - bestSize.x) / 2.0f);
        corner.y = round((size.y - bestSize.y) / 2.0f);
    }
    QImage screenshot(bestSize.x, bestSize.y, QImage::Format_ARGB32);
    getGLBackend()->downloadFramebuffer(_compositeFramebuffer, ivec4(corner, bestSize), screenshot);
    return screenshot.mirrored(false, true);
}

QImage OpenGLDisplayPlugin::getSecondaryCameraScreenshot() {
    auto textureCache = DependencyManager::get<TextureCache>();
    auto secondaryCameraFramebuffer = textureCache->getSpectatorCameraFramebuffer();
    gpu::Vec4i region(0, 0, secondaryCameraFramebuffer->getWidth(), secondaryCameraFramebuffer->getHeight());

    QImage screenshot(region.z, region.w, QImage::Format_ARGB32);
    getGLBackend()->downloadFramebuffer(secondaryCameraFramebuffer, region, screenshot);
    return screenshot.mirrored(false, true);
}

glm::uvec2 OpenGLDisplayPlugin::getSurfacePixels() const {
    uvec2 result;
    auto window = _container->getPrimaryWidget();
    if (window) {
        result = toGlm(window->geometry().size() * window->devicePixelRatio());
    }
    return result;
}

glm::uvec2 OpenGLDisplayPlugin::getSurfaceSize() const {
    uvec2 result;
    auto window = _container->getPrimaryWidget();
    if (window) {
        result = toGlm(window->geometry().size());
    }
    return result;
}

void OpenGLDisplayPlugin::assertNotPresentThread() const {
    Q_ASSERT(QThread::currentThread() != _presentThread);
}

void OpenGLDisplayPlugin::assertIsPresentThread() const {
    Q_ASSERT(QThread::currentThread() == _presentThread);
}

bool OpenGLDisplayPlugin::beginFrameRender(uint32_t frameIndex) {
    withNonPresentThreadLock([&] {
        _compositeHUDAlpha = _hudAlpha;
    });
    return Parent::beginFrameRender(frameIndex);
}

gpu::gl::GLBackend* OpenGLDisplayPlugin::getGLBackend() {
    if (!_gpuContext || !_gpuContext->getBackend()) {
        return nullptr;
    }
    auto backend = _gpuContext->getBackend().get();
#if defined(Q_OS_MAC)
    // Should be dynamic_cast, but that doesn't work in plugins on OSX
    auto glbackend = static_cast<gpu::gl::GLBackend*>(backend);
#else
    auto glbackend = dynamic_cast<gpu::gl::GLBackend*>(backend);
#endif

    return glbackend;
}

void OpenGLDisplayPlugin::render(std::function<void(gpu::Batch& batch)> f) {
    gpu::Batch batch;
    f(batch);
    _gpuContext->executeBatch(batch);
}

OpenGLDisplayPlugin::~OpenGLDisplayPlugin() {
}

void OpenGLDisplayPlugin::updateCompositeFramebuffer() {
    auto renderSize = getRecommendedRenderSize();
    if (!_compositeFramebuffer || _compositeFramebuffer->getSize() != renderSize) {
        _compositeFramebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("OpenGLDisplayPlugin::composite", gpu::Element::COLOR_SRGBA_32, renderSize.x, renderSize.y));
    }
}

void OpenGLDisplayPlugin::copyTextureToQuickFramebuffer(NetworkTexturePointer networkTexture, QOpenGLFramebufferObject* target, GLsync* fenceSync) {
#if !defined(USE_GLES)
    auto glBackend = const_cast<OpenGLDisplayPlugin&>(*this).getGLBackend();
    withOtherThreadContext([&] {
        GLuint sourceTexture = glBackend->getTextureID(networkTexture->getGPUTexture());
        GLuint targetTexture = target->texture();
        GLuint fbo[2] {0, 0};

        // need mipmaps for blitting texture
        glGenerateTextureMipmap(sourceTexture);

        // create 2 fbos (one for initial texture, second for scaled one)
        glCreateFramebuffers(2, fbo);

        // setup source fbo
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sourceTexture, 0);

        GLint texWidth = networkTexture->getWidth();
        GLint texHeight = networkTexture->getHeight();

        // setup destination fbo
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTexture, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // maintain aspect ratio, filling the width first if possible.  If that makes the height too
        // much, fill height instead. TODO: only do this when texture changes
        GLint newX = 0;
        GLint newY = 0;
        float aspectRatio = (float)texHeight / (float)texWidth;
        GLint newWidth = target->width();
        GLint newHeight = std::round(aspectRatio * (float)target->width());
        if (newHeight > target->height()) {
            newHeight = target->height();
            newWidth = std::round((float)target->height() / aspectRatio);
            newX = (target->width() - newWidth) / 2;
        } else {
            newY = (target->height() - newHeight) / 2;
        }

        glBlitNamedFramebuffer(fbo[0], fbo[1], 0, 0, texWidth, texHeight, newX, newY, newX + newWidth, newY + newHeight, GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // don't delete the textures!
        glDeleteFramebuffers(2, fbo);
        *fenceSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    });
#endif
}

gpu::PipelinePointer OpenGLDisplayPlugin::getRenderTexturePipeline() {
    return _drawTexturePipeline;
}
