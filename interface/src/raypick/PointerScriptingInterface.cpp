//
//  Created by Sam Gondelman 10/20/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PointerScriptingInterface.h"

#include <QtCore/QVariant>
#include <GLMHelpers.h>
#include <shared/QtHelpers.h>

#include <ScriptValueUtils.h>

#include "Application.h"
#include "PickManager.h"
#include "LaserPointer.h"
#include "StylusPointer.h"
#include "ParabolaPointer.h"
#include "StylusPick.h"

static const glm::quat X_ROT_NEG_90{ 0.70710678f, -0.70710678f, 0.0f, 0.0f };
static const glm::vec3 DEFAULT_POSITION_OFFSET{0.0f, 0.0f, -StylusPick::WEB_STYLUS_LENGTH / 2.0f};
static const glm::vec3 DEFAULT_MODEL_DIMENSIONS{0.01f, 0.01f, StylusPick::WEB_STYLUS_LENGTH};

void PointerScriptingInterface::setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreItems) const {
    DependencyManager::get<PointerManager>()->setIgnoreItems(uid, qVectorQUuidFromScriptValue(ignoreItems));
}

void PointerScriptingInterface::setIncludeItems(unsigned int uid, const ScriptValuePointer& includeItems) const {
    DependencyManager::get<PointerManager>()->setIncludeItems(uid, qVectorQUuidFromScriptValue(includeItems));
}

unsigned int PointerScriptingInterface::createPointer(const PickQuery::PickType& type, const QVariant& properties) {
    // Interaction with managers should always happen on the main thread
    if (QThread::currentThread() != qApp->thread()) {
        unsigned int result;
        BLOCKING_INVOKE_METHOD(this, "createPointer", Q_RETURN_ARG(unsigned int, result), Q_ARG(PickQuery::PickType, type), Q_ARG(QVariant, properties));
        return result;
    }

    QVariantMap propertyMap = properties.toMap();

    std::shared_ptr<Pointer> pointer;
    switch (type) {
        case PickQuery::PickType::Ray:
            pointer = buildLaserPointer(propertyMap);
            break;
        case PickQuery::PickType::Stylus:
            pointer = buildStylus(propertyMap);
            break;
        case PickQuery::PickType::Parabola:
            pointer = buildParabolaPointer(propertyMap);
            break;
        default:
            break;
    }

    if (!pointer) {
        return PointerEvent::INVALID_POINTER_ID;
    }

    propertyMap["pointerType"] = (int)type;

    pointer->setScriptParameters(propertyMap);

    return DependencyManager::get<PointerManager>()->addPointer(pointer);
}

bool PointerScriptingInterface::isPointerEnabled(unsigned int uid) const {
    return DependencyManager::get<PointerManager>()->isPointerEnabled(uid);
}

QVector<unsigned int> PointerScriptingInterface::getPointers() const {
    return DependencyManager::get<PointerManager>()->getPointers();
}

QVariantMap PointerScriptingInterface::getPointerProperties(unsigned int uid) const {
    return DependencyManager::get<PointerManager>()->getPointerProperties(uid);
}

QVariantMap PointerScriptingInterface::getPointerScriptParameters(unsigned int uid) const {
    return DependencyManager::get<PointerManager>()->getPointerScriptParameters(uid);
}

/*@jsdoc
 * The properties of a stylus pointer. These include the properties from the underlying stylus pick that the pointer uses.
 * @typedef {object} Pointers.StylusPointerProperties
 * @property {Pointers.StylusPointerModel} [model] - Override some or all of the default stylus model properties.
 * @property {boolean} [hover=false] - <code>true</code> if the pointer generates {@link Entities} hover events, 
 *     <code>false</code> if it doesn't.
 * @property {PickType} pointerType - The type of the stylus pointer returned from {@link Pointers.getPointerProperties} 
 *     or {@link Pointers.getPointerScriptParameters}. A stylus pointer's type is {@link PickType(0)|PickType.Stylus}.
 * @property {number} [pickID] - The ID of the pick created alongside this pointer, returned from 
 *     {@link Pointers.getPointerProperties}. 
 * @see {@link Picks.StylusPickProperties} for additional properties from the underlying stylus pick.
 */
 
/*@jsdoc
 * The properties of a stylus pointer model.
 * @typedef {object} Pointers.StylusPointerModel
 * @property {string} [url] - The url of a model to use for the stylus, to override the default stylus mode.
 * @property {Vec3} [dimensions] - The dimensions of the stylus, to override the default stylus dimensions.
 * @property {Vec3} [positionOffset] - The position offset of the model from the stylus tip, to override the default position 
 *     offset.
 * @property {Quat} [rotationOffset] - The rotation offset of the model from the hand, to override the default rotation offset.
 */
std::shared_ptr<Pointer> PointerScriptingInterface::buildStylus(const QVariant& properties) {
    QVariantMap propertyMap = properties.toMap();

    bool hover = false;
    if (propertyMap["hover"].isValid()) {
        hover = propertyMap["hover"].toBool();
    }

    bool enabled = false;
    if (propertyMap["enabled"].isValid()) {
        enabled = propertyMap["enabled"].toBool();
    }

    glm::vec3 modelPositionOffset = DEFAULT_POSITION_OFFSET;
    glm::quat modelRotationOffset = X_ROT_NEG_90;
    glm::vec3 modelDimensions = DEFAULT_MODEL_DIMENSIONS;

    if (propertyMap["model"].isValid()) {
        QVariantMap modelData = propertyMap["model"].toMap();

        if (modelData["positionOffset"].isValid()) {
            modelPositionOffset = vec3FromVariant(modelData["positionOffset"]);
        }

        if (modelData["rotationOffset"].isValid()) {
            modelRotationOffset = quatFromVariant(modelData["rotationOffset"]);
        }

        if (modelData["dimensions"].isValid()) {
            modelDimensions = vec3FromVariant(modelData["dimensions"]);
        }
    }

    return std::make_shared<StylusPointer>(properties, StylusPointer::buildStylus(propertyMap), hover, enabled, modelPositionOffset, modelRotationOffset, modelDimensions);
}

/*@jsdoc
 * Properties that define the visual appearance of a ray pointer when the pointer is not intersecting something. These are the 
 * properties of {@link Pointers.RayPointerRenderState} but with an additional property.
 * @typedef {object} Pointers.DefaultRayPointerRenderState
 * @property {number} distance - The distance at which to render the end of the ray pointer.
 * @see {@link Pointers.RayPointerRenderState} for the remainder of the properties.
 */
/*@jsdoc
 * Properties that define the visual appearance of a ray pointer when the pointer is intersecting something.
 * @typedef {object} Pointers.RayPointerRenderState
 * @property {string} name - When creating using {@link Pointers.createPointer}, the name of the render state.
 * @property {Overlays.OverlayProperties|Uuid} [start]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of 
 *     an overlay to render at the start of the ray pointer. The <code>type</code> property must be specified.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, the ID of the overlay rendered at the start of the ray;
 *     <code>null</code> if there is no overlay.
 *
 * @property {Overlays.OverlayProperties|Uuid} [path]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of
 *     the overlay rendered for the path of the ray pointer. The <code>type</code> property must be specified and be 
 *     <code>"line3d"</code>.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, the ID of the overlay rendered for the path of the ray;
 *     <code>null</code> if there is no overlay.
 *
 * @property {Overlays.OverlayProperties|Uuid} [end]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of
 *     an overlay to render at the end of the ray pointer. The <code>type</code> property must be specified.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, the ID of the overlay rendered at the end of the ray; 
 *     <code>null</code> if there is no overlay.
 */
/*@jsdoc
 * The properties of a ray pointer. These include the properties from the underlying ray pick that the pointer uses.
 * @typedef {object} Pointers.RayPointerProperties
 * @property {boolean} [faceAvatar=false] - <code>true</code> if the overlay rendered at the end of the ray rotates about the 
 *     world y-axis to always face the avatar; <code>false</code> if it maintains its world orientation.
 * @property {boolean} [centerEndY=true] - <code>true</code> if the overlay rendered at the end of the ray is centered on 
 *     the ray end; <code>false</code> if the overlay is positioned against the surface if <code>followNormal</code> is 
 *     <code>true</code>, or above the ray end if <code>followNormal</code> is <code>false</code>.
* @property {boolean} [lockEnd=false] - <code>true</code> if the end of the ray is locked to the center of the object at 
 *     which the ray is pointing; <code>false</code> if the end of the ray is at the intersected surface.
 * @property {boolean} [distanceScaleEnd=false] - <code>true</code> if the dimensions of the overlay at the end of the ray 
 *     scale linearly with distance; <code>false</code> if they aren't. 
 * @property {boolean} [scaleWithParent=false] - <code>true</code> if the width of the ray's path and the size of the 
 *     start and end overlays scale linearly with the pointer parent's scale; <code>false</code> if they don't scale.
 * @property {boolean} [scaleWithAvatar=false] - A synonym for <code>scalewithParent</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} [followNormal=false] - <code>true</code> if the overlay rendered at the end of the ray rotates to 
 *     follow the normal of the surface if one is intersected; <code>false</code> if it doesn't.
 * @property {number} [followNormalStrength=0.0] - How quickly the overlay rendered at the end of the ray rotates to follow 
 *     the normal of an intersected surface. If <code>0</code> or <code>1</code>, the overlay rotation follows instantaneously; 
 *    for other values, the larger the value the more quickly the rotation follows.
 * @property {Pointers.RayPointerRenderState[]|Object.<string, Pointers.RayPointerRenderState>} [renderStates]
 *     <p>A set of visual states that can be switched among using {@link Pointers.setRenderState}. These define the visual 
 *     appearance of the pointer when it is intersecting something.</p>
 *     <p>When setting using {@link Pointers.createPointer}, an array of 
 *     {@link Pointers.RayPointerRenderState|RayPointerRenderState} values.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, an object mapping render state names to 
 *     {@link Pointers.RayPointerRenderState|RayPointerRenderState} values.</p>
 * @property {Pointers.DefaultRayPointerRenderState[]|Object.<string, Pointers.DefaultRayPointerRenderState>} 
 *     [defaultRenderStates]
 *     <p>A set of visual states that can be switched among using {@link Pointers.setRenderState}. These define the visual
 *     appearance of the pointer when it is not intersecting something.</p>
 *     <p>When setting using {@link Pointers.createPointer}, an array of 
 *     {@link Pointers.DefaultRayPointerRenderState|DefaultRayPointerRenderState} values.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, an object mapping render state names to 
 *     {@link Pointers.DefaultRayPointerRenderState|DefaultRayPointerRenderState} values.</p>
 * @property {boolean} [hover=false] - <code>true</code> if the pointer generates {@link Entities} hover events, 
 *     <code>false</code> if it doesn't.
 * @property {Pointers.Trigger[]} [triggers=[]] - A list of ways that a {@link Controller} action or function should trigger 
 *     events on the entity or overlay currently intersected.
 * @property {PickType} pointerType - The type of pointer returned from {@link Pointers.getPointerProperties} or 
 *     {@link Pointers.getPointerScriptParameters}. A laser pointer's type is {@link PickType(0)|PickType.Ray}.
 * @property {number} [pickID] - The ID of the pick created alongside this pointer, returned from 
 *     {@link Pointers.getPointerProperties}. 
 * @see {@link Picks.RayPickProperties} for additional properties from the underlying ray pick.
 */
std::shared_ptr<Pointer> PointerScriptingInterface::buildLaserPointer(const QVariant& properties) {
    QVariantMap propertyMap = properties.toMap();

#if defined (Q_OS_ANDROID)
    QString jointName { "" };
    if (propertyMap["joint"].isValid()) {
        QString jointName = propertyMap["joint"].toString();
        const QString MOUSE_JOINT = "Mouse";
        if (jointName == MOUSE_JOINT) {
            return nullptr;
        }
    }
#endif

    bool faceAvatar = false;
    if (propertyMap["faceAvatar"].isValid()) {
        faceAvatar = propertyMap["faceAvatar"].toBool();
    }

    bool centerEndY = true;
    if (propertyMap["centerEndY"].isValid()) {
        centerEndY = propertyMap["centerEndY"].toBool();
    }

    bool lockEnd = false;
    if (propertyMap["lockEnd"].isValid()) {
        lockEnd = propertyMap["lockEnd"].toBool();
    }

    bool distanceScaleEnd = false;
    if (propertyMap["distanceScaleEnd"].isValid()) {
        distanceScaleEnd = propertyMap["distanceScaleEnd"].toBool();
    }

    bool scaleWithParent = false;
    if (propertyMap["scaleWithParent"].isValid()) {
        scaleWithParent = propertyMap["scaleWithParent"].toBool();
    } else if (propertyMap["scaleWithAvatar"].isValid()) {
        scaleWithParent = propertyMap["scaleWithAvatar"].toBool();
    }

    bool followNormal = false;
    if (propertyMap["followNormal"].isValid()) {
        followNormal = propertyMap["followNormal"].toBool();
    }
    float followNormalStrength = 0.0f;
    if (propertyMap["followNormalStrength"].isValid()) {
        followNormalStrength = propertyMap["followNormalStrength"].toFloat();
    }

    bool enabled = false;
    if (propertyMap["enabled"].isValid()) {
        enabled = propertyMap["enabled"].toBool();
    }

    RenderStateMap renderStates;
    if (propertyMap["renderStates"].isValid()) {
        QList<QVariant> renderStateVariants = propertyMap["renderStates"].toList();
        for (const QVariant& renderStateVariant : renderStateVariants) {
            if (renderStateVariant.isValid()) {
                QVariantMap renderStateMap = renderStateVariant.toMap();
                if (renderStateMap["name"].isValid()) {
                    std::string name = renderStateMap["name"].toString().toStdString();
                    renderStates[name] = LaserPointer::buildRenderState(renderStateMap);
                }
            }
        }
    }

    DefaultRenderStateMap defaultRenderStates;
    if (propertyMap["defaultRenderStates"].isValid()) {
        QList<QVariant> renderStateVariants = propertyMap["defaultRenderStates"].toList();
        for (const QVariant& renderStateVariant : renderStateVariants) {
            if (renderStateVariant.isValid()) {
                QVariantMap renderStateMap = renderStateVariant.toMap();
                if (renderStateMap["name"].isValid() && renderStateMap["distance"].isValid()) {
                    std::string name = renderStateMap["name"].toString().toStdString();
                    float distance = renderStateMap["distance"].toFloat();
                    defaultRenderStates[name] = std::pair<float, std::shared_ptr<StartEndRenderState>>(distance, LaserPointer::buildRenderState(renderStateMap));
                }
            }
        }
    }

    bool hover = false;
    if (propertyMap["hover"].isValid()) {
        hover = propertyMap["hover"].toBool();
    }

    PointerTriggers triggers;
    auto userInputMapper = DependencyManager::get<UserInputMapper>();
    if (propertyMap["triggers"].isValid()) {
        QList<QVariant> triggerVariants = propertyMap["triggers"].toList();
        for (const QVariant& triggerVariant : triggerVariants) {
            if (triggerVariant.isValid()) {
                QVariantMap triggerMap = triggerVariant.toMap();
                if (triggerMap["action"].isValid() && triggerMap["button"].isValid()) {
                    controller::Endpoint::Pointer endpoint = userInputMapper->endpointFor(controller::Input(triggerMap["action"].toUInt()));
                    if (endpoint) {
                        std::string button = triggerMap["button"].toString().toStdString();
                        triggers.emplace_back(endpoint, button);
                    }
                }
            }
        }
    }

    return std::make_shared<LaserPointer>(properties, renderStates, defaultRenderStates, hover, triggers,
                                          faceAvatar, followNormal, followNormalStrength, centerEndY, lockEnd,
                                          distanceScaleEnd, scaleWithParent, enabled);
}

/*@jsdoc
 * The visual appearance of the parabolic path.
 * @typedef {object} Pointers.ParabolaPointerPath
 * @property {Color} [color=255,255,255] - The color of the parabola.
 * @property {number} [alpha=1.0] - The opacity of the parabola, range <code>0.0</code> &ndash; <code>1.0</code>.
 * @property {number} [width=0.01] - The width of the parabola, in meters.
 * @property {boolean} [isVisibleInSecondaryCamera=false] - <code>true</code> if the parabola is rendered in the secondary 
 *     camera, <code>false</code> if it isn't. 
 * @property {boolean} [drawInFront=false] - <code>true</code> if the parabola is rendered in front of objects in the world, 
 *     but behind the HUD, <code>false</code> if it is occluded by objects in front of it.
 */
/*@jsdoc
 * Properties that define the visual appearance of a parabola pointer when the pointer is not intersecting something. These are
 * properties of {@link Pointers.ParabolaPointerRenderState} but with an additional property.
 * @typedef {object} Pointers.DefaultParabolaPointerRenderState
 * @property {number} distance - The distance along the parabola at which to render the end of the parabola pointer.
 * @see {@link Pointers.ParabolaPointerRenderState} for the remainder of the properties.
 */
/*@jsdoc
 * Properties that define the visual appearance of a parabola pointer when the pointer is intersecting something.
 * @typedef {object} Pointers.ParabolaPointerRenderState
 * @property {string} name - When creating using {@link Pointers.createPointer}, the name of the render state.
 * @property {Overlays.OverlayProperties|Uuid} [start]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of
 *     an overlay to render at the start of the parabola pointer. The <code>type</code> property must be specified.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, the ID of the overlay rendered at the start of the 
 *     parabola; <code>null</code> if there is no overlay.
 * @property {Pointers.ParabolaPointerPath|Uuid} [path]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of
 *     the rendered path of the parabola pointer.</p>
 *     <p>This property is not provided when getting using {@link Pointers.getPointerProperties}.
 * @property {Overlays.OverlayProperties|Uuid} [end]
 *     <p>When creating or editing using {@link Pointers.createPointer} or {@link Pointers.editRenderState}, the properties of
 *     an overlay to render at the end of the ray pointer. The <code>type</code> property must be specified.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, the ID of the overlay rendered at the end of the parabola;
 *     <code>null</code> if there is no overlay.
 */
/*@jsdoc
 * The properties of a parabola pointer. These include the properties from the underlying parabola pick that the pointer uses.
 * @typedef {object} Pointers.ParabolaPointerProperties
 * @property {boolean} [faceAvatar=false] - <code>true</code> if the overlay rendered at the end of the ray rotates about the
 *     world y-axis to always face the avatar; <code>false</code> if it maintains its world orientation.
 * @property {boolean} [centerEndY=true] - <code>true</code> if the overlay rendered at the end of the ray is centered on
 *     the ray end; <code>false</code> if the overlay is positioned against the surface if <code>followNormal</code> is
 *     <code>true</code>, or above the ray end if <code>followNormal</code> is <code>false</code>.
* @property {boolean} [lockEnd=false] - <code>true</code> if the end of the ray is locked to the center of the object at
 *     which the ray is pointing; <code>false</code> if the end of the ray is at the intersected surface.
 * @property {boolean} [distanceScaleEnd=false] - <code>true</code> if the dimensions of the overlay at the end of the ray
 *     scale linearly with distance; <code>false</code> if they aren't.
 * @property {boolean} [scaleWithParent=false] - <code>true</code> if the width of the ray's path and the size of the
 *     start and end overlays scale linearly with the pointer parent's scale; <code>false</code> if they don't scale.
 * @property {boolean} [scaleWithAvatar=false] - A synonym for <code>scalewithParent</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} [followNormal=false] - <code>true</code> if the overlay rendered at the end of the ray rotates to
 *     follow the normal of the surface if one is intersected; <code>false</code> if it doesn't.
 * @property {number} [followNormalStrength=0.0] - How quickly the overlay rendered at the end of the ray rotates to follow
 *     the normal of an intersected surface. If <code>0</code> or <code>1</code>, the overlay rotation follows instantaneously;
 *    for other values, the larger the value the more quickly the rotation follows.
 * @property {Pointers.ParabolaPointerRenderState[]|Object.<string, Pointers.ParabolaPointerRenderState>} [renderStates] 
 *     <p>A set of visual states that can be switched among using {@link Pointers.setRenderState}. These define the visual
 *     appearance of the pointer when it is intersecting something.</p>
 *     <p>When setting using {@link Pointers.createPointer}, an array of
 *     {@link Pointers.ParabolaPointerRenderState|ParabolaPointerRenderState} values.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, an object mapping render state names to
 *     {@link Pointers.ParabolaPointerRenderState|ParabolaPointerRenderState} values.</p>
 * @property {Pointers.DefaultParabolaPointerRenderState[]|Object.<string, Pointers.DefaultParabolaPointerRenderState>} 
 *     [defaultRenderStates] 
 *     <p>A set of visual states that can be switched among using {@link Pointers.setRenderState}. These define the visual
 *     appearance of the pointer when it is not intersecting something.</p>
 *     <p>When setting using {@link Pointers.createPointer}, an array of
 *     {@link Pointers.DefaultParabolaPointerRenderState|DefaultParabolaPointerRenderState} values.</p>
 *     <p>When getting using {@link Pointers.getPointerProperties}, an object mapping render state names to
 *     {@link Pointers.DefaultParabolaPointerRenderState|DefaultParabolaPointerRenderState} values.</p>
 * @property {boolean} [hover=false] - <code>true</code> if the pointer generates {@link Entities} hover events,
 *     <code>false</code> if it doesn't.
 * @property {Pointers.Trigger[]} [triggers=[]] - A list of ways that a {@link Controller} action or function should trigger
 *     events on the entity or overlay currently intersected.
 * @property {PickType} pointerType - The type of pointer returned from {@link Pointers.getPointerProperties} or 
 *     {@link Pointers.getPointerScriptParameters}. A parabola pointer's type is {@link PickType(0)|PickType.Parabola}.
 * @property {number} [pickID] - The ID of the pick created alongside this pointer, returned from 
 *     {@link Pointers.getPointerProperties}. 
 * @see {@link Picks.ParabolaPickProperties} for additional properties from the underlying parabola pick.
 */
std::shared_ptr<Pointer> PointerScriptingInterface::buildParabolaPointer(const QVariant& properties) {
    QVariantMap propertyMap = properties.toMap();

    bool faceAvatar = false;
    if (propertyMap["faceAvatar"].isValid()) {
        faceAvatar = propertyMap["faceAvatar"].toBool();
    }

    bool centerEndY = true;
    if (propertyMap["centerEndY"].isValid()) {
        centerEndY = propertyMap["centerEndY"].toBool();
    }

    bool lockEnd = false;
    if (propertyMap["lockEnd"].isValid()) {
        lockEnd = propertyMap["lockEnd"].toBool();
    }

    bool distanceScaleEnd = false;
    if (propertyMap["distanceScaleEnd"].isValid()) {
        distanceScaleEnd = propertyMap["distanceScaleEnd"].toBool();
    }

    bool scaleWithParent = true;
    if (propertyMap["scaleWithParent"].isValid()) {
        scaleWithParent = propertyMap["scaleWithParent"].toBool();
    } else if (propertyMap["scaleWithAvatar"].isValid()) {
        scaleWithParent = propertyMap["scaleWithAvatar"].toBool();
    }

    bool followNormal = false;
    if (propertyMap["followNormal"].isValid()) {
        followNormal = propertyMap["followNormal"].toBool();
    }
    float followNormalStrength = 0.0f;
    if (propertyMap["followNormalStrength"].isValid()) {
        followNormalStrength = propertyMap["followNormalStrength"].toFloat();
    }

    bool enabled = false;
    if (propertyMap["enabled"].isValid()) {
        enabled = propertyMap["enabled"].toBool();
    }

    RenderStateMap renderStates;
    if (propertyMap["renderStates"].isValid()) {
        QList<QVariant> renderStateVariants = propertyMap["renderStates"].toList();
        for (const QVariant& renderStateVariant : renderStateVariants) {
            if (renderStateVariant.isValid()) {
                QVariantMap renderStateMap = renderStateVariant.toMap();
                if (renderStateMap["name"].isValid()) {
                    std::string name = renderStateMap["name"].toString().toStdString();
                    renderStates[name] = ParabolaPointer::buildRenderState(renderStateMap);
                }
            }
        }
    }

    DefaultRenderStateMap defaultRenderStates;
    if (propertyMap["defaultRenderStates"].isValid()) {
        QList<QVariant> renderStateVariants = propertyMap["defaultRenderStates"].toList();
        for (const QVariant& renderStateVariant : renderStateVariants) {
            if (renderStateVariant.isValid()) {
                QVariantMap renderStateMap = renderStateVariant.toMap();
                if (renderStateMap["name"].isValid() && renderStateMap["distance"].isValid()) {
                    std::string name = renderStateMap["name"].toString().toStdString();
                    float distance = renderStateMap["distance"].toFloat();
                    defaultRenderStates[name] = std::pair<float, std::shared_ptr<StartEndRenderState>>(distance, ParabolaPointer::buildRenderState(renderStateMap));
                }
            }
        }
    }

    bool hover = false;
    if (propertyMap["hover"].isValid()) {
        hover = propertyMap["hover"].toBool();
    }

    PointerTriggers triggers;
    auto userInputMapper = DependencyManager::get<UserInputMapper>();
    if (propertyMap["triggers"].isValid()) {
        QList<QVariant> triggerVariants = propertyMap["triggers"].toList();
        for (const QVariant& triggerVariant : triggerVariants) {
            if (triggerVariant.isValid()) {
                QVariantMap triggerMap = triggerVariant.toMap();
                if (triggerMap["action"].isValid() && triggerMap["button"].isValid()) {
                    controller::Endpoint::Pointer endpoint = userInputMapper->endpointFor(controller::Input(triggerMap["action"].toUInt()));
                    if (endpoint) {
                        std::string button = triggerMap["button"].toString().toStdString();
                        triggers.emplace_back(endpoint, button);
                    }
                }
            }
        }
    }

    return std::make_shared<ParabolaPointer>(properties, renderStates, defaultRenderStates, hover, triggers,
                                             faceAvatar, followNormal, followNormalStrength, centerEndY, lockEnd, distanceScaleEnd,
                                             scaleWithParent, enabled);
}

void PointerScriptingInterface::editRenderState(unsigned int uid, const QString& renderState, const QVariant& properties) const {
    QVariantMap propMap = properties.toMap();

    QVariant startProps;
    if (propMap["start"].isValid()) {
        startProps = propMap["start"];
    }

    QVariant pathProps;
    if (propMap["path"].isValid()) {
        pathProps = propMap["path"];
    }

    QVariant endProps;
    if (propMap["end"].isValid()) {
        endProps = propMap["end"];
    }

    DependencyManager::get<PointerManager>()->editRenderState(uid, renderState.toStdString(), startProps, pathProps, endProps);
}

QVariantMap PointerScriptingInterface::getPrevPickResult(unsigned int uid) const { 
    QVariantMap result;
    auto pickResult = DependencyManager::get<PointerManager>()->getPrevPickResult(uid);
    if (pickResult) {
        result = pickResult->toVariantMap();
    }
    return result;
}
