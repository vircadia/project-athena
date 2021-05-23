//
//  AnimationObject.h
//  libraries/animation/src/
//
//  Created by Andrzej Kapolka on 4/17/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimationObject_h
#define hifi_AnimationObject_h

#include <QObject>

#include <FBXSerializer.h>
#include "Scriptable.h"

class ScriptEngine;

/*@jsdoc
 * Information about an animation resource, created by {@link AnimationCache.getAnimation}.
 *
 * @class AnimationObject
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {string[]} jointNames - The names of the joints that are animated. <em>Read-only.</em>
 * @property {AnimationFrameObject[]} frames - The frames in the animation. <em>Read-only.</em>
 */
/// Scriptable wrapper for animation pointers.
class AnimationObject : public QObject, protected Scriptable {
    Q_OBJECT
    Q_PROPERTY(QStringList jointNames READ getJointNames)
    Q_PROPERTY(QVector<HFMAnimationFrame> frames READ getFrames)

public:
    
    /*@jsdoc
     * Gets the names of the joints that are animated.
     * @function AnimationObject.getJointNames
     * @returns {string[]} The names of the joints that are animated.
     */
    Q_INVOKABLE QStringList getJointNames() const;
    
    /*@jsdoc
     * Gets the frames in the animation.
     * @function AnimationObject.getFrames
     * @returns {AnimationFrameObject[]} The frames in the animation.
     */
    Q_INVOKABLE QVector<HFMAnimationFrame> getFrames() const;
};

/*@jsdoc
 * Joint rotations in one frame of an {@link AnimationObject}.
 *
 * @class AnimationFrameObject
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {Quat[]} rotations - Joint rotations. <em>Read-only.</em>
 */
/// Scriptable wrapper for animation frames.
class AnimationFrameObject : public QObject, protected Scriptable {
    Q_OBJECT
    Q_PROPERTY(QVector<glm::quat> rotations READ getRotations)

public:
    
    /*@jsdoc
     * Gets the joint rotations in the animation frame.
     * @function AnimationFrameObject.getRotations
     * @returns {Quat[]} The joint rotations in the animation frame.
     */
    Q_INVOKABLE QVector<glm::quat> getRotations() const;
};

void registerAnimationTypes(ScriptEngine* engine);

#endif // hifi_AnimationObject_h
