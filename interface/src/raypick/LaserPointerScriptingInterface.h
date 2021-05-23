//
//  LaserPointerScriptingInterface.h
//  interface/src/raypick
//
//  Created by Sam Gondelman 7/11/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_LaserPointerScriptingInterface_h
#define hifi_LaserPointerScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include "DependencyManager.h"
#include <PointerManager.h>

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class LaserPointerScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

/*@jsdoc
 * The <code>LaserPointers</code> API is a subset of the {@link Pointers} API. It lets you create, manage, and visually 
 * represent objects for repeatedly calculating ray intersections with avatars, entities, and overlays. Ray pointers can also 
 * be configured to generate events on entities and overlays intersected.
 *
 * @namespace LaserPointers
 *
 * @deprecated This API is deprecated and will be removed. Use {@link Pointers} instead.
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @borrows Pointers.enablePointer as enableLaserPointer
 * @borrows Pointers.disablePointer as disableLaserPointer
 * @borrows Pointers.removePointer as removeLaserPointer
 * @borrows Pointers.setPrecisionPicking as setPrecisionPicking
 */
public:

    /*@jsdoc
     * Creates a new ray pointer. The pointer can have a wide range of behaviors depending on the properties specified. For
     * example, it may be a static ray pointer, a mouse ray pointer, or joint ray pointer.
     * <p><strong>Warning:</strong> Pointers created using this method currently always intersect at least visible and
     * collidable things but this may not always be the case.</p>
     * @function LaserPointers.createLaserPointer
     * @param {Pointers.RayPointerProperties} properties - The properties of the pointer, including the properties of the 
     *     underlying pick that the pointer uses to do its picking.
     * @returns {number} The ID of the pointer if successfully created, otherwise <code>0</code>.
     */
    Q_INVOKABLE unsigned int createLaserPointer(const QVariant& properties) const;

    // jsdoc @borrows from Pointers
    Q_INVOKABLE void enableLaserPointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->enablePointer(uid); }

    // jsdoc @borrows from Pointers
    Q_INVOKABLE void disableLaserPointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->disablePointer(uid); }

    // jsdoc @borrows from Pointers
    Q_INVOKABLE void removeLaserPointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->removePointer(uid); }

    /*@jsdoc
     * Edits a render state of a pointer, to change its visual appearance for the state when the pointer is intersecting 
     * something.
     * <p><strong>Note:</strong> You can only edit the properties of the existing parts of the pointer; you cannot change the
     * type of any part.</p>
     * <p><strong>Note:</strong> You cannot use this method to change the appearance of a default render state.</p>
     * @function LaserPointers.editRenderState
     * @param {number} id - The ID of the pointer.
     * @param {string} renderState - The name of the render state to edit.
     * @param {Pointers.RayPointerRenderState} properties - The new properties for the render state. Only the overlay 
     *     properties to change need be specified.
     */
    Q_INVOKABLE void editRenderState(unsigned int uid, const QString& renderState, const QVariant& properties) const;

    /*@jsdoc
     * Sets the render state of a pointer, to change its visual appearance and possibly disable or enable it.
     * @function LaserPointers.setRenderState
     * @param {string} renderState - <p>The name of the render state to set the pointer to. This may be:</p>
     *     <ul>
     *       <li>The name of one of the render states set in the pointer's properties.</li>
     *       <li><code>""</code>, to hide the pointer and disable emitting of events.</li>
     *     </ul>
     * @param {number} id - The ID of the pointer.
     */
    Q_INVOKABLE void setRenderState(unsigned int uid, const QString& renderState) const { DependencyManager::get<PointerManager>()->setRenderState(uid, renderState.toStdString()); }

    /*@jsdoc
     * Gets the most recent intersection of a pointer. A pointer continues to be updated ready to return a result, as long as
     * it is enabled, regardless of the render state.
     * @function LaserPointers.getPrevRayPickResult
     * @param {number} id - The ID of the pointer.
     * @returns {RayPickResult} The most recent intersection of the pointer.
     */
    Q_INVOKABLE QVariantMap getPrevRayPickResult(unsigned int uid) const;

     // jsdoc @borrows from Pointers
    Q_INVOKABLE void setPrecisionPicking(unsigned int uid, bool precisionPicking) const { DependencyManager::get<PointerManager>()->setPrecisionPicking(uid, precisionPicking); }

    /*@jsdoc
     * Sets the length of a pointer.
     * @function LaserPointers.setLaserLength
     * @param {number} id - The ID of the pointer.
     * @param {number} laserLength - The desired length of the pointer.
     */
    Q_INVOKABLE void setLaserLength(unsigned int uid, float laserLength) const { DependencyManager::get<PointerManager>()->setLength(uid, laserLength); }

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pointer should ignore during intersection.
     * @function LaserPointers.setIgnoreItems
     * @param {number} id - The ID of the pointer.
     * @param {Uuid[]} ignoreItems - A list of IDs to ignore.
     */
    Q_INVOKABLE void setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreEntities) const;

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pointer should include during intersection, instead of intersecting with
     * everything.
     * @function LaserPointers.setIncludeItems
     * @param {number} id - The ID of the pointer.
     * @param {Uuid[]} includeItems - A list of IDs to include.
     */
    Q_INVOKABLE void setIncludeItems(unsigned int uid, const ScriptValuePointer& includeEntities) const;


    /*@jsdoc
     * Locks a pointer onto a specific entity or avatar.
     * @function LaserPointers.setLockEndUUID
     * @param {number} id - The ID of the pointer.
     * @param {Uuid} targetID - The ID of the entity or avatar to lock the pointer on to.
     * @param {boolean} isAvatar - <code>true</code> if the target is an avatar, <code>false</code> if it is an entity.
     * @param {Mat4} [offset] - The offset of the target point from the center of the target item. If not specified, the
     *     pointer locks on to the center of the target item.
     */
    Q_INVOKABLE void setLockEndUUID(unsigned int uid, const QUuid& objectID, bool isAvatar, const glm::mat4& offsetMat = glm::mat4()) const { DependencyManager::get<PointerManager>()->setLockEndUUID(uid, objectID, isAvatar, offsetMat); }


    /*@jsdoc
     * Checks if a pointer is associated with the left hand: a pointer with <code>joint</code> property set to
     * <code>"_CONTROLLER_LEFTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_LEFTHAND"</code>.
     * @function LaserPointers.isLeftHand
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the left hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isLeftHand(unsigned int uid) { return DependencyManager::get<PointerManager>()->isLeftHand(uid); }

    /*@jsdoc
     * Checks if a pointer is associated with the right hand: a pointer with <code>joint</code> property set to
     * <code>"_CONTROLLER_RIGHTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND"</code>.
     * @function LaserPointers.isRightHand
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the right hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isRightHand(unsigned int uid) { return DependencyManager::get<PointerManager>()->isRightHand(uid); }

    /*@jsdoc
     * Checks if a pointer is associated with the system mouse: a pointer with <code>joint</code> property set to 
     * <code>"Mouse"</code>.
     * @function LaserPointers.isMouse
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the system mouse, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isMouse(unsigned int uid) { return DependencyManager::get<PointerManager>()->isMouse(uid); }
};

#endif // hifi_LaserPointerScriptingInterface_h
