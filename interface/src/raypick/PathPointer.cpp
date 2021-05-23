//
//  Created by Sam Gondelman 7/17/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "PathPointer.h"

#include "Application.h"
#include "avatar/AvatarManager.h"

#include <DependencyManager.h>
#include <PickManager.h>
#include <Quat.h>
#include "PickScriptingInterface.h"
#include "RayPick.h"

PathPointer::PathPointer(PickQuery::PickType type, const QVariant& rayProps, const RenderStateMap& renderStates, const DefaultRenderStateMap& defaultRenderStates,
                         bool hover, const PointerTriggers& triggers, bool faceAvatar, bool followNormal, float followNormalStrength, bool centerEndY, bool lockEnd,
                         bool distanceScaleEnd, bool scaleWithParent, bool enabled) :
    Pointer(DependencyManager::get<PickScriptingInterface>()->createPick(type, rayProps), enabled, hover),
    _renderStates(renderStates),
    _defaultRenderStates(defaultRenderStates),
    _triggers(triggers),
    _faceAvatar(faceAvatar),
    _followNormal(followNormal),
    _followNormalStrength(followNormalStrength),
    _centerEndY(centerEndY),
    _lockEnd(lockEnd),
    _distanceScaleEnd(distanceScaleEnd),
    _scaleWithParent(scaleWithParent)
{
    for (auto& state : _renderStates) {
        if (!enabled || state.first != _currentRenderState) {
            state.second->disable();
        }
    }
    for (auto& state : _defaultRenderStates) {
        if (!enabled || state.first != _currentRenderState) {
            state.second.second->disable();
        }
    }
}

PathPointer::~PathPointer() {
    for (auto& renderState : _renderStates) {
        renderState.second->cleanup();
    }
    for (auto& renderState : _defaultRenderStates) {
        renderState.second.second->cleanup();
    }
}

void PathPointer::setRenderState(const std::string& state) {
    withWriteLock([&] {
        if (!_currentRenderState.empty() && state != _currentRenderState) {
            auto renderState = _renderStates.find(_currentRenderState);
            if (renderState != _renderStates.end()) {
                renderState->second->disable();
            }
            auto defaultRenderState = _defaultRenderStates.find(_currentRenderState);
            if (defaultRenderState != _defaultRenderStates.end()) {
                defaultRenderState->second.second->disable();
            }
        }
        _currentRenderState = state;
    });
}

void PathPointer::setLength(float length) {
    withWriteLock([&] {
        _pathLength = length;
    });
}

void PathPointer::setLockEndUUID(const QUuid& objectID, bool isAvatar, const glm::mat4& offsetMat) {
    withWriteLock([&] {
        _lockEndObject.id = objectID;
        _lockEndObject.isAvatar = isAvatar;
        _lockEndObject.offsetMat = offsetMat;
    });
}

PickResultPointer PathPointer::getVisualPickResult(const PickResultPointer& pickResult) {
    PickResultPointer visualPickResult = pickResult;
    glm::vec3 origin = getPickOrigin(pickResult);
    IntersectionType type = getPickedObjectType(pickResult);
    QUuid id;
    glm::vec3 intersection;
    float distance;
    glm::vec3 surfaceNormal;

    if (type != IntersectionType::HUD) {
        glm::vec3 endVec;
        if (!_lockEndObject.id.isNull()) {
            glm::vec3 pos;
            glm::quat rot;
            glm::vec3 dim;
            glm::vec3 registrationPoint;
            // TODO: use isAvatar
            {
                EntityItemProperties props = DependencyManager::get<EntityScriptingInterface>()->getEntityProperties(_lockEndObject.id);
                glm::mat4 entityMat = createMatFromQuatAndPos(props.getRotation(), props.getPosition());
                glm::mat4 finalPosAndRotMat = entityMat * _lockEndObject.offsetMat;
                pos = extractTranslation(finalPosAndRotMat);
                rot = props.getRotation();
                dim = props.getDimensions();
                registrationPoint = props.getRegistrationPoint();
            }
            const glm::vec3 DEFAULT_REGISTRATION_POINT = glm::vec3(0.5f);
            endVec = pos + rot * (dim * (DEFAULT_REGISTRATION_POINT - registrationPoint));
            glm::vec3 direction = endVec - origin;
            distance = glm::distance(origin, endVec);
            glm::vec3 normalizedDirection = glm::normalize(direction);

            type = IntersectionType::ENTITY;
            id = _lockEndObject.id;
            intersection = endVec;
            surfaceNormal = -normalizedDirection;
            setVisualPickResultInternal(visualPickResult, type, id, intersection, distance, surfaceNormal);
        } else if (type != IntersectionType::NONE && _lockEnd) {
            id = getPickedObjectID(pickResult);
            if (type == IntersectionType::ENTITY) {
                endVec = DependencyManager::get<EntityScriptingInterface>()->getEntityTransform(id)[3];
            } else if (type == IntersectionType::AVATAR) {
                endVec = DependencyManager::get<AvatarHashMap>()->getAvatar(id)->getPosition();
            }
            glm::vec3 direction = endVec - origin;
            distance = glm::distance(origin, endVec);
            glm::vec3 normalizedDirection = glm::normalize(direction);
            intersection = endVec;
            surfaceNormal = -normalizedDirection;
            setVisualPickResultInternal(visualPickResult, type, id, intersection, distance, surfaceNormal);
        }
    }
    return visualPickResult;
}

void PathPointer::updateVisuals(const PickResultPointer& pickResult) {
    IntersectionType type = getPickedObjectType(pickResult);
    auto renderState = _renderStates.find(_currentRenderState);
    auto defaultRenderState = _defaultRenderStates.find(_currentRenderState);
    float parentScale = 1.0f;
    if (_enabled && _scaleWithParent) {
        glm::vec3 dimensions = DependencyManager::get<PickManager>()->getParentTransform(_pickUID).getScale();
        parentScale = glm::max(glm::max(dimensions.x, dimensions.y), dimensions.z);
    }

    if (_enabled && !_currentRenderState.empty() && renderState != _renderStates.end() &&
        (type != IntersectionType::NONE || _pathLength > 0.0f)) {
        glm::vec3 origin = getPickOrigin(pickResult);
        glm::vec3 end = getPickEnd(pickResult, _pathLength);
        glm::vec3 surfaceNormal = getPickedObjectNormal(pickResult);
        renderState->second->update(origin, end, surfaceNormal, parentScale, _distanceScaleEnd, _centerEndY, _faceAvatar,
                                    _followNormal, _followNormalStrength, _pathLength, pickResult);
        if (defaultRenderState != _defaultRenderStates.end() && defaultRenderState->second.second->isEnabled()) {
            defaultRenderState->second.second->disable();
        }
    } else if (_enabled && !_currentRenderState.empty() && defaultRenderState != _defaultRenderStates.end()) {
        if (renderState != _renderStates.end() && renderState->second->isEnabled()) {
            renderState->second->disable();
        }
        glm::vec3 origin = getPickOrigin(pickResult);
        glm::vec3 end = getPickEnd(pickResult, defaultRenderState->second.first);
        defaultRenderState->second.second->update(origin, end, Vectors::UP, parentScale, _distanceScaleEnd, _centerEndY,
                                                  _faceAvatar, _followNormal, _followNormalStrength, defaultRenderState->second.first, pickResult);
    } else if (!_currentRenderState.empty()) {
        if (renderState != _renderStates.end() && renderState->second->isEnabled()) {
            renderState->second->disable();
        }
        if (defaultRenderState != _defaultRenderStates.end() && defaultRenderState->second.second->isEnabled()) {
            defaultRenderState->second.second->disable();
        }
    }
}

void PathPointer::editRenderState(const std::string& state, const QVariant& startProps, const QVariant& pathProps, const QVariant& endProps) {
    withWriteLock([&] {
        auto renderState = _renderStates.find(state);
        if (renderState != _renderStates.end()) {
            updateRenderState(renderState->second->getStartID(), startProps);
            updateRenderState(renderState->second->getEndID(), endProps);
            QVariant startDim = startProps.toMap()["dimensions"];
            if (startDim.isValid()) {
                renderState->second->setStartDim(vec3FromVariant(startDim));
            }
            QVariant endDim = endProps.toMap()["dimensions"];
            if (endDim.isValid()) {
                renderState->second->setEndDim(vec3FromVariant(endDim));
            }
            QVariant rotation = endProps.toMap()["rotation"];
            if (rotation.isValid()) {
                renderState->second->setEndRot(quatFromVariant(rotation));
            }

            editRenderStatePath(state, pathProps);
        }
    });
}

void PathPointer::updateRenderState(const QUuid& id, const QVariant& props) {
    // FIXME: we have to keep using the Overlays interface here, because existing scripts use overlay properties to define pointers
    if (!id.isNull() && props.isValid()) {
        QVariantMap propMap = props.toMap();
        propMap.remove("visible");
        qApp->getOverlays().editOverlay(id, propMap);
    }
}

Pointer::PickedObject PathPointer::getHoveredObject(const PickResultPointer& pickResult) {
    return PickedObject(getPickedObjectID(pickResult), getPickedObjectType(pickResult));
}

Pointer::Buttons PathPointer::getPressedButtons(const PickResultPointer& pickResult) {
    std::unordered_set<std::string> toReturn;

    for (const PointerTrigger& trigger : _triggers) {
        std::string button = trigger.getButton();
        TriggerState& state = _states[button];
        // TODO: right now, LaserPointers don't support axes, only on/off buttons
        if (trigger.getEndpoint()->peek().value >= 1.0f) {
            toReturn.insert(button);

            if (_previousButtons.find(button) == _previousButtons.end()) {
                // start triggering for buttons that were just pressed
                state.triggeredObject = PickedObject(getPickedObjectID(pickResult), getPickedObjectType(pickResult));
                state.intersection = getPickEnd(pickResult);
                state.triggerPos2D = findPos2D(state.triggeredObject, state.intersection);
                state.triggerStartTime = usecTimestampNow();
                state.surfaceNormal = getPickedObjectNormal(pickResult);
                state.deadspotExpired = false;
                state.wasTriggering = true;
                state.triggering = true;
                _latestState = state;
            }
        } else {
            // stop triggering for buttons that aren't pressed
            state.wasTriggering = state.triggering;
            state.triggering = false;
            _latestState = state;
        }
    }
    _previousButtons = toReturn;
    return toReturn;
}

StartEndRenderState::StartEndRenderState(const QUuid& startID, const QUuid& endID) :
    _startID(startID), _endID(endID) {
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    if (!_startID.isNull()) {
        EntityPropertyFlags desiredProperties;
        desiredProperties += PROP_DIMENSIONS;
        desiredProperties += PROP_IGNORE_PICK_INTERSECTION;
        auto properties = entityScriptingInterface->getEntityProperties(_startID, desiredProperties);
        _startDim = properties.getDimensions();
        _startIgnorePicks = properties.getIgnorePickIntersection();
    }
    if (!_endID.isNull()) {
        EntityPropertyFlags desiredProperties;
        desiredProperties += PROP_DIMENSIONS;
        desiredProperties += PROP_ROTATION;
        desiredProperties += PROP_IGNORE_PICK_INTERSECTION;
        auto properties = entityScriptingInterface->getEntityProperties(_endID, desiredProperties);
        _endDim = properties.getDimensions();
        _endRot = properties.getRotation();
        _endIgnorePicks = properties.getIgnorePickIntersection();
    }
}

void StartEndRenderState::cleanup() {
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    if (!_startID.isNull()) {
        entityScriptingInterface->deleteEntity(_startID);
    }
    if (!_endID.isNull()) {
        entityScriptingInterface->deleteEntity(_endID);
    }
}

void StartEndRenderState::disable() {
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    if (!getStartID().isNull()) {
        EntityItemProperties properties;
        properties.setVisible(false);
        properties.setIgnorePickIntersection(true);
        entityScriptingInterface->editEntity(getStartID(), properties);
    }
    if (!getEndID().isNull()) {
        EntityItemProperties properties;
        properties.setVisible(false);
        properties.setIgnorePickIntersection(true);
        entityScriptingInterface->editEntity(getEndID(), properties);
    }
    _enabled = false;
}

void StartEndRenderState::update(const glm::vec3& origin, const glm::vec3& end, const glm::vec3& surfaceNormal, float parentScale, bool distanceScaleEnd, bool centerEndY,
                                 bool faceAvatar, bool followNormal, float followNormalStrength, float distance, const PickResultPointer& pickResult) {
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    if (!getStartID().isNull()) {
        EntityItemProperties properties;
        properties.setPosition(origin);
        properties.setVisible(true);
        properties.setDimensions(getStartDim() * parentScale);
        properties.setIgnorePickIntersection(doesStartIgnorePicks());
        entityScriptingInterface->editEntity(getStartID(), properties);
    }

    if (!getEndID().isNull()) {
        EntityItemProperties properties;
        EntityPropertyFlags desiredProperties;
        desiredProperties += PROP_DIMENSIONS;
        glm::vec3 dim;
        if (distanceScaleEnd) {
            dim = getEndDim() * glm::distance(origin, end);
        } else {
            dim = getEndDim() * parentScale;
        }
        properties.setDimensions(dim);

        glm::quat normalQuat = Quat().lookAtSimple(Vectors::ZERO, surfaceNormal);
        normalQuat = normalQuat * glm::quat(glm::vec3(-M_PI_2, 0, 0));
        glm::vec3 avatarUp = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldOrientation() * Vectors::UP;
        glm::quat rotation = glm::rotation(Vectors::UP, avatarUp);
        glm::vec3 position = end;
        if (!centerEndY) {
            if (followNormal) {
                position = end + 0.5f * dim.y * surfaceNormal;
            } else {
                position = end + 0.5f * dim.y * avatarUp;
            }
        }
        if (faceAvatar) {
            glm::quat orientation = followNormal ? normalQuat : DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldOrientation();
            glm::quat lookAtWorld = Quat().lookAt(position, DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition(), surfaceNormal);
            glm::quat lookAtModel = glm::inverse(orientation) * lookAtWorld;
            glm::quat lookAtFlatModel = Quat().cancelOutRollAndPitch(lookAtModel);
            glm::quat lookAtFlatWorld = orientation * lookAtFlatModel;
            rotation = lookAtFlatWorld;
        } else if (followNormal) {
            rotation = normalQuat;
        }
        if (followNormal && followNormalStrength > 0.0f && followNormalStrength < 1.0f) {
            if (!_avgEndRotInitialized) {
                _avgEndRot = rotation;
                _avgEndRotInitialized = true;
            } else {
                rotation = glm::slerp(_avgEndRot, rotation, followNormalStrength);
                if (!centerEndY) {
                    position = end + 0.5f * dim.y * (rotation * Vectors::UP);
                }
                _avgEndRot = rotation;
            }
        }
        properties.setPosition(position);
        properties.setRotation(rotation);
        properties.setVisible(true);
        properties.setIgnorePickIntersection(doesEndIgnorePicks());
        entityScriptingInterface->editEntity(getEndID(), properties);
    }
    _enabled = true;
}

glm::vec2 PathPointer::findPos2D(const PickedObject& pickedObject, const glm::vec3& origin) {
    switch (pickedObject.type) {
    case ENTITY:
    case LOCAL_ENTITY:
        return RayPick::projectOntoEntityXYPlane(pickedObject.objectID, origin);
    case HUD:
        return DependencyManager::get<PickManager>()->calculatePos2DFromHUD(origin);
    default:
        return glm::vec2(NAN);
    }
}
