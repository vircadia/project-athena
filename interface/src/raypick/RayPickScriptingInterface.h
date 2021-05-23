//
//  RayPickScriptingInterface.h
//  interface/src/raypick
//
//  Created by Sam Gondelman 8/15/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_RayPickScriptingInterface_h
#define hifi_RayPickScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include "RegisteredMetaTypes.h"
#include <DependencyManager.h>

#include "PickScriptingInterface.h"

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>RayPick</code> API is a subset of the {@link Picks} API, as used for ray picks.
 *
 * @namespace RayPick
 *
 * @deprecated This API is deprecated and will be removed. Use the {@link Picks} API instead.
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {FilterFlags} PICK_ENTITIES - Include domain and avatar entities when intersecting. 
 *     <em>Read-only.</em>
 * @property {FilterFlags} PICK_OVERLAYS - Include local entities when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_AVATARS - Include avatars when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_HUD - Include the HUD surface when intersecting in HMD mode. <em>Read-only.</em>
 * @property {FilterFlags} PICK_PRECISE - Pick against exact meshes. <em>Read-only.</em>
 * @property {FilterFlags} PICK_INCLUDE_INVISIBLE - Include invisible objects when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_INCLUDE_NONCOLLIDABLE - Include non-collidable objects when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_ALL_INTERSECTIONS - Return all intersections instead of just the closest. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_NONE - Intersected nothing with the given filter flags. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_ENTITY - Intersected an entity. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_LOCAL_ENTITY - Intersected a local entity. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_OVERLAY - Intersected an entity (3D Overlays no longer exist). <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_AVATAR - Intersected an avatar. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_HUD - Intersected the HUD surface. <em>Read-only.</em>
 */
class RayPickScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    Q_PROPERTY(unsigned int PICK_ENTITIES READ PICK_ENTITIES CONSTANT)
    Q_PROPERTY(unsigned int PICK_OVERLAYS READ PICK_OVERLAYS CONSTANT)
    Q_PROPERTY(unsigned int PICK_AVATARS READ PICK_AVATARS CONSTANT)
    Q_PROPERTY(unsigned int PICK_HUD READ PICK_HUD CONSTANT)
    Q_PROPERTY(unsigned int PICK_COARSE READ PICK_COARSE CONSTANT)
    Q_PROPERTY(unsigned int PICK_INCLUDE_INVISIBLE READ PICK_INCLUDE_INVISIBLE CONSTANT)
    Q_PROPERTY(unsigned int PICK_INCLUDE_NONCOLLIDABLE READ PICK_INCLUDE_NONCOLLIDABLE CONSTANT)
    Q_PROPERTY(unsigned int PICK_ALL_INTERSECTIONS READ PICK_ALL_INTERSECTIONS CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_NONE READ INTERSECTED_NONE CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_ENTITY READ INTERSECTED_ENTITY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_LOCAL_ENTITY READ INTERSECTED_LOCAL_ENTITY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_OVERLAY READ INTERSECTED_OVERLAY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_AVATAR READ INTERSECTED_AVATAR CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_HUD READ INTERSECTED_HUD CONSTANT)
    SINGLETON_DEPENDENCY

public:

    /*@jsdoc
     * Creates a new ray pick.
     * <p><strong>Warning:</strong> Picks created using this method currently always intersect at least visible and collidable
     * things but this may not always be the case.</p>
     * @function RayPick.createRayPick
     * @param {Picks.RayPickProperties} properties - Properties of the pick.
     * @returns {number} The ID of the pick created. <code>0</code> if invalid.
     */
    Q_INVOKABLE unsigned int createRayPick(const QVariant& properties);

    /*@jsdoc
     * Enables a ray pick.
     * @function RayPick.enableRayPick
     * @param {number} id - The ID of the ray pick.
     */
    Q_INVOKABLE void enableRayPick(unsigned int uid);

    /*@jsdoc
     * Disables a ray pick.
     * @function RayPick.disableRayPick
     * @param {number} id - The ID of the ray pick.
     */
    Q_INVOKABLE void disableRayPick(unsigned int uid);

    /*@jsdoc
     * Removes (deletes) a ray pick.
     * @function RayPick.removeRayPick
     * @param {number} id - The ID of the ray pick.
     */
    Q_INVOKABLE void removeRayPick(unsigned int uid);

    /*@jsdoc
     * Gets the most recent pick result from a ray pick. A ray pick continues to be updated ready to return a result, as long 
     * as it is enabled.
     * @function RayPick.getPrevRayPickResult
     * @param {number} id - The ID of the ray pick.
     * @returns {RayPickResult}
     */
    Q_INVOKABLE QVariantMap getPrevRayPickResult(unsigned int uid);


    /*@jsdoc
     * Sets whether or not a ray pick should use precision picking, i.e., whether it should pick against precise meshes or 
     * coarse meshes.
     * @function RayPick.setPrecisionPicking
     * @param {number} id - The ID of the ray pick.
     * @param {boolean} precisionPicking - <code>true</code> to use precision picking, <code>false</code> to use coarse picking.
     */
    Q_INVOKABLE void setPrecisionPicking(unsigned int uid, bool precisionPicking);

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a ray pick should ignore during intersection.
     * @function RayPick.setIgnoreItems
     * @param {number} id - The ID of the ray pick.
     * @param {Uuid[]} ignoreItems - The list of IDs to ignore.
     */
    Q_INVOKABLE void setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreEntities);

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a ray pick should include during intersection, instead of intersecting with 
     * everything.
     * @function RayPick.setIncludeItems
     * @param {number} id - The ID of the ray pick.
     * @param {Uuid[]} includeItems - The list of IDs to include.
     */
    Q_INVOKABLE void setIncludeItems(unsigned int uid, const ScriptValuePointer& includeEntities);


    /*@jsdoc
     * Checks if a pick is associated with the left hand: a ray or parabola pick with <code>joint</code> property set to
     * <code>"_CONTROLLER_LEFTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_LEFTHAND"</code>, or a stylus pick with 
     * <code>hand</code> property set to <code>0</code>.
     * @function RayPick.isLeftHand
     * @param {number} id - The ID of the ray pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the left hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isLeftHand(unsigned int uid);

    /*@jsdoc
     * Checks if a pick is associated with the right hand: a ray or parabola pick with <code>joint</code> property set to
     * <code>"_CONTROLLER_RIGHTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND"</code>, or a stylus pick with 
     * <code>hand</code> property set to <code>1</code>.
     * @function RayPick.isRightHand
     * @param {number} id - The ID of the ray pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the right hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isRightHand(unsigned int uid);

    /*@jsdoc
     * Checks if a pick is associated with the system mouse: a ray or parabola pick with <code>joint</code> property set to 
     * <code>"Mouse"</code>.
     * @function RayPick.isMouse
     * @param {number} id - The ID of the ray pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the system mouse, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isMouse(unsigned int uid);

public slots:

    /*@jsdoc
     * @function RayPick.PICK_ENTITIES
     * @deprecated This function is deprecated and will be removed. Use the <code>Raypick.PICK_ENTITIES</code> property instead.
     * @returns {number}
     */
    static unsigned int PICK_ENTITIES() { return PickScriptingInterface::PICK_ENTITIES(); }

    /*@jsdoc
     * @function RayPick.PICK_OVERLAYS
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_OVERLAYS</code> property instead.
     * @returns {number}
     */
    static unsigned int PICK_OVERLAYS() { return PickScriptingInterface::PICK_OVERLAYS(); }

    /*@jsdoc
     * @function RayPick.PICK_AVATARS
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_AVATARS</code> property instead.
     * @returns {number}
     */
    static unsigned int PICK_AVATARS() { return PickScriptingInterface::PICK_AVATARS(); }

    /*@jsdoc
     * @function RayPick.PICK_HUD
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_HUD</code> property instead.
     * @returns {number}
     */
    static unsigned int PICK_HUD() { return PickScriptingInterface::PICK_HUD(); }

    /*@jsdoc
     * @function RayPick.PICK_COARSE
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_COARSE</code> property instead.
     * @returns {number}
     */
    static unsigned int PICK_COARSE() { return PickScriptingInterface::PICK_COARSE(); }

    /*@jsdoc
     * @function RayPick.PICK_INCLUDE_INVISIBLE
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_INCLUDE_INVISIBLE</code> 
     *     property instead.
     * @returns {number}
     */
    static unsigned int PICK_INCLUDE_INVISIBLE() { return PickScriptingInterface::PICK_INCLUDE_INVISIBLE(); }

    /*@jsdoc
     * @function RayPick.PICK_INCLUDE_NONCOLLIDABLE
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_INCLUDE_NONCOLLIDABLE</code> 
     *     property instead.
     * @returns {number}
     */
    static unsigned int PICK_INCLUDE_NONCOLLIDABLE() { return PickScriptingInterface::PICK_INCLUDE_NONCOLLIDABLE(); }

    /*@jsdoc
     * @function RayPick.PICK_ALL_INTERSECTIONS
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.PICK_ALL_INTERSECTIONS</code> 
     *     property instead.
     * @returns {number}
     */
    static unsigned int PICK_ALL_INTERSECTIONS() { return PickScriptingInterface::PICK_ALL_INTERSECTIONS(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_NONE
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_NONE</code> property 
     *     instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_NONE() { return PickScriptingInterface::INTERSECTED_NONE(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_ENTITY
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_ENTITY</code> property 
     *     instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_ENTITY() { return PickScriptingInterface::INTERSECTED_ENTITY(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_OVERLAY
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_LOCAL_ENTITY</code> 
     *     property instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_LOCAL_ENTITY() { return PickScriptingInterface::INTERSECTED_LOCAL_ENTITY(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_OVERLAY
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_OVERLAY</code> property 
     *     instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_OVERLAY() { return PickScriptingInterface::INTERSECTED_LOCAL_ENTITY(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_AVATAR
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_AVATAR</code> property 
     *     instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_AVATAR() { return PickScriptingInterface::INTERSECTED_AVATAR(); }

    /*@jsdoc
     * @function RayPick.INTERSECTED_HUD
     * @deprecated This function is deprecated and will be removed. Use the <code>RayPick.INTERSECTED_HUD</code> property 
     *     instead.
     * @returns {number}
     */
    static unsigned int INTERSECTED_HUD() { return PickScriptingInterface::INTERSECTED_HUD(); }
};

#endif // hifi_RayPickScriptingInterface_h
