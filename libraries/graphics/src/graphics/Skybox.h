//
//  Skybox.h
//  libraries/graphics/src/graphics
//
//  Created by Sam Gateau on 5/4/2015.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_model_Skybox_h
#define hifi_model_Skybox_h

#include <gpu/Texture.h>

#include "Light.h"

class ViewFrustum;

namespace gpu { class Batch; }

namespace graphics {

typedef glm::vec3 Color;

class Skybox {
public:
    typedef gpu::BufferView UniformBufferView;

    Skybox();
    Skybox& operator= (const Skybox& skybox);
    virtual ~Skybox() {};
 
    void setColor(const Color& color);
    const Color getColor() const { return _schemaBuffer.get<Schema>().color; }

    void setCubemap(const gpu::TexturePointer& cubemap);
    const gpu::TexturePointer& getCubemap() const { return _cubemap; }

    void setOrientation(const glm::quat& orientation);
    const glm::quat getOrientation() const { return _orientation; }

    virtual bool empty() { return _empty; }
    virtual void clear();

    void prepare(gpu::Batch& batch) const;
    virtual void render(gpu::Batch& batch, const ViewFrustum& frustum, bool forward, uint transformSlot) const;

    static void render(gpu::Batch& batch, const ViewFrustum& frustum, const Skybox& skybox, bool forward, uint transformSlot);

    const UniformBufferView& getSchemaBuffer() const { return _schemaBuffer; }

protected:
    class Schema {
    public:
        glm::vec3 color { 0.0f, 0.0f, 0.0f };
        float blend { 0.0f };
    };

    void updateSchemaBuffer() const;

    mutable gpu::BufferView _schemaBuffer;
    gpu::TexturePointer _cubemap;
    glm::quat _orientation;

    bool _empty{ true };
};
typedef std::shared_ptr<Skybox> SkyboxPointer;

};

#endif //hifi_model_Skybox_h
