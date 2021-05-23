//
//  AnimationPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimationPropertyGroup.h"

#include <QJsonDocument>
#include <OctreePacketData.h>


#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

const float AnimationPropertyGroup::MAXIMUM_POSSIBLE_FRAME = 100000.0f;

bool operator==(const AnimationPropertyGroup& a, const AnimationPropertyGroup& b) {
    return
        (a._currentFrame == b._currentFrame) &&
        (a._running == b._running) &&
        (a._loop == b._loop) &&
        (a._hold == b._hold) &&
        (a._firstFrame == b._firstFrame) &&
        (a._lastFrame == b._lastFrame) &&
        (a._fps == b._fps) &&
        (a._allowTranslation == b._allowTranslation) &&
        (a._url == b._url);
}

bool operator!=(const AnimationPropertyGroup& a, const AnimationPropertyGroup& b) {
    return
        (a._currentFrame != b._currentFrame) ||
        (a._running != b._running) ||
        (a._loop != b._loop) ||
        (a._hold != b._hold) ||
        (a._firstFrame != b._firstFrame) ||
        (a._lastFrame != b._lastFrame) ||
        (a._fps != b._fps) ||
        (a._allowTranslation != b._allowTranslation) ||
        (a._url != b._url);
}


/*@jsdoc
 * An animation is configured by the following properties:
 * @typedef {object} Entities.AnimationProperties
 * @property {string} url="" - The URL of the glTF or FBX file that has the animation. glTF files may be in JSON or binary 
 *     format (".gltf" or ".glb" URLs respectively).
 *     <p><strong>Warning:</strong> glTF animations currently do not always animate correctly.</p>
 * @property {boolean} allowTranslation=true - <code>true</code> to enable translations contained in the animation to be
 *     played, <code>false</code> to disable translations.
 * @property {number} fps=30 - The speed in frames/s that the animation is played at.
 * @property {number} firstFrame=0 - The first frame to play in the animation.
 * @property {number} lastFrame=100000 - The last frame to play in the animation.
 * @property {number} currentFrame=0 - The current frame being played in the animation.
 * @property {boolean} running=false - <code>true</code> if the animation should play, <code>false</code> if it shouldn't.
 * @property {boolean} loop=true - <code>true</code> if the animation is continuously repeated in a loop, <code>false</code> if 
 *     it isn't.
 * @property {boolean} hold=false - <code>true</code> if the rotations and translations of the last frame played are 
 *     maintained when the animation stops playing, <code>false</code> if they aren't.
 */
void AnimationPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_URL, Animation, animation, URL, url);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_ALLOW_TRANSLATION, Animation, animation, AllowTranslation, allowTranslation);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_FPS, Animation, animation, FPS, fps);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_FRAME_INDEX, Animation, animation, CurrentFrame, currentFrame);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_PLAYING, Animation, animation, Running, running);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_LOOP, Animation, animation, Loop, loop);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_FIRST_FRAME, Animation, animation, FirstFrame, firstFrame);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_LAST_FRAME, Animation, animation, LastFrame, lastFrame);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_ANIMATION_HOLD, Animation, animation, Hold, hold);
}


void AnimationPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, url, QString, setURL);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, allowTranslation, bool, setAllowTranslation);

    // legacy property support
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(animationURL, QString, setURL, getURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_NOCHECK(animationSettings, QString, setFromOldAnimationSettings);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, fps, float, setFPS);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, currentFrame, float, setCurrentFrame);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, running, bool, setRunning);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, loop, bool, setLoop);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, firstFrame, float, setFirstFrame);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, lastFrame, float, setLastFrame);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(animation, hold, bool, setHold);

    // legacy property support
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(animationFPS, float, setFPS, getFPS);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(animationIsPlaying, bool, setRunning, getRunning);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(animationFrameIndex, float, setCurrentFrame, getCurrentFrame);
}

void AnimationPropertyGroup::merge(const AnimationPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(url);
    COPY_PROPERTY_IF_CHANGED(allowTranslation);
    COPY_PROPERTY_IF_CHANGED(fps);
    COPY_PROPERTY_IF_CHANGED(currentFrame);
    COPY_PROPERTY_IF_CHANGED(running);
    COPY_PROPERTY_IF_CHANGED(loop);
    COPY_PROPERTY_IF_CHANGED(firstFrame);
    COPY_PROPERTY_IF_CHANGED(lastFrame);
    COPY_PROPERTY_IF_CHANGED(hold);
}

void AnimationPropertyGroup::setFromOldAnimationSettings(const QString& value) {
    // the animations setting is a JSON string that may contain various animation settings.
    // if it includes fps, currentFrame, or running, those values will be parsed out and
    // will over ride the regular animation settings

    float fps = getFPS();
    float currentFrame = getCurrentFrame();
    bool running = getRunning();
    float firstFrame = getFirstFrame();
    float lastFrame = getLastFrame();
    bool loop = getLoop();
    bool hold = getHold();
    bool allowTranslation = getAllowTranslation();

    QJsonDocument settingsAsJson = QJsonDocument::fromJson(value.toUtf8());
    QJsonObject settingsAsJsonObject = settingsAsJson.object();
    QVariantMap settingsMap = settingsAsJsonObject.toVariantMap();

    if (settingsMap.contains("fps")) {
        fps = settingsMap["fps"].toFloat();
    }

    // old settings had frameIndex
    if (settingsMap.contains("frameIndex")) {
        currentFrame = settingsMap["frameIndex"].toFloat();
    }

    if (settingsMap.contains("running")) {
        running = settingsMap["running"].toBool();
    }

    if (settingsMap.contains("firstFrame")) {
        firstFrame = settingsMap["firstFrame"].toFloat();
    }

    if (settingsMap.contains("lastFrame")) {
        lastFrame = settingsMap["lastFrame"].toFloat();
    }

    if (settingsMap.contains("loop")) {
        running = settingsMap["loop"].toBool();
    }

    if (settingsMap.contains("hold")) {
        running = settingsMap["hold"].toBool();
    }

    if (settingsMap.contains("allowTranslation")) {
        allowTranslation = settingsMap["allowTranslation"].toBool();
    }


    setAllowTranslation(allowTranslation);
    setFPS(fps);
    setCurrentFrame(currentFrame);
    setRunning(running);
    setFirstFrame(firstFrame);
    setLastFrame(lastFrame);
    setLoop(loop);
    setHold(hold);
}


void AnimationPropertyGroup::debugDump() const {
    qCDebug(entities) << "   AnimationPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "       fps:" << getFPS() << " has changed:" << fpsChanged();
    qCDebug(entities) << "currentFrame:" << getCurrentFrame() << " has changed:" << currentFrameChanged();
    qCDebug(entities) << "allowTranslation:" << getAllowTranslation() << " has changed:" << allowTranslationChanged(); 
}

void AnimationPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (urlChanged()) {
        out << "animation-url";
    }
    if (allowTranslationChanged()) {
        out << "animation-allowTranslation";
    }
    if (fpsChanged()) {
        out << "animation-fps";
    }
    if (currentFrameChanged()) {
        out << "animation-currentFrame";
    }
    if (runningChanged()) {
        out << "animation-running";
    }
    if (loopChanged()) {
        out << "animation-loop";
    }
    if (firstFrameChanged()) {
        out << "animation-firstFrame";
    }
    if (lastFrameChanged()) {
        out << "animation-lastFrame";
    }
    if (holdChanged()) {
        out << "animation-hold";
    }
}


bool AnimationPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_URL, getURL());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_ALLOW_TRANSLATION, getAllowTranslation());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FPS, getFPS());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FRAME_INDEX, getCurrentFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_PLAYING, getRunning());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_LOOP, getLoop());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FIRST_FRAME, getFirstFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_LAST_FRAME, getLastFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_HOLD, getHold());

    return true;
}


bool AnimationPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) {
    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_ANIMATION_URL, QString, setURL);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_ALLOW_TRANSLATION, bool, setAllowTranslation);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FPS, float, setFPS);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FRAME_INDEX, float, setCurrentFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_PLAYING, bool, setRunning);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_LOOP, bool, setLoop);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FIRST_FRAME, float, setFirstFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_LAST_FRAME, float, setLastFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_HOLD, bool, setHold);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_URL, URL);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_ALLOW_TRANSLATION, AllowTranslation);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_FPS, FPS);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_FRAME_INDEX, CurrentFrame);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_PLAYING, Running);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_LOOP, Loop);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_FIRST_FRAME, FirstFrame);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_LAST_FRAME, LastFrame);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_ANIMATION_HOLD, Hold);
    
    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void AnimationPropertyGroup::markAllChanged() {
    _urlChanged = true;
    _allowTranslationChanged = true;
    _fpsChanged = true;
    _currentFrameChanged = true;
    _runningChanged = true;
    _loopChanged = true;
    _firstFrameChanged = true;
    _lastFrameChanged = true;
    _holdChanged = true;
}

EntityPropertyFlags AnimationPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_URL, url);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_ALLOW_TRANSLATION, allowTranslation);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_FPS, fps);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_FRAME_INDEX, currentFrame);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_PLAYING, running);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_LOOP, loop);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_FIRST_FRAME, firstFrame);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_LAST_FRAME, lastFrame);
    CHECK_PROPERTY_CHANGE(PROP_ANIMATION_HOLD, hold);

    return changedProperties;
}

void AnimationPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, URL, getURL);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, AllowTranslation, getAllowTranslation);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, FPS, getFPS);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, CurrentFrame, getCurrentFrame);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, Running, getRunning);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, Loop, getLoop);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, FirstFrame, getFirstFrame);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, LastFrame, getLastFrame);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Animation, Hold, getHold);
}

bool AnimationPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, URL, url, setURL);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, AllowTranslation, allowTranslation, setAllowTranslation);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, FPS, fps, setFPS);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, CurrentFrame, currentFrame, setCurrentFrame);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, Running, running, setRunning);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, Loop, loop, setLoop);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, FirstFrame, firstFrame, setFirstFrame);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, LastFrame, lastFrame, setLastFrame);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Animation, Hold, hold, setHold);
    return somethingChanged;
}

EntityPropertyFlags AnimationPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_ANIMATION_URL;
    requestedProperties += PROP_ANIMATION_ALLOW_TRANSLATION;
    requestedProperties += PROP_ANIMATION_FPS;
    requestedProperties += PROP_ANIMATION_FRAME_INDEX;
    requestedProperties += PROP_ANIMATION_PLAYING;
    requestedProperties += PROP_ANIMATION_LOOP;
    requestedProperties += PROP_ANIMATION_FIRST_FRAME;
    requestedProperties += PROP_ANIMATION_LAST_FRAME;
    requestedProperties += PROP_ANIMATION_HOLD;

    return requestedProperties;
}

void AnimationPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                EntityPropertyFlags& requestedProperties,
                                EntityPropertyFlags& propertyFlags,
                                EntityPropertyFlags& propertiesDidntFit,
                                int& propertyCount,
                                OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_URL, getURL());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_ALLOW_TRANSLATION, getAllowTranslation());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FPS, getFPS());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FRAME_INDEX, getCurrentFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_PLAYING, getRunning());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_LOOP, getLoop());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_FIRST_FRAME, getFirstFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_LAST_FRAME, getLastFrame());
    APPEND_ENTITY_PROPERTY(PROP_ANIMATION_HOLD, getHold());
}

int AnimationPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                            ReadBitstreamToTreeParams& args,
                                            EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                            bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_ANIMATION_URL, QString, setURL);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_ALLOW_TRANSLATION, bool, setAllowTranslation);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FPS, float, setFPS);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FRAME_INDEX, float, setCurrentFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_PLAYING, bool, setRunning);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_LOOP, bool, setLoop);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_FIRST_FRAME, float, setFirstFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_LAST_FRAME, float, setLastFrame);
    READ_ENTITY_PROPERTY(PROP_ANIMATION_HOLD, bool, setHold);
    return bytesRead;
}

float AnimationPropertyGroup::getNumFrames() const {
    return _lastFrame - _firstFrame + 1.0f;
}

float AnimationPropertyGroup::computeLoopedFrame(float frame) const {
    float numFrames = getNumFrames();
    if (numFrames > 1.0f) {
        frame = getFirstFrame() + fmodf(frame - getFirstFrame(), numFrames);
    } else {
        frame = getFirstFrame();
    }
    return frame;
}

bool AnimationPropertyGroup::isValidAndRunning() const {
    return getRunning() && (getFPS() > 0.0f) && (getNumFrames() > 1.0f) && !(getURL().isEmpty());
}
