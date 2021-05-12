//
//  ModelEntityItem.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ModelEntityItem_h
#define hifi_ModelEntityItem_h

#include "EntityItem.h"
#include <JointData.h>
#include <ThreadSafeValueCache.h>
#include "AnimationPropertyGroup.h"

#include "BlendshapeConstants.h"

class ModelEntityItem : public EntityItem {
public:
    static EntityItemPointer factory(const EntityItemID& entityID, const EntityItemProperties& properties);

    ModelEntityItem(const EntityItemID& entityItemID);

    ALLOW_INSTANTIATION // This class can be instantiated

    // methods for getting/setting all properties of an entity
    virtual EntityItemProperties getProperties(const EntityPropertyFlags& desiredProperties, bool allowEmptyDesiredProperties) const override;
    virtual bool setSubClassProperties(const EntityItemProperties& properties) override;

    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const override;

    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const override;


    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) override;


    virtual void update(const quint64& now) override;
    bool needsToCallUpdate() const override { return isAnimatingSomething(); }

    virtual void debugDump() const override;

    void setShapeType(ShapeType type) override;
    virtual ShapeType getShapeType() const override;

    // TODO: Move these to subclasses, or other appropriate abstraction
    // getters/setters applicable to models and particles
    glm::u8vec3 getColor() const;
    void setColor(const glm::u8vec3& value);

    bool hasModel() const;
    virtual bool hasCompoundShapeURL() const;

    static const QString DEFAULT_MODEL_URL;
    QString getModelURL() const;

    virtual glm::vec3 getScaledDimensions() const override;
    virtual void setScaledDimensions(const glm::vec3& value) override;

    virtual const Transform getTransform(bool& success, int depth = 0) const override;
    virtual const Transform getTransformWithOnlyLocalRotation(bool& success, int depth = 0) const override;
    virtual const Transform getTransform() const override;

    static const QString DEFAULT_COMPOUND_SHAPE_URL;
    QString getCompoundShapeURL() const;

    // model related properties
    virtual void setModelURL(const QString& url);
    virtual void setCompoundShapeURL(const QString& url);

    // Animation related items...
    AnimationPropertyGroup getAnimationProperties() const;
    bool hasAnimation() const;
    QString getAnimationURL() const;
    void setAnimationCurrentFrame(float value);
    float getAnimationCurrentFrame() const;
    bool getAnimationAllowTranslation() const;
    bool isAnimatingSomething() const;

    void setRelayParentJoints(bool relayJoints);
    bool getRelayParentJoints() const;

    void setGroupCulled(bool value);
    bool getGroupCulled() const;

    static const QString DEFAULT_TEXTURES;
    const QString getTextures() const;
    void setTextures(const QString& textures);

    virtual void setJointRotations(const QVector<glm::quat>& rotations);
    virtual void setJointRotationsSet(const QVector<bool>& rotationsSet);
    virtual void setJointTranslations(const QVector<glm::vec3>& translations);
    virtual void setJointTranslationsSet(const QVector<bool>& translationsSet);

    virtual void setAnimationJointsData(const QVector<EntityJointData>& jointsData);

    QVector<glm::quat> getJointRotations() const;
    QVector<bool> getJointRotationsSet() const;
    QVector<glm::vec3> getJointTranslations() const;
    QVector<bool> getJointTranslationsSet() const;

    glm::vec3 getModelScale() const;
    void setModelScale(const glm::vec3& modelScale);

    QString getBlendshapeCoefficients() const;
    void setBlendshapeCoefficients(const QString& blendshapeCoefficients);
    bool blendshapesChanged() const { return _blendshapesChanged; }
    QVector<float> getBlendshapeCoefficientVector();

    bool getUseOriginalPivot() const;
    void setUseOriginalPivot(bool useOriginalPivot);

    void forceReloadModelURL();
    bool shouldForceReloadModelURL();

private:
    void setAnimationSettings(const QString& value); // only called for old bitstream format
    bool applyNewAnimationProperties(AnimationPropertyGroup newProperties);

protected:
    void resizeJointArrays(int newSize);

    // these are used:
    // - to bounce joint data from an animation into the model/rig.
    // - to relay changes from scripts to model/rig.
    // - to relay between network and model/rig
    // they aren't currently updated from data in the model/rig, and they don't have a direct effect
    // on what's rendered.
    ReadWriteLockable _jointDataLock;

    bool _jointRotationsExplicitlySet { false }; // were the joints set as a property or just side effect of animations
    bool _jointTranslationsExplicitlySet{ false }; // were the joints set as a property or just side effect of animations

    struct ModelJointData {
        EntityJointData joint;
        bool rotationDirty { false };
        bool translationDirty { false };
    };

    QVector<ModelJointData> _localJointData;
    int _lastKnownCurrentFrame{-1};

    glm::u8vec3 _color;
    glm::vec3 _modelScale { 1.0f };
    QString _modelURL;
    bool _relayParentJoints;
    bool _groupCulled { false };
    QVariantMap _blendshapeCoefficientsMap;
    bool _useOriginalPivot { false };
    bool _forceReloadModelURL { false };

    ThreadSafeValueCache<QString> _compoundShapeURL;

    AnimationPropertyGroup _animationProperties;

    QString _textures;

    ShapeType _shapeType { SHAPE_TYPE_NONE };

private:
    uint64_t _lastAnimated{ 0 };
    float _currentFrame{ -1.0f };

    QVector<float> _blendshapeCoefficientsVector;
    bool _blendshapesChanged { false };
};

#endif // hifi_ModelEntityItem_h
