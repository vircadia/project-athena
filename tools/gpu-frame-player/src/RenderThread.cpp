//
//  Created by Bradley Austin Davis on 2018/10/21
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderThread.h"
#include <QtGui/QWindow>
#include <gl/QOpenGLContextWrapper.h>

void RenderThread::submitFrame(const gpu::FramePointer& frame) {
    std::unique_lock<std::mutex> lock(_frameLock);
    _pendingFrames.push(frame);
}

void RenderThread::resize(const QSize& newSize) {
    std::unique_lock<std::mutex> lock(_frameLock);
    _pendingSize.push(newSize);
}

void RenderThread::move(const glm::vec3& v) {
    std::unique_lock<std::mutex> lock(_frameLock);
    _correction = glm::inverse(glm::translate(mat4(), v)) * _correction;
}

void RenderThread::initialize(QWindow* window) {
    std::unique_lock<std::mutex> lock(_frameLock);
    setObjectName("RenderThread");
    Parent::initialize();

    _window = window;
#ifdef USE_GL
    _window->setFormat(getDefaultOpenGLSurfaceFormat());
    _context.setWindow(window);
    _context.create();
    if (!_context.makeCurrent()) {
        qFatal("Unable to make context current");
    }
    QOpenGLContextWrapper(_context.qglContext()).makeCurrent(_window);
    glGenTextures(1, &_externalTexture);
    glBindTexture(GL_TEXTURE_2D, _externalTexture);
    static const glm::u8vec4 color{ 0 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color);
    gl::setSwapInterval(0);
    // GPU library init
    gpu::Context::init<gpu::gl::GLBackend>();
    _context.makeCurrent();
    _gpuContext = std::make_shared<gpu::Context>();
    _backend = _gpuContext->getBackend();
    _context.doneCurrent();
    _context.moveToThread(_thread);
    
    if (!_presentPipeline) {
        gpu::ShaderPointer program = gpu::Shader::createProgram(shader::gpu::program::DrawTexture);
        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        _presentPipeline = gpu::Pipeline::create(program, state);
    }
#else
    auto size = window->size();
    _extent = vk::Extent2D{ (uint32_t)size.width(), (uint32_t)size.height() };

    _context.setValidationEnabled(true);
    _context.requireExtensions({
        std::string{ VK_KHR_SURFACE_EXTENSION_NAME },
        std::string{ VK_KHR_WIN32_SURFACE_EXTENSION_NAME },
    });
    _context.requireDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
    _context.createInstance();
    _surface = _context.instance.createWin32SurfaceKHR({ {}, GetModuleHandle(NULL), (HWND)window->winId() });
    _context.createDevice(_surface);
    _swapchain.setSurface(_surface);
    _swapchain.create(_extent, true);

    setupRenderPass();
    setupFramebuffers();

    acquireComplete = _context.device.createSemaphore(vk::SemaphoreCreateInfo{});
    renderComplete = _context.device.createSemaphore(vk::SemaphoreCreateInfo{});

    // GPU library init
    gpu::Context::init<gpu::vulkan::VKBackend>();
    _gpuContext = std::make_shared<gpu::Context>();
    _backend = _gpuContext->getBackend();
#endif
}

void RenderThread::setup() {
    // Wait until the context has been moved to this thread
    { std::unique_lock<std::mutex> lock(_frameLock); }
    _gpuContext->beginFrame();
    _gpuContext->endFrame();

#ifdef USE_GL
    _context.makeCurrent();
    glViewport(0, 0, 800, 600);
    (void)CHECK_GL_ERROR();
#endif
    _elapsed.start();
}

void RenderThread::shutdown() {
    _activeFrame.reset();
    while (!_pendingFrames.empty()) {
        _gpuContext->consumeFrameUpdates(_pendingFrames.front());
        _pendingFrames.pop();
    }
    _gpuContext->shutdown();
    _gpuContext.reset();
}

#ifndef USE_GL
extern vk::CommandBuffer currentCommandBuffer;
#endif

void RenderThread::renderFrame(gpu::FramePointer& frame) {
    ++_presentCount;
#ifdef USE_GL
    _context.makeCurrent();
#endif
    {
       std::unique_lock<std::mutex> lock(_frameLock);
       if (_correction != glm::mat4()) {
           _backend->updatePresentFrame(_correction);
       }
    }
    _backend->recycle();
    _backend->syncCache();

    auto windowSize = _window->size();

#ifndef USE_GL
    auto windowExtent = vk::Extent2D{ (uint32_t)windowSize.width(), (uint32_t)windowSize.height() };
    if (windowExtent != _extent) {
        return;
    }

    if (_extent != _swapchain.extent) {
        _swapchain.create(_extent);
        setupFramebuffers();
        return;
    }

    static const vk::Offset2D offset;
    static const std::array<vk::ClearValue, 2> clearValues{
        vk::ClearColorValue(std::array<float, 4Ui64>{ { 0.2f, 0.2f, 0.2f, 0.2f } }),
        vk::ClearDepthStencilValue({ 1.0f, 0 }),
    };

    auto swapchainIndex = _swapchain.acquireNextImage(acquireComplete).value;
    auto framebuffer = _framebuffers[swapchainIndex];
    const auto& commandBuffer = currentCommandBuffer = _context.createCommandBuffer();

    auto rect = vk::Rect2D{ offset, _extent };
    vk::RenderPassBeginInfo beginInfo{ _renderPass, framebuffer, rect, (uint32_t)clearValues.size(), clearValues.data() };
    commandBuffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    using namespace vks::debug::marker;
    beginRegion(commandBuffer, "executeFrame", glm::vec4{ 1, 1, 1, 1 });
#endif
    auto& glbackend = (gpu::gl::GLBackend&)(*_backend);
    glm::uvec2 fboSize{ frame->framebuffer->getWidth(), frame->framebuffer->getHeight() };
    auto fbo = glbackend.getFramebufferID(frame->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glClearColor(0, 0, 0, 1);
    glClearDepth(0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //_gpuContext->enableStereo(true);
    if (frame && !frame->batches.empty()) {
        _gpuContext->executeFrame(frame);
    }

#ifdef USE_GL
    static gpu::BatchPointer batch = nullptr;
    if (!batch) {
        batch = std::make_shared<gpu::Batch>();
        batch->setPipeline(_presentPipeline);
        batch->setFramebuffer(nullptr);
        batch->setResourceTexture(0, frame->framebuffer->getRenderBuffer(0));
        batch->setViewportTransform(ivec4(uvec2(0), ivec2(windowSize.width(), windowSize.height())));
        batch->draw(gpu::TRIANGLE_STRIP, 4);
    }
    glDisable(GL_FRAMEBUFFER_SRGB);
    _gpuContext->executeBatch(*batch);
    
    // Keep this raw gl code here for reference
    //glDisable(GL_FRAMEBUFFER_SRGB);
    //glClear(GL_COLOR_BUFFER_BIT);
  /*  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(
        0, 0, fboSize.x, fboSize.y, 
        0, 0, windowSize.width(), windowSize.height(),
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
*/
    (void)CHECK_GL_ERROR();
    _context.swapBuffers();
    _context.doneCurrent();
#else
    endRegion(commandBuffer);
    beginRegion(commandBuffer, "renderpass:testClear", glm::vec4{ 0, 1, 1, 1 });
    commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
    commandBuffer.endRenderPass();
    endRegion(commandBuffer);
    commandBuffer.end();

    static const vk::PipelineStageFlags waitFlags{ vk::PipelineStageFlagBits::eBottomOfPipe };
    vk::SubmitInfo submitInfo{ 1, &acquireComplete, &waitFlags, 1, &commandBuffer, 1, &renderComplete };
    vk::Fence frameFence = _context.device.createFence(vk::FenceCreateInfo{});
    _context.queue.submit(submitInfo, frameFence);
    _swapchain.queuePresent(renderComplete);
    _context.trashCommandBuffers({ commandBuffer });
    _context.emptyDumpster(frameFence);
    _context.recycle();
#endif
}

bool RenderThread::process() {
    std::queue<gpu::FramePointer> pendingFrames;
    std::queue<QSize> pendingSize;

    {
        std::unique_lock<std::mutex> lock(_frameLock);
        pendingFrames.swap(_pendingFrames);
        pendingSize.swap(_pendingSize);
    }
    
    while (!pendingFrames.empty()) {
        _activeFrame = pendingFrames.front();
        pendingFrames.pop();
        _gpuContext->consumeFrameUpdates(_activeFrame);
    }

    while (!pendingSize.empty()) {
#ifndef USE_GL
        const auto& size = pendingSize.front();
        _extent = { (uint32_t)size.width(), (uint32_t)size.height() };
#endif
        pendingSize.pop();
    }

    if (!_activeFrame) {
        QThread::msleep(1);
        return true;
    }

    renderFrame(_activeFrame);
    return true;
}

#ifndef USE_GL

void RenderThread::setupFramebuffers() {
    // Recreate the frame buffers
    _context.trashAll<vk::Framebuffer>(_framebuffers, [this](const std::vector<vk::Framebuffer>& framebuffers) {
        for (const auto& framebuffer : framebuffers) {
            _device.destroy(framebuffer);
        }
    });

    vk::ImageView attachment;
    vk::FramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.renderPass = _renderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = &attachment;
    framebufferCreateInfo.width = _extent.width;
    framebufferCreateInfo.height = _extent.height;
    framebufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    _framebuffers = _swapchain.createFramebuffers(framebufferCreateInfo);
}

void RenderThread::setupRenderPass() {
    if (_renderPass) {
        _device.destroy(_renderPass);
    }

    vk::AttachmentDescription attachment;
    // Color attachment
    attachment.format = _swapchain.colorFormat;
    attachment.loadOp = vk::AttachmentLoadOp::eClear;
    attachment.storeOp = vk::AttachmentStoreOp::eStore;
    attachment.initialLayout = vk::ImageLayout::eUndefined;
    attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    vk::SubpassDependency subpassDependency;
    subpassDependency.srcSubpass = 0;
    subpassDependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &subpassDependency;
    _renderPass = _device.createRenderPass(renderPassInfo);
}
#endif
