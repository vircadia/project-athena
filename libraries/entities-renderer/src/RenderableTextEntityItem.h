//
//  RenderableTextEntityItem.h
//  interface/src/entities
//
//  Created by Brad Hefta-Gaub on 8/6/14.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderableTextEntityItem_h
#define hifi_RenderableTextEntityItem_h

#include "RenderableEntityItem.h"

class TextEntityItem;
class TextRenderer3D;

namespace render { namespace entities {

class TextPayload;

class TextEntityRenderer : public TypedEntityRenderer<TextEntityItem> {
    using Parent = TypedEntityRenderer<TextEntityItem>;
    using Pointer = std::shared_ptr<TextEntityRenderer>;
public:
    TextEntityRenderer(const EntityItemPointer& entity);
    ~TextEntityRenderer();

    QSizeF textSize(const QString& text) const;

protected:
    bool isTransparent() const override;
    bool isTextTransparent() const;
    ShapeKey getShapeKey() override;
    ItemKey getKey() override;
    virtual uint32_t metaFetchMetaSubItems(ItemIDs& subItems) const override;

    void onAddToSceneTyped(const TypedEntityPointer& entity) override;
    void onRemoveFromSceneTyped(const TypedEntityPointer& entity) override;

private:
    virtual void doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) override;
    virtual void doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) override;
    virtual void doRender(RenderArgs* args) override;

    std::shared_ptr<TextRenderer3D> _textRenderer;

    PulsePropertyGroup _pulseProperties;

    QString _text;
    float _lineHeight;
    glm::vec3 _textColor;
    float _textAlpha;
    glm::vec3 _backgroundColor;
    float _backgroundAlpha;
    bool _unlit;

    float _leftMargin;
    float _rightMargin;
    float _topMargin;
    float _bottomMargin;

    glm::vec3 _dimensions;

    QString _font { "" };
    TextEffect _effect { TextEffect::NO_EFFECT };
    glm::vec3 _effectColor { 0 };
    float _effectThickness { 0.0f };

    int _geometryID { 0 };

    std::shared_ptr<TextPayload> _textPayload;
    render::ItemID _textRenderID;
    void updateTextRenderItem() const;

    friend class render::entities::TextPayload;
};

class TextPayload {
public:
    TextPayload() = default;
    TextPayload(const QUuid& entityID, const std::weak_ptr<TextRenderer3D>& textRenderer);
    ~TextPayload();

    typedef render::Payload<TextPayload> Payload;
    typedef Payload::DataPointer Pointer;

    ItemKey getKey() const;
    Item::Bound getBound(RenderArgs* args) const;
    ShapeKey getShapeKey() const;
    void render(RenderArgs* args);
    bool passesZoneOcclusionTest(const std::unordered_set<QUuid>& containingZones) const;

protected:
    QUuid _entityID;
    std::weak_ptr<TextRenderer3D> _textRenderer;
    Transform _prevRenderTransform;

    int _geometryID { 0 };
};

} }

namespace render {
    template <> const ItemKey payloadGetKey(const entities::TextPayload::Pointer& payload);
    template <> const Item::Bound payloadGetBound(const entities::TextPayload::Pointer& payload, RenderArgs* args);
    template <> const ShapeKey shapeGetShapeKey(const entities::TextPayload::Pointer& payload);
    template <> void payloadRender(const entities::TextPayload::Pointer& payload, RenderArgs* args);
    template <> bool payloadPassesZoneOcclusionTest(const entities::TextPayload::Pointer& payload, const std::unordered_set<QUuid>& containingZones);
}

#endif // hifi_RenderableTextEntityItem_h
