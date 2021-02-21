//
//  Created by Sam Gondelman 7/17/2018
//  Copyright 2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_ParabolaPointer_h
#define hifi_ParabolaPointer_h

#include "PathPointer.h"

#include <render/Item.h>

class ParabolaPointer : public PathPointer {
    using Parent = PathPointer;
public:
    class RenderState : public StartEndRenderState {
    public:
        class ParabolaRenderItem {
        public:
            using Payload = render::Payload<ParabolaRenderItem>;
            using Pointer = Payload::DataPointer;

            ParabolaRenderItem(const glm::vec3& color, float alpha, float width,
                               bool isVisibleInSecondaryCamera, bool drawInFront, bool enabled);
            ~ParabolaRenderItem() {}

            static std::map<std::pair<bool, bool>, gpu::PipelinePointer> _parabolaPipelines;
            gpu::PipelinePointer getParabolaPipeline(bool forward) const;

            void render(RenderArgs* args);
            render::Item::Bound& editBound() { return _bound; }
            const render::Item::Bound& getBound(RenderArgs* args) { return _bound; }
            render::ItemKey getKey() const { return _key; }

            void setVisible(bool visible);
            void updateKey();
            void updateUniformBuffer() { _uniformBuffer->setSubData(0, _parabolaData); }
            void updateBounds();

            void setColor(const glm::vec3& color) { _parabolaData.color = glm::vec4(color, _parabolaData.color.a); }
            void setAlpha(const float& alpha) { _parabolaData.color.a = alpha; }
            void setWidth(const float& width) { _parabolaData.width = width; }
            void setParabolicDistance(const float& parabolicDistance) { _parabolaData.parabolicDistance = parabolicDistance; }
            void setVelocity(const glm::vec3& velocity) { _parabolaData.velocity = velocity; }
            void setAcceleration(const glm::vec3& acceleration) { _parabolaData.acceleration = acceleration; }
            void setOrigin(const glm::vec3& origin) { _origin = origin; }
            void setIsVisibleInSecondaryCamera(const bool& isVisibleInSecondaryCamera) { _isVisibleInSecondaryCamera = isVisibleInSecondaryCamera; }
            void setDrawInFront(const bool& drawInFront) { _drawInFront = drawInFront; }
            void setEnabled(const bool& enabled) { _enabled = enabled; }

            static const glm::vec4 DEFAULT_PARABOLA_COLOR;
            static const float DEFAULT_PARABOLA_WIDTH;
            static const bool DEFAULT_PARABOLA_ISVISIBLEINSECONDARYCAMERA;
            static const bool DEFAULT_PARABOLA_DRAWINFRONT;

        private:
            render::Item::Bound _bound;
            render::ItemKey _key;

            glm::vec3 _origin { 0.0f };
            Transform _prevRenderTransform;
            bool _isVisibleInSecondaryCamera { DEFAULT_PARABOLA_ISVISIBLEINSECONDARYCAMERA };
            bool _drawInFront { DEFAULT_PARABOLA_DRAWINFRONT };
            bool _visible { false };
            bool _enabled { false };

            struct ParabolaData {
                glm::vec3 velocity { 0.0f };
                float parabolicDistance { 0.0f };
                vec3 acceleration { 0.0f };
                float width { DEFAULT_PARABOLA_WIDTH };
                vec4 color { vec4(DEFAULT_PARABOLA_COLOR)};
                int numSections { 0 };
                ivec3 spare;
            };

            ParabolaData _parabolaData;
            gpu::BufferPointer _uniformBuffer { std::make_shared<gpu::Buffer>() };
        };

        RenderState() {}
        RenderState(const QUuid& startID, const QUuid& endID, const glm::vec3& pathColor, float pathAlpha, float parentScale,
                    bool isVisibleInSecondaryCamera, bool drawInFront, bool pathEnabled);

        void setPathWidth(float width) { _pathWidth = width; }
        float getPathWidth() const { return _pathWidth; }

        void cleanup() override;
        void disable() override;
        void update(const glm::vec3& origin, const glm::vec3& end, const glm::vec3& surfaceNormal, float parentScale, bool distanceScaleEnd, bool centerEndY,
                    bool faceAvatar, bool followNormal, float followNormalStrength, float distance, const PickResultPointer& pickResult) override;

        void editParabola(const glm::vec3& color, float alpha, float width, bool isVisibleInSecondaryCamera, bool drawInFront, bool enabled);

    private:
        int _pathID;
        float _pathWidth;
    };

    ParabolaPointer(const QVariant& rayProps, const RenderStateMap& renderStates, const DefaultRenderStateMap& defaultRenderStates, bool hover, const PointerTriggers& triggers,
        bool faceAvatar, bool followNormal, float followNormalStrength, bool centerEndY, bool lockEnd, bool distanceScaleEnd, bool scaleWithAvatar, bool enabled);

    PickQuery::PickType getType() const override;

    QVariantMap toVariantMap() const override;

    static std::shared_ptr<StartEndRenderState> buildRenderState(const QVariantMap& propMap);

protected:
    virtual PickResultPointer getPickResultCopy(const PickResultPointer& pickResult) const override;

    void editRenderStatePath(const std::string& state, const QVariant& pathProps) override;

    glm::vec3 getPickOrigin(const PickResultPointer& pickResult) const override;
    glm::vec3 getPickEnd(const PickResultPointer& pickResult, float distance) const override;
    glm::vec3 getPickedObjectNormal(const PickResultPointer& pickResult) const override;
    IntersectionType getPickedObjectType(const PickResultPointer& pickResult) const override;
    QUuid getPickedObjectID(const PickResultPointer& pickResult) const override;
    void setVisualPickResultInternal(PickResultPointer pickResult, IntersectionType type, const QUuid& id,
                                     const glm::vec3& intersection, float distance, const glm::vec3& surfaceNormal) override;

    PointerEvent buildPointerEvent(const PickedObject& target, const PickResultPointer& pickResult, const std::string& button = "", bool hover = true) override;

private:
    static glm::vec3 findIntersection(const PickedObject& pickedObject, const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration);
};

namespace render {
    template <> const ItemKey payloadGetKey(const ParabolaPointer::RenderState::ParabolaRenderItem::Pointer& payload);
    template <> const Item::Bound payloadGetBound(const ParabolaPointer::RenderState::ParabolaRenderItem::Pointer& payload, RenderArgs* args);
    template <> void payloadRender(const ParabolaPointer::RenderState::ParabolaRenderItem::Pointer& payload, RenderArgs* args);
    template <> const ShapeKey shapeGetShapeKey(const ParabolaPointer::RenderState::ParabolaRenderItem::Pointer& payload);
}

#endif // hifi_ParabolaPointer_h
