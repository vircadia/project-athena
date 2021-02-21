//
//  DeferredFramebuffer.h
//  libraries/render-utils/src/
//
//  Created by Sam Gateau 7/11/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DeferredFramebuffer_h
#define hifi_DeferredFramebuffer_h

#include "gpu/Resource.h"
#include "gpu/Framebuffer.h"

// DeferredFramebuffer is  a helper class gathering in one place the GBuffer (Framebuffer) and lighting framebuffer
class DeferredFramebuffer {
public:

    DeferredFramebuffer();

    gpu::FramebufferPointer getDeferredFramebuffer();
    gpu::FramebufferPointer getDeferredFramebufferDepthColor();

    gpu::TexturePointer getDeferredColorTexture();
    gpu::TexturePointer getDeferredNormalTexture();
    gpu::TexturePointer getDeferredSpecularTexture();
    gpu::TexturePointer getDeferredVelocityTexture();

    gpu::FramebufferPointer getLightingFramebuffer();
    gpu::FramebufferPointer getLightingWithVelocityFramebuffer();
    gpu::TexturePointer getLightingTexture();

    // Update the depth buffer which will drive the allocation of all the other resources according to its size.
    void updatePrimaryDepth(const gpu::TexturePointer& depthBuffer);
    gpu::TexturePointer getPrimaryDepthTexture();
    const glm::ivec2& getFrameSize() const { return _frameSize; }

protected:
    void allocate();

    gpu::TexturePointer _primaryDepthTexture;

    gpu::FramebufferPointer _deferredFramebuffer;
    gpu::FramebufferPointer _deferredFramebufferDepthColor;

    gpu::TexturePointer _deferredColorTexture;
    gpu::TexturePointer _deferredNormalTexture;
    gpu::TexturePointer _deferredSpecularTexture;
    gpu::TexturePointer _deferredVelocityTexture;

    gpu::TexturePointer _lightingTexture;
    gpu::FramebufferPointer _lightingFramebuffer;
    gpu::FramebufferPointer _lightingWithVelocityFramebuffer;

    glm::ivec2 _frameSize;
};

using DeferredFramebufferPointer = std::shared_ptr<DeferredFramebuffer>;

#endif // hifi_DeferredFramebuffer_h
