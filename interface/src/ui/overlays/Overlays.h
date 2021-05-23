//
//  Overlays.h
//  interface/src/ui/overlays
//
//  Modified by Zander Otavka on 7/15/15
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
//  Exposes methods to scripts for managing `Overlay`s and `OverlayPanel`s.
//
//  YOU SHOULD NOT USE `Overlays` DIRECTLY, unless you like pain and deprecation.  Instead, use
//  the object oriented API replacement found in `examples/libraries/overlayManager.js`.  See
//  that file for docs and usage.
//

#ifndef hifi_Overlays_h
#define hifi_Overlays_h

#include <QMouseEvent>
#include <QReadWriteLock>

#include <PointerEvent.h>
#include <QtCore/QSharedPointer>

#include "Overlay.h"

#include <EntityScriptingInterface.h>

class PickRay;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The result of a {@link PickRay} search using {@link Overlays.findRayIntersection|findRayIntersection}.
 * @typedef {object} Overlays.RayToOverlayIntersectionResult
 * @property {boolean} intersects - <code>true</code> if the {@link PickRay} intersected with a 3D overlay, otherwise
 *     <code>false</code>.
 * @property {Uuid} overlayID - The UUID of the local entity that was intersected.
 * @property {number} distance - The distance from the {@link PickRay} origin to the intersection point.
 * @property {Vec3} surfaceNormal - The normal of the overlay surface at the intersection point.
 * @property {Vec3} intersection - The position of the intersection point.
 * @property {object} extraInfo Additional intersection details, if available.
 */
class RayToOverlayIntersectionResult {
public:
    bool intersects { false };
    QUuid overlayID;
    float distance { 0.0f };
    BoxFace face { UNKNOWN_FACE };
    glm::vec3 surfaceNormal;
    glm::vec3 intersection;
    QVariantMap extraInfo;
};
Q_DECLARE_METATYPE(RayToOverlayIntersectionResult);
ScriptValuePointer RayToOverlayIntersectionResultToScriptValue(ScriptEngine* engine, const RayToOverlayIntersectionResult& value);
void RayToOverlayIntersectionResultFromScriptValue(const ScriptValuePointer& object, RayToOverlayIntersectionResult& value);

class ParabolaToOverlayIntersectionResult {
public:
    bool intersects { false };
    QUuid overlayID;
    float distance { 0.0f };
    float parabolicDistance { 0.0f };
    BoxFace face { UNKNOWN_FACE };
    glm::vec3 surfaceNormal;
    glm::vec3 intersection;
    QVariantMap extraInfo;
};

/*@jsdoc
 * The <code>Overlays</code> API provides facilities to create and interact with overlays. These are 2D and 3D objects visible 
 * only to yourself and that aren't persisted to the domain. They are used for UI.
 *
 * <p><strong>Note:</strong> 3D overlays are local {@link Entities}, internally, so many of the methods also work with 
 * entities.</p>
 *
 * <p class="important">3D overlays are deprecated: Use local {@link Entities} for these instead.</p>
 *
 * @namespace Overlays
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {Uuid} keyboardFocusOverlay - The <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay 
 *     ({@link Entities.EntityProperties-Web|Web} entity) that has keyboard focus. If no overlay (entity) has keyboard focus, 
 *     returns <code>null</code>; set to <code>null</code> or {@link Uuid(0)|Uuid.NULL} to clear keyboard focus.
 */

class Overlays : public QObject {
    Q_OBJECT

    Q_PROPERTY(QUuid keyboardFocusOverlay READ getKeyboardFocusOverlay WRITE setKeyboardFocusOverlay)

public:
    Overlays();

    void init();
    void update(float deltatime);
    void render(RenderArgs* renderArgs);
    void disable();
    void enable();

    Overlay::Pointer take2DOverlay(const QUuid& id);
    Overlay::Pointer get2DOverlay(const QUuid& id) const;

    /// adds an overlay that's already been created
    QUuid addOverlay(Overlay* overlay) { return add2DOverlay(Overlay::Pointer(overlay)); }
    QUuid add2DOverlay(const Overlay::Pointer& overlay);

    RayToOverlayIntersectionResult findRayIntersectionVector(const PickRay& ray, bool precisionPicking,
        const QVector<EntityItemID>& include,
        const QVector<EntityItemID>& discard,
        bool visibleOnly = false, bool collidableOnly = false);

    ParabolaToOverlayIntersectionResult findParabolaIntersectionVector(const PickParabola& parabola, bool precisionPicking,
        const QVector<EntityItemID>& include,
        const QVector<EntityItemID>& discard,
        bool visibleOnly = false, bool collidableOnly = false);

    void cleanupAllOverlays();

    mutable ScriptEnginePointer _scriptEngine;

public slots:
    /*@jsdoc
     * Adds an overlay to the scene.
     * @function Overlays.addOverlay
     * @param {Overlays.OverlayType} type - The type of the overlay to add.
     * @param {Overlays.OverlayProperties} properties - The properties of the overlay to add.
     * @returns {Uuid} The ID of the newly created overlay if successful, otherwise {@link Uuid(0)|Uuid.NULL}.
     * @example <caption>Add a cube overlay in front of your avatar.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     */
    QUuid addOverlay(const QString& type, const QVariant& properties);

    /*@jsdoc
     * Creates a clone of an existing overlay (or entity).
     * <p>Note: For cloning behavior of 3D overlays and entities, see {@link Entities.cloneEntity}.</p>
     * @function Overlays.cloneOverlay
     * @param {Uuid} id - The ID of the overlay (or entity) to clone.
     * @returns {Uuid} The ID of the new overlay (or entity) if successful, otherwise {@link Uuid(0)|Uuid.NULL}.
     */
    QUuid cloneOverlay(const QUuid& id);

    /*@jsdoc
     * Edits an overlay's (or entity's) properties.
     * @function Overlays.editOverlay
     * @param {Uuid} id - The ID of the overlay (or entity) to edit.
     * @param {Overlays.OverlayProperties} properties - The properties changes to make.
     * @returns {boolean} <code>false</code> if Interface is exiting. Otherwise, if a 2D overlay then <code>true</code> always, 
     *    and if a 3D overlay then <code>true</code> if the overlay was found and edited, otherwise <code>false</code>.
     * @example <caption>Add an overlay in front of your avatar then change its color.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     *
     * var success = Overlays.editOverlay(overlay, {
     *     color: { red: 255, green: 0, blue: 0 }
     * });
     * print("Success: " + success);
     */
    bool editOverlay(const QUuid& id, const QVariant& properties);

    /*@jsdoc
     * Edits the properties of multiple overlays (or entities).
     * @function Overlays.editOverlays
     * @param propertiesById {object.<Uuid, Overlays.OverlayProperties>} - An object with overlay (or entity) IDs as keys and
     *     {@link Overlays.OverlayProperties|OverlayProperties} edits to make as values.
     * @returns {boolean} <code>false</code> if Interface is exiting, otherwise <code>true</code>.
     * @example <caption>Create two overlays in front of your avatar then change their colors.</caption>
     * var overlayA = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: -0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var overlayB = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     *
     * var overlayEdits = {};
     * overlayEdits[overlayA] = { color: { red: 255, green: 0, blue: 0 } };
     * overlayEdits[overlayB] = { color: { red: 0, green: 255, blue: 0 } };
     * var success = Overlays.editOverlays(overlayEdits);
     * print("Success: " + success);
     */
    bool editOverlays(const QVariant& propertiesById);

    /*@jsdoc
     * Deletes an overlay (or entity).
     * @function Overlays.deleteOverlay
     * @param {Uuid} id - The ID of the overlay (or entity) to delete.
     */
    void deleteOverlay(const QUuid& id);

    /*@jsdoc
     * Gets the type of an overlay.
     * @function Overlays.getOverlayType
     * @param {Uuid} id - The ID of the overlay to get the type of.
     * @returns {Overlays.OverlayType} The type of the overlay if found, otherwise <code>"unknown"</code>.
     * @example <caption>Create an overlay in front of your avatar then get and report its type.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var type = Overlays.getOverlayType(overlay);
     * print("Type: " + type); // cube
     */
    QString getOverlayType(const QUuid& id);

    /*@jsdoc
     * Gets an overlay's (or entity's) script object. In particular, this is useful for accessing a 
     * <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay's <code>EventBridge</code> script object to 
     * exchange messages with the web page script.
     * <p>To send a message from an Interface script to a <code>"web3d"</code> overlay over its event bridge:</p>
     * <pre class="prettyprint"><code>var overlayObject = Overlays.getOverlayObject(overlayID);
     * overlayObject.emitScriptEvent(message);</code></pre>
     * <p>To receive a message from a <code>"web3d"</code> overlay over its event bridge in an Interface script:</p>
     * <pre class="prettyprint"><code>var overlayObject = Overlays.getOverlayObject(overlayID);
     * overlayObject.webEventReceived.connect(function(message) {
     *     ...
     * };</code></pre>
     * @function Overlays.getOverlayObject
     * @param {Uuid} overlayID - The ID of the overlay to get the script object of.
     * @returns {object} The script object for the overlay if found.
     * @example <caption>Exchange messages with a <code>"web3d"</code> overlay.</caption>
     * // HTML file, name: "web3d.html".
     * <!DOCTYPE html>
     * <html>
     * <head>
     *     <title>HELLO</title>
     * </head>
     * <body>
     *     <h1>HELLO</h1>
     *     <script>
     *         function onScriptEventReceived(message) {
     *             // Message received from the script.
     *             console.log("Message received: " + message);
     *         }
     * 
     *         EventBridge.scriptEventReceived.connect(onScriptEventReceived);
     * 
     *         setInterval(function () {
     *             // Send a message to the script.
     *             EventBridge.emitWebEvent("hello");
     *         }, 2000);
     *     </script>
     * </body>
     * </html>
     * 
     * // Interface script file.
     * var web3DOverlay = Overlays.addOverlay("web3d", {
     *     type: "Web",
     *     position : Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y : 0.5, z : -3 })),
     *     rotation : MyAvatar.orientation,
     *     sourceUrl : Script.resolvePath("web3d.html"),
     *     alpha : 1.0
     *     });
     * 
     * var overlayObject;
     * 
     * function onWebEventReceived(message) {
     *     // Message received.
     *     print("Message received: " + message);
     * 
     *     // Send a message back.
     *     overlayObject.emitScriptEvent(message + " back");
     * }
     * 
     * Script.setTimeout(function() {
     *     overlayObject = Overlays.getOverlayObject(web3DOverlay);
     *     overlayObject.webEventReceived.connect(onWebEventReceived);
     * }, 500);
     * 
     * Script.scriptEnding.connect(function() {
     *     Overlays.deleteOverlay(web3DOverlay);
     * });
     */
    QObject* getOverlayObject(const QUuid& id);

    /*@jsdoc
     * Gets the ID of the 2D overlay at a particular point on the desktop screen or HUD surface.
     * @function Overlays.getOverlayAtPoint
     * @param {Vec2} point - The point to check for an overlay.
     * @returns {Uuid} The ID of the 2D overlay at the specified point if found, otherwise <code>null</code>.
     * @example <caption>Create a 2D overlay and add an event function that reports the overlay clicked on, if any.</caption>
     * var overlay = Overlays.addOverlay("rectangle", {
     *     bounds: { x: 100, y: 100, width: 200, height: 100 },
     *     color: { red: 255, green: 255, blue: 255 }
     * });
     * print("Created: " + overlay);
     *
     * Controller.mousePressEvent.connect(function (event) {
     *     var overlay = Overlays.getOverlayAtPoint({ x: event.x, y: event.y });
     *     print("Clicked: " + overlay);
     * });
     */
    QUuid getOverlayAtPoint(const glm::vec2& point);

    /*@jsdoc
     * Gets a specified property value of a 3D overlay (or entity).
     * <p><strong>Note:</strong> 2D overlays' property values cannot be retrieved.</p>
     * @function Overlays.getProperty
     * @param {Uuid} id - The ID of the 3D overlay (or entity).
     * @param {string} property - The name of the property to get the value of.
     * @returns {object} The value of the property if the 3D overlay and property can be found, otherwise
     *     <code>undefined</code>.
     * @example <caption>Create an overlay in front of your avatar then report its alpha property value.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var alpha = Overlays.getProperty(overlay, "alpha");
     * print("Overlay alpha: " + alpha);
     */
    QVariant getProperty(const QUuid& id, const QString& property);

    /*@jsdoc
     * Gets specified property values of a 3D overlay (or entity).
     * <p><strong>Note:</strong> 2D overlays' property values cannot be retrieved.</p>
     * @function Overlays.getProperties
     * @param {Uuid} id - The ID of the overlay (or entity).
     * @param {Array.<string>} properties - The names of the properties to get the values of.
     * @returns {Overlays.OverlayProperties} The values of valid properties if the overlay can be found, otherwise an empty 
     *     object.
     * @example <caption>Create an overlay in front of your avatar then report some of its properties.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var properties = Overlays.getProperties(overlay, ["color", "alpha", "grabbable"]);
     * print("Overlay properties: " + JSON.stringify(properties));
     */
    QVariantMap getProperties(const QUuid& id, const QStringList& properties);

    /*@jsdoc
     * Gets the values of multiple overlays' (or entities') properties.
     * @function Overlays.getOverlaysProperties
     * @param propertiesById {object.<Uuid, Array.<string>>} - An object with overlay (or entity) IDs as keys and arrays of the 
     *     names of properties to get for each as values.
     * @returns {object.<Uuid, Overlays.OverlayProperties>} An object with overlay (or entity) IDs as keys and
     *     {@link Overlays.OverlayProperties|OverlayProperties} as values.
     * @example <caption>Create two cube overlays in front of your avatar then get some of their properties.</caption>
     * var overlayA = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: -0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var overlayB = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * var propertiesToGet = {};
     * propertiesToGet[overlayA] = ["color", "alpha"];
     * propertiesToGet[overlayB] = ["dimensions"];
     * var properties = Overlays.getOverlaysProperties(propertiesToGet);
     * print("Overlays properties: " + JSON.stringify(properties));
     */
    QVariantMap getOverlaysProperties(const QVariant& overlaysProperties);

    /*@jsdoc
     * Finds the closest 3D overlay (or local entity) intersected by a {@link PickRay}.
     * @function Overlays.findRayIntersection
     * @param {PickRay} pickRay - The PickRay to use for finding overlays.
     * @param {boolean} [precisionPicking=false] - <code>true</code> to pick against precise meshes, <code>false</code> to pick 
     *     against coarse meshes. If <code>true</code> and the intersected entity is a model, the result's 
     *     <code>extraInfo</code> property includes more information than it otherwise would.
     * @param {Array.<Uuid>} [include=[]] - If not empty, then the search is restricted to these overlays (and local entities).
     * @param {Array.<Uuid>} [discard=[]] - Overlays (and local entities) to ignore during the search.
     * @param {boolean} [visibleOnly=false] - <code>true</code> if only overlays (and local entities) that are 
     *     <code>{@link Overlays.OverlayProperties|visible}</code> should be searched.
     * @param {boolean} [collideableOnly=false] - <code>true</code> if only local entities that are not 
     *     <code>{@link Entities.EntityProperties|collisionless}</code> should be searched.
     * @returns {Overlays.RayToOverlayIntersectionResult} The result of the search for the first intersected overlay (or local 
     *     entity.
     * @example <caption>Create a cube overlay in front of your avatar. Report 3D overlay intersection details for mouse 
     *     clicks.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     *
     * Controller.mousePressEvent.connect(function (event) {
     *     var pickRay = Camera.computePickRay(event.x, event.y);
     *     var intersection = Overlays.findRayIntersection(pickRay);
     *     print("Intersection: " + JSON.stringify(intersection));
     * });
     */
    RayToOverlayIntersectionResult findRayIntersection(const PickRay& ray,
                                                       bool precisionPicking = false,
                                                       const ScriptValuePointer& include = ScriptValuePointer(),
                                                       const ScriptValuePointer& discard = ScriptValuePointer(),
                                                       bool visibleOnly = false,
                                                       bool collidableOnly = false);

    /*@jsdoc
     * Gets a list of visible 3D overlays (local entities) with bounding boxes that touch a search sphere.
     * @function Overlays.findOverlays
     * @param {Vec3} center - The center of the search sphere.
     * @param {number} radius - The radius of the search sphere.
     * @returns {Uuid[]} The IDs of the overlays (local entities) that are visible and have bounding boxes that touch a search 
     *     sphere.
     * @example <caption>Create two overlays in front of your avatar then search for overlays near your avatar.</caption>
     * var overlayA = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: -0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("Overlay A: " + overlayA);
     * var overlayB = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0.3, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("Overlay B: " + overlayB);
     *
     * var overlaysFound = Overlays.findOverlays(MyAvatar.position, 5.0);
     * print("Overlays found: " + JSON.stringify(overlaysFound));
     */
    QVector<QUuid> findOverlays(const glm::vec3& center, float radius);

    /*@jsdoc
     * Checks whether an overlay's (or entity's) assets have been loaded. For example, for an 
     * <code>{@link Overlays.OverlayProperties-Image|"image"}</code> overlay, the result indicates whether its image has been 
     * loaded.
     * @function Overlays.isLoaded
     * @param {Uuid} id - The ID of the overlay (or entity) to check.
     * @returns {boolean} <code>true</code> if the overlay's (or entity's) assets have been loaded, otherwise 
     *     <code>false</code>.
     * @example <caption>Create an image overlay and report whether its image is loaded after 1s.</caption>
     * var overlay = Overlays.addOverlay("image", {
     *     bounds: { x: 100, y: 100, width: 200, height: 200 },
     *     imageURL: "https://content.vircadia.com/eu-c-1/vircadia-assets/interface/default/default_particle.png"
     * });
     * Script.setTimeout(function () {
     *     var isLoaded = Overlays.isLoaded(overlay);
     *     print("Image loaded: " + isLoaded);
     * }, 1000);
     */
    bool isLoaded(const QUuid& id);

    /*@jsdoc
     * Calculates the size of some text in a text overlay (or entity). The overlay (or entity) need not be set visible.
     * <p><strong>Note:</strong> The size of text in a 3D overlay (or entity) cannot be calculated immediately after the 
     * overlay (or entity) is created; a short delay is required while the overlay (or entity) finishes being created.</p>
     * @function Overlays.textSize
     * @param {Uuid} id - The ID of the overlay (or entity) to use for calculation.
     * @param {string} text - The string to calculate the size of.
     * @returns {Size} The size of the <code>text</code> if the object is a text overlay (or entity), otherwise
     *     <code>{ height: 0, width : 0 }</code>. If the object is a 2D overlay, the size is in pixels; if the object is a 3D 
     *     overlay (or entity), the size is in meters.
     * @example <caption>Calculate the size of "hello" in a 3D text entity.</caption>
     * var overlay = Overlays.addOverlay("text3d", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -2 })),
     *     rotation: MyAvatar.orientation,
     *     lineHeight: 0.2,
     *     visible: false
     * });
     *
     * Script.setTimeout(function() {
     *     var textSize = Overlays.textSize(overlay, "hello");
     *     print("Size of \"hello\": " + JSON.stringify(textSize));
     * }, 500);
     */
    QSizeF textSize(const QUuid& id, const QString& text);

    /*@jsdoc
     * Gets the width of the Interface window or HUD surface.
     * @function Overlays.width
     * @returns {number} The width, in pixels, of the Interface window if in desktop mode or the HUD surface if in HMD mode.
     */
    float width();

    /*@jsdoc
     * Gets the height of the Interface window or HUD surface.
     * @function Overlays.height
     * @returns {number} The height, in pixels, of the Interface window if in desktop mode or the HUD surface if in HMD mode.
     */
    float height();

    /*@jsdoc
     * Checks if an overlay (or entity) exists.
     * @function Overlays.isAddedOverlay
     * @param {Uuid} id - The ID of the overlay (or entity) to check.
     * @returns {boolean} <code>true</code> if an overlay (or entity) with the given ID exists, <code>false</code> if it doesn't.
     */
    bool isAddedOverlay(const QUuid& id);

    /*@jsdoc
     * Generates a mouse press event on an overlay (or local entity).
     * @function Overlays.sendMousePressOnOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a mouse press event on.
     * @param {PointerEvent} event - The mouse press event details.
     * @example <caption>Create a 2D rectangle overlay plus a 3D cube overlay and generate mousePressOnOverlay events for the 
     * 2D overlay.</caption>
     * var overlay3D = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("3D overlay: " + overlay);
     *
     * var overlay2D = Overlays.addOverlay("rectangle", {
     *     bounds: { x: 100, y: 100, width: 200, height: 100 },
     *     color: { red: 255, green: 255, blue: 255 }
     * });
     * print("2D overlay: " + overlay);
     *
     * // Overlays.mousePressOnOverlay by default applies only to 3D overlays.
     * Overlays.mousePressOnOverlay.connect(function(overlayID, event) {
     *     print("Clicked: " + overlayID);
     * });
     *
     * Controller.mousePressEvent.connect(function (event) {
     *     // Overlays.getOverlayAtPoint applies only to 2D overlays.
     *     var overlay = Overlays.getOverlayAtPoint({ x: event.x, y: event.y });
     *     if (overlay) {
     *         Overlays.sendMousePressOnOverlay(overlay, {
     *             type: "press",
     *             id: 0,
     *             pos2D: event
     *         });
     *     }
     * });
     */
    void sendMousePressOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Generates a mouse release event on an overlay (or local entity).
     * @function Overlays.sendMouseReleaseOnOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a mouse release event on.
     * @param {PointerEvent} event - The mouse release event details.
     */
    void sendMouseReleaseOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Generates a mouse move event on an overlay (or local entity).
     * @function Overlays.sendMouseMoveOnOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a mouse move event on.
     * @param {PointerEvent} event - The mouse move event details.
     */
    void sendMouseMoveOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Generates a hover enter event on an overlay (or local entity).
     * @function Overlays.sendHoverEnterOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a hover enter event on.
     * @param {PointerEvent} event - The hover enter event details.
     */
    void sendHoverEnterOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Generates a hover over event on an overlay (or entity).
     * @function Overlays.sendHoverOverOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a hover over event on.
     * @param {PointerEvent} event - The hover over event details.
     */
    void sendHoverOverOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Generates a hover leave event on an overlay (or local entity).
     * @function Overlays.sendHoverLeaveOverlay
     * @param {Uuid} id - The ID of the overlay (or local entity) to generate a hover leave event on.
     * @param {PointerEvent} event - The hover leave event details.
     */
    void sendHoverLeaveOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Gets the ID of the <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay 
     * ({@link Entities.EntityProperties-Web|Web} entity) that has keyboard focus.
     * @function Overlays.getKeyboardFocusOverlay
     * @returns {Uuid} The ID of the <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay 
     * ({@link Entities.EntityProperties-Web|Web} entity) that has focus, if any, otherwise <code>null</code>.
     */
    QUuid getKeyboardFocusOverlay() { return DependencyManager::get<EntityScriptingInterface>()->getKeyboardFocusEntity(); }

    /*@jsdoc
     * Sets the <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay 
     * ({@link Entities.EntityProperties-Web|Web} entity) that has keyboard focus.
     * @function Overlays.setKeyboardFocusOverlay
     * @param {Uuid} id - The ID of the <code>{@link Overlays.OverlayProperties-Web3D|"web3d"}</code> overlay 
     * ({@link Entities.EntityProperties-Web|Web} entity) to set keyboard focus to. Use <code>null</code> or 
     * {@link Uuid(0)|Uuid.NULL} to unset keyboard focus from an overlay (entity).
     */
    void setKeyboardFocusOverlay(const QUuid& id) { DependencyManager::get<EntityScriptingInterface>()->setKeyboardFocusEntity(id); }

signals:
    /*@jsdoc
     * Triggered when an overlay (or entity) is deleted.
     * @function Overlays.overlayDeleted
     * @param {Uuid} id - The ID of the overlay (or entity) that was deleted.
     * @returns {Signal}
     * @example <caption>Create an overlay then delete it after 1s.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("Overlay: " + overlay);
     *
     * Overlays.overlayDeleted.connect(function(overlayID) {
     *     print("Deleted: " + overlayID);
     * });
     * Script.setTimeout(function () {
     *     Overlays.deleteOverlay(overlay);
     * }, 1000);
     */
    void overlayDeleted(const QUuid& id);

    /*@jsdoc
     * Triggered when a mouse press event occurs on an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendMousePressOnOverlay|sendMousePressOnOverlay} for a 2D overlay).
     * @function Overlays.mousePressOnOverlay
     * @param {Uuid} id - The ID of the overlay the mouse press event occurred on.
     * @param {PointerEvent} event - The mouse press event details.
     * @returns {Signal}
     * @example <caption>Create a cube overlay in front of your avatar and report mouse clicks on it.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("My overlay: " + overlay);
     *
     * Overlays.mousePressOnOverlay.connect(function(overlayID, event) {
     *     if (overlayID === overlay) {
     *         print("Clicked on my overlay");
     *     }
     * });
     */
    void mousePressOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse double press event occurs on an overlay. Only occurs for 3D overlays.
     * @function Overlays.mouseDoublePressOnOverlay
     * @param {Uuid} id - The ID of the overlay the mouse double press event occurred on.
     * @param {PointerEvent} event - The mouse double press event details.
     * @returns {Signal}
     */
    void mouseDoublePressOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse release event occurs on an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendMouseReleaseOnOverlay|sendMouseReleaseOnOverlay} for a 2D overlay).
     * @function Overlays.mouseReleaseOnOverlay
     * @param {Uuid} id - The ID of the overlay the mouse release event occurred on.
     * @param {PointerEvent} event - The mouse release event details.
     * @returns {Signal}
     */
    void mouseReleaseOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse move event occurs on an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendMouseMoveOnOverlay|sendMouseMoveOnOverlay} for a 2D overlay).
     * @function Overlays.mouseMoveOnOverlay
     * @param {Uuid} id - The ID of the overlay the mouse moved event occurred on.
     * @param {PointerEvent} event - The mouse move event details.
     * @returns {Signal}
     */
    void mouseMoveOnOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse press event occurs on something other than a 3D overlay.
     * @function Overlays.mousePressOffOverlay
     * @returns {Signal}
     */
    void mousePressOffOverlay();

    /*@jsdoc
     * Triggered when a mouse double press event occurs on something other than a 3D overlay.
     * @function Overlays.mouseDoublePressOffOverlay
     * @returns {Signal}
     */
    void mouseDoublePressOffOverlay();

    /*@jsdoc
     * Triggered when a mouse cursor starts hovering over an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendHoverEnterOverlay|sendHoverEnterOverlay} for a 2D overlay).
     * @function Overlays.hoverEnterOverlay
     * @param {Uuid} id - The ID of the overlay the mouse moved event occurred on.
     * @param {PointerEvent} event - The mouse move event details.
     * @returns {Signal}
     * @example <caption>Create a cube overlay in front of your avatar and report when you start hovering your mouse over
     *     it.</caption>
     * var overlay = Overlays.addOverlay("cube", {
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.3, y: 0.3, z: 0.3 },
     *     solid: true
     * });
     * print("Overlay: " + overlay);
     * Overlays.hoverEnterOverlay.connect(function(overlayID, event) {
     *     print("Hover enter: " + overlayID);
     * });
     */
    void hoverEnterOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse cursor continues hovering over an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendHoverOverOverlay|sendHoverOverOverlay} for a 2D overlay).
     * @function Overlays.hoverOverOverlay
     * @param {Uuid} id - The ID of the overlay the hover over event occurred on.
     * @param {PointerEvent} event - The hover over event details.
     * @returns {Signal}
     */
    void hoverOverOverlay(const QUuid& id, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse cursor finishes hovering over an overlay. Only occurs for 3D overlays (unless you use 
     *     {@link Overlays.sendHoverLeaveOverlay|sendHoverLeaveOverlay} for a 2D overlay).
     * @function Overlays.hoverLeaveOverlay
     * @param {Uuid} id - The ID of the overlay the hover leave event occurred on.
     * @param {PointerEvent} event - The hover leave event details.
     * @returns {Signal}
     */
    void hoverLeaveOverlay(const QUuid& id, const PointerEvent& event);

private:
    void cleanupOverlaysToDelete();

    mutable QMutex _mutex { QMutex::Recursive };
    QMap<QUuid, Overlay::Pointer> _overlays;
    QList<Overlay::Pointer> _overlaysToDelete;

    unsigned int _stackOrder { 1 };

    bool _enabled { true };
    std::atomic<bool> _shuttingDown { false };

    PointerEvent calculateOverlayPointerEvent(const QUuid& id, const PickRay& ray, const RayToOverlayIntersectionResult& rayPickResult,
        QMouseEvent* event, PointerEvent::EventType eventType);

    static QString entityToOverlayType(const QString& type);
    static QString overlayToEntityType(const QString& type);
    static std::unordered_map<QString, QString> _entityToOverlayTypes;
    static std::unordered_map<QString, QString> _overlayToEntityTypes;

    QVariantMap convertEntityToOverlayProperties(const EntityItemProperties& entityProps);
    EntityItemProperties convertOverlayToEntityProperties(QVariantMap& overlayProps, const QString& type, bool add, const QUuid& id);
    EntityItemProperties convertOverlayToEntityProperties(QVariantMap& overlayProps, std::pair<glm::quat, bool>& rotationToSave, const QString& type, bool add, const QUuid& id = QUuid());

private slots:
    void mousePressOnPointerEvent(const QUuid& id, const PointerEvent& event);
    void mousePressOffPointerEvent();
    void mouseDoublePressOnPointerEvent(const QUuid& id, const PointerEvent& event);
    void mouseDoublePressOffPointerEvent();
    void mouseReleasePointerEvent(const QUuid& id, const PointerEvent& event);
    void mouseMovePointerEvent(const QUuid& id, const PointerEvent& event);
    void hoverEnterPointerEvent(const QUuid& id, const PointerEvent& event);
    void hoverOverPointerEvent(const QUuid& id, const PointerEvent& event);
    void hoverLeavePointerEvent(const QUuid& id, const PointerEvent& event);


};

#define ADD_TYPE_MAP(entity, overlay) \
    _entityToOverlayTypes[#entity] = #overlay; \
    _overlayToEntityTypes[#overlay] = #entity;

#endif // hifi_Overlays_h
