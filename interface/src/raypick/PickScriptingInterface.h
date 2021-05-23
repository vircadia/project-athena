//
//  Created by Sam Gondelman 10/20/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_PickScriptingInterface_h
#define hifi_PickScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <DependencyManager.h>
#include <PhysicsEngine.h>
#include <Pick.h>
#include <PickFilter.h>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Picks</code> API lets you create and manage objects for repeatedly calculating intersections.
 *
 * @namespace Picks
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {FilterFlags} PICK_DOMAIN_ENTITIES - Include domain entities when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_AVATAR_ENTITIES - Include avatar entities when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_LOCAL_ENTITIES - Include local entities when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_AVATARS - Include avatars when intersecting. <em>Read-only.</em>
 * @property {FilterFlags} PICK_HUD - Include the HUD surface when intersecting in HMD mode. <em>Read-only.</em>
 *
 * @property {FilterFlags} PICK_ENTITIES - Include domain and avatar entities when intersecting. <em>Read-only.</em>
 *     <p class="important">Deprecated: This property is deprecated and will be removed. Use <code>PICK_DOMAIN_ENTITIES | 
 *     PICK_AVATAR_ENTITIES</code> instead.</p>
 * @property {FilterFlags} PICK_OVERLAYS - Include local entities when intersecting. <em>Read-only.</em>
 *     <p class="important">Deprecated: This property is deprecated and will be removed. Use <code>PICK_LOCAL_ENTITIES</code> 
 *     instead.</p>
 *
 * @property {FilterFlags} PICK_INCLUDE_VISIBLE - Include visible objects when intersecting. <em>Read-only.</em>
 *     <p><strong>Warning:</strong> Is currently always enabled by default but may not be in the future.</p>
 * @property {FilterFlags} PICK_INCLUDE_INVISIBLE - Include invisible objects when intersecting. <em>Read-only.</em>
 *
 * @property {FilterFlags} PICK_INCLUDE_COLLIDABLE - Include collidable objects when intersecting. <em>Read-only.</em>
 *     <p><strong>Warning:</strong> Is currently always enabled by default but may not be in the future.</p>
 * @property {FilterFlags} PICK_INCLUDE_NONCOLLIDABLE - Include non-collidable objects when intersecting. <em>Read-only.</em>
 *
 * @property {FilterFlags} PICK_PRECISE - Pick against exact meshes. <em>Read-only.</em>
 * @property {FilterFlags} PICK_COARSE - Pick against coarse meshes. <em>Read-only.</em>
 *
 * @property {FilterFlags} PICK_ALL_INTERSECTIONS - If set, returns all intersections instead of just the closest. 
 *     <em>Read-only.</em>
 *     <p><strong>Warning:</strong> Not yet implemented.</p>
 *
 * @property {FilterFlags} PICK_BYPASS_IGNORE - Allows pick to intersect entities even when their 
 *     <code>ignorePickIntersection</code> property value is <code>true</code>. For debug purposes.
 *     <em>Read-only.</em>
 *
 * @property {IntersectionType} INTERSECTED_NONE - Intersected nothing. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_ENTITY - Intersected an entity. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_LOCAL_ENTITY - Intersected a local entity. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_OVERLAY - Intersected a local entity. (3D overlays no longer exist.) 
 *     <em>Read-only.</em>
 *     <p class="important">Deprecated: This property is deprecated and will be removed. Use 
 *     <code>INTERSECTED_LOCAL_ENTITY</code> instead.</p>
 * @property {IntersectionType} INTERSECTED_AVATAR - Intersected an avatar. <em>Read-only.</em>
 * @property {IntersectionType} INTERSECTED_HUD - Intersected the HUD surface. <em>Read-only.</em>
 *
 * @property {number} perFrameTimeBudget - The maximum time, in microseconds, to spend per frame updating pick results.
 */

class PickScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    Q_PROPERTY(unsigned int PICK_ENTITIES READ PICK_ENTITIES CONSTANT)
    Q_PROPERTY(unsigned int PICK_OVERLAYS READ PICK_OVERLAYS CONSTANT)

    Q_PROPERTY(unsigned int PICK_DOMAIN_ENTITIES READ PICK_DOMAIN_ENTITIES CONSTANT)
    Q_PROPERTY(unsigned int PICK_AVATAR_ENTITIES READ PICK_AVATAR_ENTITIES CONSTANT)
    Q_PROPERTY(unsigned int PICK_LOCAL_ENTITIES READ PICK_LOCAL_ENTITIES CONSTANT)
    Q_PROPERTY(unsigned int PICK_AVATARS READ PICK_AVATARS CONSTANT)
    Q_PROPERTY(unsigned int PICK_HUD READ PICK_HUD CONSTANT)

    Q_PROPERTY(unsigned int PICK_INCLUDE_VISIBLE READ PICK_INCLUDE_VISIBLE CONSTANT)
    Q_PROPERTY(unsigned int PICK_INCLUDE_INVISIBLE READ PICK_INCLUDE_INVISIBLE CONSTANT)

    Q_PROPERTY(unsigned int PICK_INCLUDE_COLLIDABLE READ PICK_INCLUDE_COLLIDABLE CONSTANT)
    Q_PROPERTY(unsigned int PICK_INCLUDE_NONCOLLIDABLE READ PICK_INCLUDE_NONCOLLIDABLE CONSTANT)

    Q_PROPERTY(unsigned int PICK_PRECISE READ PICK_PRECISE CONSTANT)
    Q_PROPERTY(unsigned int PICK_COARSE READ PICK_COARSE CONSTANT)

    Q_PROPERTY(unsigned int PICK_ALL_INTERSECTIONS READ PICK_ALL_INTERSECTIONS CONSTANT)

    Q_PROPERTY(unsigned int PICK_BYPASS_IGNORE READ PICK_BYPASS_IGNORE CONSTANT)

    Q_PROPERTY(unsigned int INTERSECTED_NONE READ INTERSECTED_NONE CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_ENTITY READ INTERSECTED_ENTITY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_LOCAL_ENTITY READ INTERSECTED_LOCAL_ENTITY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_OVERLAY READ INTERSECTED_OVERLAY CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_AVATAR READ INTERSECTED_AVATAR CONSTANT)
    Q_PROPERTY(unsigned int INTERSECTED_HUD READ INTERSECTED_HUD CONSTANT)
    Q_PROPERTY(unsigned int perFrameTimeBudget READ getPerFrameTimeBudget WRITE setPerFrameTimeBudget)
    SINGLETON_DEPENDENCY

public:
    void registerMetaTypes(ScriptEngine* engine);

    /*@jsdoc
     * Creates a new pick. Different {@link PickType}s use different properties, and within one PickType the properties you 
     * choose can lead to a wide range of behaviors. For example, with <code>PickType.Ray</code>, the properties could 
     * configure a mouse ray pick, an avatar head ray pick, or a joint ray pick.
     * <p><strong>Warning:</strong> Picks created using this method currently always intersect at least visible and collidable 
     * things but this may not always be the case.</p>
     * @function Picks.createPick
     * @param {PickType} type - The type of picking to use.
     * @param {Picks.RayPickProperties|Picks.ParabolaPickProperties|Picks.StylusPickProperties|Picks.CollisionPickProperties} 
     *     properties - Properties of the pick, per the pick <code>type</code>.
     * @returns {number} The ID of the pick created. <code>0</code> if invalid.
     */
    // TODO: expand Pointers to be able to be fully configurable with PickFilters
    Q_INVOKABLE unsigned int createPick(const PickQuery::PickType type, const QVariant& properties);

    /*@jsdoc
     * Enables a pick. Enabled picks update their pick results.
     * @function Picks.enablePick
     * @param {number} id - The ID of the pick.
     */
    Q_INVOKABLE void enablePick(unsigned int uid);

    /*@jsdoc
     * Disables a pick. Disabled picks do not update their pick results.
     * @function Picks.disablePick
     * @param {number} id - The ID of the pick.
     */
    Q_INVOKABLE void disablePick(unsigned int uid);

    /*@jsdoc
     * Get the enabled status of a pick. Enabled picks update their pick results.
     * @function Picks.isPickEnabled
     * @param {number} id - The ID of the pick.
     * @returns {boolean} enabled - Whether or not the pick is enabled.
     */
    Q_INVOKABLE bool isPickEnabled(unsigned int uid) const;

    /*@jsdoc
     * Removes (deletes) a pick.
     * @function Picks.removePick
     * @param {number} id - The ID of the pick.
     */
    Q_INVOKABLE void removePick(unsigned int uid);

    /*@jsdoc
     * Gets the current properties of the pick.
     * @function Picks.getPickProperties
     * @param {number} id - The ID of the pick.
     * @returns {Picks.RayPickProperties|Picks.ParabolaPickProperties|Picks.StylusPickProperties|Picks.CollisionPickProperties}
     *     Properties of the pick, per the pick <code>type</code>.
     */
    Q_INVOKABLE QVariantMap getPickProperties(unsigned int uid) const;

    /*@jsdoc
     * Gets the parameters that were passed in to {@link Picks.createPick} to create the pick, if the pick was created through 
     * a script. Note that these properties do not reflect the current state of the pick.
     * See {@link Picks.getPickProperties}.
     * @function Picks.getPickScriptParameters
     * @param {number} id - The ID of the pick.
     * @returns {Picks.RayPickProperties|Picks.ParabolaPickProperties|Picks.StylusPickProperties|Picks.CollisionPickProperties} 
     *     Script-provided properties, per the pick <code>type</code>.
     */
    Q_INVOKABLE QVariantMap getPickScriptParameters(unsigned int uid) const;

    /*@jsdoc
     * Gets all picks which currently exist, including disabled picks.
     * @function Picks.getPicks
     * @returns {number[]} picks - The IDs of the picks.
     */
    Q_INVOKABLE QVector<unsigned int> getPicks() const;

    /*@jsdoc
     * Gets the most recent result from a pick. A pick continues to be updated ready to return a result, as long as it is 
     * enabled.
     * <p><strong>Note:</strong> Stylus picks only intersect with objects in their include list, set using 
     * {@link Picks.setIncludeItems|setIncludeItems}.</p>
     * @function Picks.getPrevPickResult
     * @param {number} id - The ID of the pick.
     * @returns {RayPickResult|ParabolaPickResult|StylusPickResult|CollisionPickResult} The most recent intersection result.
     * @example <caption>Highlight entities under your mouse in desktop mode or that you're looking at in HMD mode.</caption>
     * // Highlight.
     * var HIGHLIGHT_LIST_NAME = "highlightEntitiesExampleList";
     * var HIGHLIGHT_LIST_TYPE = "entity";
     * Selection.enableListHighlight(HIGHLIGHT_LIST_NAME, {});
     * 
     * // Ray pick.
     * var PICK_FILTER = Picks.PICK_DOMAIN_ENTITIES | Picks.PICK_AVATAR_ENTITIES
     *         | Picks.PICK_INCLUDE_COLLIDABLE | Picks.PICK_INCLUDE_NONCOLLIDABLE;
     * var rayPick = Picks.createPick(PickType.Ray, {
     *     enabled: true,
     *     filter: PICK_FILTER,
     *     joint: HMD.active ? "Avatar" : "Mouse"
     * });
     * 
     * // Highlight intersected entity.
     * var highlightedEntityID = null;
     * Script.update.connect(function () {
     *     var rayPickResult = Picks.getPrevPickResult(rayPick);
     *     if (rayPickResult.intersects) {
     *         if (rayPickResult.objectID !== highlightedEntityID) {
     *             if (highlightedEntityID) {
     *                 Selection.removeFromSelectedItemsList(HIGHLIGHT_LIST_NAME, HIGHLIGHT_LIST_TYPE, highlightedEntityID);
     *             }
     *             highlightedEntityID = rayPickResult.objectID;
     *             Selection.addToSelectedItemsList(HIGHLIGHT_LIST_NAME, HIGHLIGHT_LIST_TYPE, highlightedEntityID);
     *         }
     *     } else {
     *         if (highlightedEntityID) {
     *             Selection.removeFromSelectedItemsList(HIGHLIGHT_LIST_NAME, HIGHLIGHT_LIST_TYPE, highlightedEntityID);
     *             highlightedEntityID = null;
     *         }
     *     }
     * });
     * 
     * // Clean up.
     * Script.scriptEnding.connect(function () {
     *     if (highlightedEntityID) {
     *         Selection.removeFromSelectedItemsList(HIGHLIGHT_LIST_NAME, HIGHLIGHT_LIST_TYPE, highlightedEntityID);
     *     }
     * });
     */
    Q_INVOKABLE QVariantMap getPrevPickResult(unsigned int uid);

    /*@jsdoc
     * Sets whether or not a pick should use precision picking, i.e., whether it should pick against precise meshes or coarse 
     * meshes.
     * This has the same effect as using the <code>PICK_PRECISE</code> or <code>PICK_COARSE</code> filter flags.
     * @function Picks.setPrecisionPicking
     * @param {number} id - The ID of the pick.
     * @param {boolean} precisionPicking - <code>true</code> to use precision picking, <code>false</code> to use coarse picking.
     */
    Q_INVOKABLE void setPrecisionPicking(unsigned int uid, bool precisionPicking);

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pick should ignore during intersection.
     * <p><strong>Note:</strong> Not used by stylus picks.</p>
     * @function Picks.setIgnoreItems
     * @param {number} id - The ID of the pick.
     * @param {Uuid[]} ignoreItems - The list of IDs to ignore.
     */
    Q_INVOKABLE void setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreItems);

    /*@jsdoc
     * Sets a list of entity and avatar IDs that a pick should include during intersection, instead of intersecting with 
     * everything.
     * <p><strong>Note:</strong> Stylus picks only intersect with items in their include list.</p>
     * @function Picks.setIncludeItems
     * @param {number} id - The ID of the pick.
     * @param {Uuid[]} includeItems - The list of IDs to include.
     */
    Q_INVOKABLE void setIncludeItems(unsigned int uid, const ScriptValuePointer& includeItems);

    /*@jsdoc
     * Checks if a pick is associated with the left hand: a ray or parabola pick with <code>joint</code> property set to 
     * <code>"_CONTROLLER_LEFTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_LEFTHAND"</code>, or a stylus pick with 
     * <code>hand</code> property set to <code>0</code>.
     * @function Picks.isLeftHand
     * @param {number} id - The ID of the pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the left hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isLeftHand(unsigned int uid);

    /*@jsdoc
     * Checks if a pick is associated with the right hand: a ray or parabola pick with <code>joint</code> property set to
     * <code>"_CONTROLLER_RIGHTHAND"</code> or <code>"_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND"</code>, or a stylus pick with 
     * <code>hand</code> property set to <code>1</code>.
     * @function Picks.isRightHand
     * @param {number} id - The ID of the pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the right hand, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isRightHand(unsigned int uid);

    /*@jsdoc
     * Checks if a pick is associated with the system mouse: a ray or parabola pick with <code>joint</code> property set to 
     * <code>"Mouse"</code>.
     * @function Picks.isMouse
     * @param {number} id - The ID of the pick.
     * @returns {boolean} <code>true</code> if the pick is associated with the system mouse, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isMouse(unsigned int uid);

    unsigned int getPerFrameTimeBudget() const;
    void setPerFrameTimeBudget(unsigned int numUsecs);

    static constexpr unsigned int PICK_BYPASS_IGNORE() { return PickFilter::getBitMask(PickFilter::FlagBit::PICK_BYPASS_IGNORE); }

public slots:

    /*@jsdoc
     * @function Picks.PICK_ENTITIES
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_DOMAIN_ENTITIES | 
     *     Picks.PICK_AVATAR_ENTITIES</code> properties expression instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_ENTITIES() { return PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES); }

    /*@jsdoc
     * @function Picks.PICK_OVERLAYS
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_LOCAL_ENTITIES</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_OVERLAYS() { return PickFilter::getBitMask(PickFilter::FlagBit::LOCAL_ENTITIES); }


    /*@jsdoc
     * @function Picks.PICK_DOMAIN_ENTITIES
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_DOMAIN_ENTITIES</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_DOMAIN_ENTITIES() { return PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES); }

    /*@jsdoc
     * @function Picks.PICK_AVATAR_ENTITIES
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_AVATAR_ENTITIES</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_AVATAR_ENTITIES() { return PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES); }

    /*@jsdoc
     * @function Picks.PICK_LOCAL_ENTITIES
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_LOCAL_ENTITIES</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_LOCAL_ENTITIES() { return PickFilter::getBitMask(PickFilter::FlagBit::LOCAL_ENTITIES); }

    /*@jsdoc
     * @function Picks.PICK_AVATARS
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_AVATARS</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_AVATARS() { return PickFilter::getBitMask(PickFilter::FlagBit::AVATARS); }

    /*@jsdoc
     * @function Picks.PICK_HUD
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_HUD</code> property instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_HUD() { return PickFilter::getBitMask(PickFilter::FlagBit::HUD); }


    /*@jsdoc
     * @function Picks.PICK_INCLUDE_VISIBLE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_INCLUDE_VISIBLE</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_INCLUDE_VISIBLE() { return PickFilter::getBitMask(PickFilter::FlagBit::VISIBLE); }

    /*@jsdoc
     * @function Picks.PICK_INCLUDE_INVISIBLE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_INCLUDE_INVISIBLE</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_INCLUDE_INVISIBLE() { return PickFilter::getBitMask(PickFilter::FlagBit::INVISIBLE); }


    /*@jsdoc
     * @function Picks.PICK_INCLUDE_COLLIDABLE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_INCLUDE_COLLIDABLE</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_INCLUDE_COLLIDABLE() { return PickFilter::getBitMask(PickFilter::FlagBit::COLLIDABLE); }

    /*@jsdoc
     * @function Picks.PICK_INCLUDE_NONCOLLIDABLE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_INCLUDE_NONCOLLIDABLE</code> 
     *     property instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_INCLUDE_NONCOLLIDABLE() { return PickFilter::getBitMask(PickFilter::FlagBit::NONCOLLIDABLE); }


    /*@jsdoc
     * @function Picks.PICK_PRECISE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_PRECISE</code> property instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_PRECISE() { return PickFilter::getBitMask(PickFilter::FlagBit::PRECISE); }

    /*@jsdoc
     * @function Picks.PICK_COARSE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_COARSE</code> property instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_COARSE() { return PickFilter::getBitMask(PickFilter::FlagBit::COARSE); }


    /*@jsdoc
     * @function Picks.PICK_ALL_INTERSECTIONS
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.PICK_ALL_INTERSECTIONS</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int PICK_ALL_INTERSECTIONS() { return PickFilter::getBitMask(PickFilter::FlagBit::PICK_ALL_INTERSECTIONS); }

    /*@jsdoc
     * @function Picks.INTERSECTED_NONE
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_NONE</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_NONE() { return IntersectionType::NONE; }

    /*@jsdoc
     * @function Picks.INTERSECTED_ENTITY
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_ENTITY</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_ENTITY() { return IntersectionType::ENTITY; }

    /*@jsdoc
     * @function Picks.INTERSECTED_LOCAL_ENTITY
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_LOCAL_ENTITY</code> 
     *     property instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_LOCAL_ENTITY() { return IntersectionType::LOCAL_ENTITY; }

    /*@jsdoc
     * @function Picks.INTERSECTED_OVERLAY
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_LOCAL_ENTITY</code> 
     *     property instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_OVERLAY() { return INTERSECTED_LOCAL_ENTITY(); }

    /*@jsdoc
     * @function Picks.INTERSECTED_AVATAR
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_AVATAR</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_AVATAR() { return IntersectionType::AVATAR; }

    /*@jsdoc
     * @function Picks.INTERSECTED_HUD
     * @deprecated This function is deprecated and will be removed. Use the <code>Picks.INTERSECTED_HUD</code> property 
     *     instead.
     * @returns {number}
     */
    static constexpr unsigned int INTERSECTED_HUD() { return IntersectionType::HUD; }

protected:
    static std::shared_ptr<PickQuery> buildRayPick(const QVariantMap& properties);
    static std::shared_ptr<PickQuery> buildStylusPick(const QVariantMap& properties);
    static std::shared_ptr<PickQuery> buildCollisionPick(const QVariantMap& properties);
    static std::shared_ptr<PickQuery> buildParabolaPick(const QVariantMap& properties);

    static void setParentTransform(std::shared_ptr<PickQuery> pick, const QVariantMap& propMap);
};

#endif // hifi_PickScriptingInterface_h
