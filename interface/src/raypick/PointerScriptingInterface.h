//
//  Created by Sam Gondelman 10/20/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_PointerScriptingInterface_h
#define hifi_PointerScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include "DependencyManager.h"
#include "RegisteredMetaTypes.h"
#include <PointerManager.h>
#include <Pick.h>

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Pointers</code> API lets you create, manage, and visually represent objects for repeatedly calculating 
 * intersections with avatars, entities, and overlays. Pointers can also be configured to generate events on entities and 
 * overlays intersected.
 *
 * @namespace Pointers
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */

class PointerScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:

    /*@jsdoc
     * Specifies that a {@link Controller} action or function should trigger events on the entity or overlay currently 
     * intersected by a {@link Pointers.RayPointerProperties|Ray} or {@link Pointers.ParabolaPointerProperties|Parabola} 
     * pointer.
     * @typedef {object} Pointers.Trigger
     * @property {Controller.Standard|Controller.Actions|function} action - The controller output or function that triggers the 
     *     events on the entity or overlay. If a function, it must return a number <code>&gt;= 1.0</code> to start the action 
     *     and <code>&lt; 1.0</code> to terminate the action.
     * @property {string} button - Which button to trigger:
     *    <ul>
     *      <li><code>"Primary"</code>, <code>"Secondary"</code>, and <code>"Tertiary"</code> cause {@link Entities} and 
     *      {@link Overlays} mouse pointer events. Other button names also cause mouse events but the <code>button</code> 
     *      property in the event will be <code>"None"</code>.</li>
     *      <li><code>"Focus"</code> will try to give focus to the entity or overlay which the pointer is intersecting.</li>
     *    </ul>
     */

    /*@jsdoc
     * Creates a new ray, parabola, or stylus pointer. The pointer can have a wide range of behaviors depending on the 
     * properties specified. For example, a ray pointer may be a static ray pointer, a mouse ray pointer, or joint ray 
     * pointer.
     * <p><strong>Warning:</strong> Pointers created using this method currently always intersect at least visible and 
     * collidable things but this may not always be the case.</p>
     * @function Pointers.createPointer
     * @param {PickType} type - The type of pointer to create. Cannot be {@link PickType|PickType.Collision}.
     * @param {Pointers.RayPointerProperties|Pointers.ParabolaPointerProperties|Pointers.StylusPointerProperties} properties - 
     *     The properties of the pointer, per the pointer <code>type</code>, including the properties of the underlying pick 
     *     that the pointer uses to do its picking.
     * @returns {number} The ID of the pointer if successfully created, otherwise <code>0</code>.
     *
     * @example <caption>Create a ray pointer on the left hand that changes color when it's intersecting and that triggers 
     * events.<br />
     * Note: Stop controllerScripts.js from running to disable similar behavior from it.</caption>
     * var intersectEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 0, green: 255, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var intersectedPath = {
     *     type: "line3d",
     *     color: { red: 0, green: 255, blue: 0 },
     * };
     * var searchEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 255, green: 0, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var searchPath = {
     *     type: "line3d",
     *     color: { red: 255, green: 0, blue: 0 },
     * };
     * 
     * var renderStates = [{ name: "example", path: intersectedPath, end: intersectEnd }];
     * var defaultRenderStates = [{ name: "example", distance: 20.0, path: searchPath, end: searchEnd }];
     * 
     * // Create the pointer.
     * var rayPointer = Pointers.createPointer(PickType.Ray, {
     *     joint: "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND",
     *     filter: Picks.PICK_LOCAL_ENTITIES | Picks.PICK_DOMAIN_ENTITIES | Picks.PICK_INCLUDE_NONCOLLIDABLE,
     *     renderStates: renderStates,
     *     defaultRenderStates: defaultRenderStates,
     *     hover: true,  // Generate hover events.
     *     triggers: [
     *         { action: Controller.Standard.LTClick, button: "Primary" },  // Generate mouse events.
     *         { action: Controller.Standard.LTClick, button: "Focus" }  // Focus on web entities.
     *     ],
     *     enabled: true
     * });
     * Pointers.setRenderState(rayPointer, "example");
     * 
     * // Hover events.
     * Entities.hoverEnterEntity.connect(function (entityID, event) {
     *     print("hoverEnterEntity() : " + entityID);
     * });
     * Entities.hoverLeaveEntity.connect(function (entityID, event) {
     *     print("hoverLeaveEntity() : " + entityID);
     * });
     * 
     * // Mouse events.
     * Entities.mousePressOnEntity.connect(function (entityID, event) {
     *     print("mousePressOnEntity() : " + entityID + " , " + event.button);
     * });
     * Entities.mouseReleaseOnEntity.connect(function (entityID, event) {
     *     print("mouseReleaseOnEntity() : " + entityID + " , " + event.button);
     * });
     * 
     * // Tidy up.
     * Script.scriptEnding.connect(function () {
     *     Pointers.removePointer(rayPointer);
     * });
     */
    // TODO: expand Pointers to be able to be fully configurable with PickFilters
    Q_INVOKABLE unsigned int createPointer(const PickQuery::PickType& type, const QVariant& properties);

    /*@jsdoc
     * Enables and shows a pointer. Enabled pointers update their pick results and generate events.
     * @function Pointers.enablePointer
     * @param {number} id - The ID of the pointer.
     */
    Q_INVOKABLE void enablePointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->enablePointer(uid); }

    /*@jsdoc
     * Disables and hides a pointer. Disabled pointers do not update their pick results or generate events.
     * @function Pointers.disablePointer
     * @param {number} id - The ID of the pointer.
     */
    Q_INVOKABLE void disablePointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->disablePointer(uid); }

    /*@jsdoc
     * Gets the enabled status of a pointer. Enabled pointers update their pick results and generate events.
     * @function Pointers.isPointerEnabled
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} enabled - Whether or not the pointer is enabled.
     */
    Q_INVOKABLE bool isPointerEnabled(unsigned int uid) const;

    /*@jsdoc
     * Removes (deletes) a pointer.
     * @function Pointers.removePointer
     * @param {number} id - The ID of the pointer.
     */
    Q_INVOKABLE void removePointer(unsigned int uid) const { DependencyManager::get<PointerManager>()->removePointer(uid); }

    /*@jsdoc
    * Gets the parameters that were passed in to {@link Pointers.createPointer} to create the pointer when the pointer was 
    * created through a script. 
    * <p><strong>Note:</strong> These properties do not reflect the current state of the pointer. To get the current state 
    * of the pointer, see {@link Pointers.getPointerProperties}.
    * @function Pointers.getPointerScriptParameters
    * @param {number} id - The ID of the pointer.
    * @returns {Pointers.RayPointerProperties|Pointers.ParabolaPointerProperties|Pointers.StylusPointerProperties} 
    *     Script-provided properties, per the pointer <code>type</code>.
    */
    Q_INVOKABLE QVariantMap getPointerScriptParameters(unsigned int uid) const;

    /*@jsdoc
    * Gets all pointers which currently exist, including disabled pointers.
    * @function Pointers.getPointers
    * @returns {number[]} pointers - The IDs of the pointers.
    */
    Q_INVOKABLE QVector<unsigned int> getPointers() const;

    /*@jsdoc
     * Edits a render state of a {@link Pointers.RayPointerProperties|ray} or 
     * {@link Pointers.ParabolaPointerProperties|parabola} pointer, to change its visual appearance for the state when the 
     * pointer is intersecting something.
     * <p><strong>Note:</strong> You can only edit the properties of the existing parts of the pointer; you cannot change the 
     * type of any part.</p>
     * <p><strong>Note:</strong> You cannot use this method to change the appearance of a default render state.</p>
     * <p><strong>Note:</strong> Not able to be used with stylus pointers.</p>
     * @function Pointers.editRenderState
     * @param {number} id - The ID of the pointer.
     * @param {string} renderState - The name of the render state to edit.
     * @param {Pointers.RayPointerRenderState|Pointers.ParabolaPointerRenderState} properties - The new properties for the 
     *     render state. Only the overlay properties to change need be specified.
     * @example <caption>Change the dimensions of a ray pointer's intersecting end overlay.</caption>
     * var intersectEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 0, green: 255, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var intersectedPath = {
     *     type: "line3d",
     *     color: { red: 0, green: 255, blue: 0 },
     * };
     * var searchEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 255, green: 0, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var searchPath = {
     *     type: "line3d",
     *     color: { red: 255, green: 0, blue: 0 },
     * };
     * 
     * var renderStates = [ { name: "example", path: intersectedPath, end: intersectEnd } ];
     * var defaultRenderStates = [ { name: "example", distance: 20.0, path: searchPath, end: searchEnd } ];
     * 
     * // Create the pointer.
     * var rayPointer = Pointers.createPointer(PickType.Ray, {
     *     joint: "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND",
     *     filter: Picks.PICK_LOCAL_ENTITIES | Picks.PICK_DOMAIN_ENTITIES | Picks.PICK_INCLUDE_NONCOLLIDABLE,
     *     renderStates: renderStates,
     *     defaultRenderStates: defaultRenderStates,
     *     enabled: true
     * });
     * Pointers.setRenderState(rayPointer, "example");
     * 
     * // Edit the intersecting render state.
     * Script.setTimeout(function () {
     *     print("Edit render state");
     *     Pointers.editRenderState(rayPointer, "example", {
     *         end: { dimensions: { x: 0.5, y: 0.5, z: 0.5 } }
     *     });
     * }, 10000);
     * 
     * Script.setTimeout(function () {
     *     print("Edit render state");
     *     Pointers.editRenderState(rayPointer, "example", {
     *         end: { dimensions: { x: 0.2, y: 0.2, z: 0.2 } }
     *     });
     * }, 15000);
     * 
     * // Tidy up.
     * Script.scriptEnding.connect(function () {
     *     Pointers.removePointer(rayPointer);
     * });
     */
    Q_INVOKABLE void editRenderState(unsigned int uid, const QString& renderState, const QVariant& properties) const;

    /*@jsdoc
     * Sets the render state of a pointer, to change its visual appearance and possibly disable or enable it.  
     * @function Pointers.setRenderState
     * @param {number} id - The ID of the pointer.
     * @param {string} renderState - <p>The name of the render state to set the pointer to.</p>
     *     <p>For {@link Pointers.RayPointerProperties|ray} and {@link Pointers.ParabolaPointerProperties|parabola} pointers, 
     *     this may be:</p>
     *     <ul>
     *       <li>The name of one of the render states set in the pointer's properties.</li>
     *       <li><code>""</code>, to hide the pointer and disable emitting of events.</li>
     *     </ul>
     *     <p>For {@link Pointers.StylusPointerProperties|stylus} pointers, the values may be:</p>
     *     <ul>
     *       <li><code>"events on"</code>, to render and emit events (the default).</li>
     *       <li><code>"events off"</code>, to render but don't emit events.</li>
     *       <li><code>"disabled"</code>, to not render and not emit events.</li>
     *     </ul>
     * @example <caption>Switch a ray pointer between having a path and not having a path.</caption>
     * var intersectEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 0, green: 255, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var intersectedPath = {
     *     type: "line3d",
     *     color: { red: 0, green: 255, blue: 0 },
     * };
     * var searchEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 255, green: 0, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var searchPath = {
     *     type: "line3d",
     *     color: { red: 255, green: 0, blue: 0 },
     * };
     * 
     * var renderStates = [
     *     { name: "examplePath", path: intersectedPath, end: intersectEnd },
     *     { name: "exampleNoPath", end: intersectEnd }
     * ];
     * var defaultRenderStates = [
     *     { name: "examplePath", distance: 20.0, path: searchPath, end: searchEnd },
     *     { name: "exampleNoPath", distance: 20.0, end: searchEnd }
     * ];
     * 
     * // Create the pointer.
     * var rayPointer = Pointers.createPointer(PickType.Ray, {
     *     joint: "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND",
     *     filter: Picks.PICK_LOCAL_ENTITIES | Picks.PICK_DOMAIN_ENTITIES | Picks.PICK_INCLUDE_NONCOLLIDABLE,
     *     renderStates: renderStates,
     *     defaultRenderStates: defaultRenderStates,
     *     enabled: true
     * });
     * Pointers.setRenderState(rayPointer, "examplePath");
     * 
     * // Change states.
     * Script.setTimeout(function () {
     *     print("Without path");
     *     Pointers.setRenderState(rayPointer, "exampleNoPath");
     * }, 10000);
     * 
     * Script.setTimeout(function () {
     *     print("With path");
     *     Pointers.setRenderState(rayPointer, "examplePath");
     * }, 15000);
     * 
     * // Tidy up.
     * Script.scriptEnding.connect(function () {
     *     Pointers.removePointer(rayPointer);
     * });
     */
    Q_INVOKABLE void setRenderState(unsigned int uid, const QString& renderState) const { DependencyManager::get<PointerManager>()->setRenderState(uid, renderState.toStdString()); }


    /*@jsdoc
     * Gets the most recent intersection of a pointer. A pointer continues to be updated ready to return a result, as long as  
     * it is enabled, regardless of the render state.
     * @function Pointers.getPrevPickResult
     * @param {number} id - The ID of the pointer.
     * @returns {RayPickResult|ParabolaPickResult|StylusPickResult} The most recent intersection of the pointer.
     */
    Q_INVOKABLE QVariantMap getPrevPickResult(unsigned int uid) const;


    /*@jsdoc
     * Sets whether or not a pointer should use precision picking, i.e., whether it should pick against precise meshes or 
     * coarse meshes. This has the same effect as using the <code>PICK_PRECISE</code> or <code>PICK_COARSE</code> filter flags. 
     * @function Pointers.setPrecisionPicking
     * @param {number} id - The ID of the pointer.
     * @param {boolean} precisionPicking - <code>true</code> to use precision picking, <code>false</code> to use coarse picking.
     */
    Q_INVOKABLE void setPrecisionPicking(unsigned int uid, bool precisionPicking) const { DependencyManager::get<PointerManager>()->setPrecisionPicking(uid, precisionPicking); }

    /*@jsdoc
     * Sets the length of a pointer.
     * <p><strong>Note:</strong> Not used by stylus pointers.</p>
     * @function Pointers.setLength
     * @param {number} id - The ID of the pointer.
     * @param {number} length - The desired length of the pointer.
     */
    Q_INVOKABLE void setLength(unsigned int uid, float length) const { DependencyManager::get<PointerManager>()->setLength(uid, length); }

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pointer should ignore during intersection.
     * <p><strong>Note:</strong> Not used by stylus pointers.</p>
     * @function Pointers.setIgnoreItems
     * @param {number} id - The ID of the pointer.
     * @param {Uuid[]} ignoreItems - A list of IDs to ignore.
     */
    Q_INVOKABLE void setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreEntities) const;

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pointer should include during intersection, instead of intersecting with 
     * everything.  
     * <p><strong>Note:</strong> Stylus pointers only intersect with items in their include list.</p>
     * @function Pointers.setIncludeItems
     * @param {number} id - The ID of the pointer.
     * @param {Uuid[]} includeItems - A list of IDs to include.
     */
    Q_INVOKABLE void setIncludeItems(unsigned int uid, const ScriptValuePointer& includeEntities) const;


    /*@jsdoc
     * Locks a pointer onto a specific entity or avatar.
     * <p><strong>Note:</strong> Not used by stylus pointers.</p>
     * @function Pointers.setLockEndUUID
     * @param {number} id - The ID of the pointer.
     * @param {Uuid} targetID - The ID of the entity or avatar to lock the pointer on to.
     * @param {boolean} isAvatar - <code>true</code> if the target is an avatar, <code>false</code> if it is an entity.
     * @param {Mat4} [offset] - The offset of the target point from the center of the target item. If not specified, the 
     *     pointer locks on to the center of the target item.
     */
    Q_INVOKABLE void setLockEndUUID(unsigned int uid, const QUuid& objectID, bool isAvatar, const glm::mat4& offsetMat = glm::mat4()) const { DependencyManager::get<PointerManager>()->setLockEndUUID(uid, objectID, isAvatar, offsetMat); }


    /*@jsdoc
     * Checks if a pointer is associated with the left hand: a ray or parabola pointer with <code>joint</code> property set to
     * <code>"_CONTROLLER_LEFTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_LEFTHAND"</code>, or a stylus pointer with 
     * <code>hand</code> property set to <code>0</code>.
     * @function Pointers.isLeftHand
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the left hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isLeftHand(unsigned int uid) { return DependencyManager::get<PointerManager>()->isLeftHand(uid); }

    /*@jsdoc
     * Checks if a pointer is associated with the right hand: a ray or parabola pointer with <code>joint</code> property set to
     * <code>"_CONTROLLER_RIGHTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND"</code>, or a stylus pointer with 
     * <code>hand</code> property set to <code>1</code>.
     * @function Pointers.isRightHand
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the right hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isRightHand(unsigned int uid) { return DependencyManager::get<PointerManager>()->isRightHand(uid); }

    /*@jsdoc
     * Checks if a pointer is associated with the system mouse: a ray or parabola pointer with <code>joint</code> property set 
     * to <code>"Mouse"</code>.
     * @function Pointers.isMouse
     * @param {number} id - The ID of the pointer.
     * @returns {boolean} <code>true</code> if the pointer is associated with the system mouse, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isMouse(unsigned int uid) { return DependencyManager::get<PointerManager>()->isMouse(uid); }

    /*@jsdoc
     * Gets information about a pointer.
     * @function Pointers.getPointerProperties
     * @param {number} id - The ID of the pointer.
     * @returns {Pointers.RayPointerProperties|Pointers.ParabolaPointerProperties|object} The <code>renderStates</code> and 
     *     <code>defaultRenderStates</code> for ray and parabola pointers, <code>{}</code> for stylus pointers.
     * @example <caption>Report the properties of a parabola pointer.</caption>
     * var intersectEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 0, green: 255, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var intersectedPath = {
     *     color: { red: 0, green: 255, blue: 0 },
     * };
     * var searchEnd = {
     *     type: "sphere",
     *     dimensions: { x: 0.2, y: 0.2, z: 0.2 },
     *     solid: true,
     *     color: { red: 255, green: 0, blue: 0 },
     *     ignorePickIntersection: true
     * };
     * var searchPath = {
     *     color: { red: 255, green: 0, blue: 0 },
     * };
     * 
     * var renderStates = [{ name: "example", path: intersectedPath, end: intersectEnd }];
     * var defaultRenderStates = [{ name: "example", distance: 20.0, path: searchPath, end: searchEnd }];
     * 
     * // Create the pointer.
     * var parabolaPointer = Pointers.createPointer(PickType.Parabola, {
     *     joint: "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND",
     *     filter: Picks.PICK_LOCAL_ENTITIES | Picks.PICK_DOMAIN_ENTITIES | Picks.PICK_INCLUDE_NONCOLLIDABLE,
     *     renderStates: renderStates,
     *     defaultRenderStates: defaultRenderStates,
     *     enabled: true
     * });
     * Pointers.setRenderState(parabolaPointer, "example");
     * 
     * // Report the pointer properties.
     * Script.setTimeout(function () {
     *     var properties = Pointers.getPointerProperties(parabolaPointer);
     *     print("Pointer properties:" + JSON.stringify(properties));
     * }, 500);
     * 
     * // Tidy up.
     * Script.scriptEnding.connect(function () {
     *     Pointers.removePointer(parabolaPointer);
     * });
     */
    Q_INVOKABLE QVariantMap getPointerProperties(unsigned int uid) const;

protected:
    static std::shared_ptr<Pointer> buildLaserPointer(const QVariant& properties);
    static std::shared_ptr<Pointer> buildStylus(const QVariant& properties);
    static std::shared_ptr<Pointer> buildParabolaPointer(const QVariant& properties);
};

#endif // hifi_PointerScriptingInterface_h
