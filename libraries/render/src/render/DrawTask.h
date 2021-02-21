//
//  DrawTask.h
//  render/src/render
//
//  Created by Sam Gateau on 5/21/15.
//  Copyright 20154 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_DrawTask_h
#define hifi_render_DrawTask_h

#include "Engine.h"

namespace render {

void renderItems(const RenderContextPointer& renderContext, const ItemBounds& inItems, int maxDrawnItems = -1);
void renderShapes(const RenderContextPointer& renderContext, const ShapePlumberPointer& shapeContext, const ItemBounds& inItems, int maxDrawnItems = -1, const ShapeKey& globalKey = ShapeKey());
void renderStateSortShapes(const RenderContextPointer& renderContext, const ShapePlumberPointer& shapeContext, const ItemBounds& inItems, int maxDrawnItems = -1, const ShapeKey& globalKey = ShapeKey());

class DrawLightConfig : public Job::Config {
    Q_OBJECT
    Q_PROPERTY(int numDrawn READ getNumDrawn NOTIFY numDrawnChanged)
    Q_PROPERTY(int maxDrawn MEMBER maxDrawn NOTIFY dirty)
public:
    int getNumDrawn() { return numDrawn; }
    void setNumDrawn(int num) { numDrawn = num; emit numDrawnChanged(); }

    int maxDrawn{ -1 };
signals:
    void numDrawnChanged();
    void dirty();

protected:
    int numDrawn{ 0 };
};

class DrawLight {
public:
    using Config = DrawLightConfig;
    using JobModel = Job::ModelI<DrawLight, ItemBounds, Config>;

    void configure(const Config& config) { _maxDrawn = config.maxDrawn; }
    void run(const RenderContextPointer& renderContext, const ItemBounds& inLights);
protected:
    int _maxDrawn; // initialized by Config
};

class DrawBounds {
public:
    class Config : public render::JobConfig {
    public:
        Config(bool enabled = false) : JobConfig(enabled) {}
    };

    using Inputs = render::ItemBounds;
    using JobModel = render::Job::ModelI<DrawBounds, Inputs, Config>;

    DrawBounds(uint transformSlot) : _transformSlot(transformSlot) {}

    void configure(const Config& configuration) {}
    void run(const render::RenderContextPointer& renderContext,
        const Inputs& items);

private:
    const gpu::PipelinePointer getPipeline();
    gpu::PipelinePointer _boundsPipeline;
    gpu::BufferPointer _drawBuffer;
    gpu::BufferPointer _paramsBuffer;
    uint _transformSlot;
};

class DrawQuadVolumeConfig : public render::JobConfig {
    Q_OBJECT
        Q_PROPERTY(bool isFrozen MEMBER isFrozen NOTIFY dirty)
public:

    DrawQuadVolumeConfig(bool enabled = false) : JobConfig(enabled) {}

    bool isFrozen{ false };
signals:
    void dirty();

};

class DrawQuadVolume {
public:

    using Config = DrawQuadVolumeConfig;

    void configure(const Config& configuration);

protected:
    DrawQuadVolume(const glm::vec3& color);

    void run(const render::RenderContextPointer& renderContext, const glm::vec3 vertices[8],
             const gpu::BufferView& indices, int indexCount);

    gpu::BufferView _meshVertices;
    gpu::BufferPointer _params;
    bool _isUpdateEnabled{ true };

    static gpu::Stream::FormatPointer _format;

    static gpu::PipelinePointer getPipeline();
};

class DrawAABox : public DrawQuadVolume {
public:
    using Inputs = AABox;
    using JobModel = render::Job::ModelI<DrawAABox, Inputs, Config>;

    DrawAABox(const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f));

    void run(const render::RenderContextPointer& renderContext, const Inputs& box);

protected:

    static gpu::BufferView _cubeMeshIndices;

    static void getVertices(const AABox& box, glm::vec3 vertices[8]);
};

class DrawFrustum : public DrawQuadVolume {
public:
    using Input = ViewFrustumPointer;
    using JobModel = render::Job::ModelI<DrawFrustum, Input, Config>;

    DrawFrustum(const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f));

    void run(const render::RenderContextPointer& renderContext, const Input& input);

private:

    static gpu::BufferView _frustumMeshIndices;

    static void getVertices(const ViewFrustum& frustum, glm::vec3 vertices[8]);
};
}

#endif // hifi_render_DrawTask_h
