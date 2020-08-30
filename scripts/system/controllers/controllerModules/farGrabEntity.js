"use strict";

//  farGrabEntity.js
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

/* jslint bitwise: true */

/* global Script, Controller, RIGHT_HAND, LEFT_HAND, Mat4, MyAvatar, Vec3, Quat, getEnabledModuleByName, makeRunningValues,
   Entities, enableDispatcherModule, disableDispatcherModule, entityIsGrabbable, makeDispatcherModuleParameters, MSECS_PER_SEC,
   HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, TRIGGER_OFF_VALUE, TRIGGER_ON_VALUE, ZERO_VEC,
   projectOntoEntityXYPlane, HMD, Picks, makeLaserLockInfo, makeLaserParams, AddressManager,
   getEntityParents, Selection, DISPATCHER_HOVERING_LIST, unhighlightTargetEntity, Messages, findGrabbableGroupParent,
   worldPositionToRegistrationFrameMatrix, DISPATCHER_PROPERTIES, handsAreTracked
*/

Script.include("/~/system/libraries/controllerDispatcherUtils.js");
Script.include("/~/system/libraries/controllers.js");

(function () {
    var MARGIN = 25;

    function TargetObject(entityID, entityProps) {
        this.entityID = entityID;
        this.entityProps = entityProps;
        this.targetEntityID = null;
        this.targetEntityProps = null;

        this.getTargetEntity = function () {
            var parentPropsLength = this.parentProps.length;
            if (parentPropsLength !== 0) {
                var targetEntity = {
                    id: this.parentProps[parentPropsLength - 1].id,
                    props: this.parentProps[parentPropsLength - 1]
                };
                this.targetEntityID = targetEntity.id;
                this.targetEntityProps = targetEntity.props;
                return targetEntity;
            }
            this.targetEntityID = this.entityID;
            this.targetEntityProps = this.entityProps;
            return {
                id: this.entityID,
                props: this.entityProps
            };
        };
    }

    function FarGrabEntity(hand) {
        this.hand = hand;
        this.grabbing = false;
        this.targetEntityID = null;
        this.targetObject = null;
        this.previouslyUnhooked = {};
        this.reticleMinX = MARGIN;
        this.reticleMaxX = 0;
        this.reticleMinY = MARGIN;
        this.reticleMaxY = 0;
        this.endedGrab = 0;
        this.MIN_HAPTIC_PULSE_INTERVAL = 500; // ms
        this.disabled = false;
        this.initialControllerRotation = Quat.IDENTITY;
        this.currentControllerRotation = Quat.IDENTITY;
        this.manipulating = false;
        this.wasManipulating = false;

        var FAR_GRAB_JOINTS = [65527, 65528]; // FARGRAB_LEFTHAND_INDEX, FARGRAB_RIGHTHAND_INDEX

        var DISTANCE_HOLDING_RADIUS_FACTOR = 3.5; // multiplied by distance between hand and object
        var DISTANCE_HOLDING_ACTION_TIMEFRAME = 0.1; // how quickly objects move to their new position
        var DISTANCE_HOLDING_UNITY_MASS = 1200; //  The mass at which the distance holding action timeframe is unmodified
        var DISTANCE_HOLDING_UNITY_DISTANCE = 6; //  The distance at which the distance holding action timeframe is unmodified

        this.parameters = makeDispatcherModuleParameters(
            540,
            this.hand === RIGHT_HAND ? ["rightHand"] : ["leftHand"],
            [],
            100,
            makeLaserParams(this.hand, false));

        this.getOtherModule = function () {
            return getEnabledModuleByName(this.hand === RIGHT_HAND ? ("LeftFarGrabEntity") : ("RightFarGrabEntity"));
        };

        // Get the rotation of the fargrabbed entity.
        this.getTargetRotation = function () {
            if (this.targetIsNull()) {
                return null;
            } else {
                var props = Entities.getEntityProperties(this.targetEntityID, ["rotation"]);
                return props.rotation;
            }
        };

        this.getOffhand = function () {
            return (this.hand === RIGHT_HAND ? LEFT_HAND : RIGHT_HAND);
        };

        // Activation criteria for rotating a fargrabbed entity. If we're changing the mapping, this is where to do it.
        this.shouldManipulateTarget = function (controllerData) {
            return (controllerData.triggerValues[this.getOffhand()] > TRIGGER_ON_VALUE || controllerData.secondaryValues[this.getOffhand()] > TRIGGER_ON_VALUE) ? true : false;
        };

        // Get the delta between the current rotation and where the controller was when manipulation started.
        this.calculateEntityRotationManipulation = function (controllerRotation) {
            return Quat.multiply(controllerRotation, Quat.inverse(this.initialControllerRotation));
        };

        this.setJointTranslation = function (newTargetPosLocal) {
            MyAvatar.setJointTranslation(FAR_GRAB_JOINTS[this.hand], newTargetPosLocal);
        };

        this.setJointRotation = function (newTargetRotLocal) {
            MyAvatar.setJointRotation(FAR_GRAB_JOINTS[this.hand], newTargetRotLocal);
        };

        this.setJointRotation = function (newTargetRotLocal) {
            MyAvatar.setJointRotation(FAR_GRAB_JOINTS[this.hand], newTargetRotLocal);
        };

        this.handToController = function () {
            return (this.hand === RIGHT_HAND) ? Controller.Standard.RightHand : Controller.Standard.LeftHand;
        };

        this.distanceGrabTimescale = function (mass, distance) {
            var timeScale = DISTANCE_HOLDING_ACTION_TIMEFRAME * mass /
                DISTANCE_HOLDING_UNITY_MASS * distance /
                DISTANCE_HOLDING_UNITY_DISTANCE;
            if (timeScale < DISTANCE_HOLDING_ACTION_TIMEFRAME) {
                timeScale = DISTANCE_HOLDING_ACTION_TIMEFRAME;
            }
            return timeScale;
        };

        this.getMass = function (dimensions, density) {
            return (dimensions.x * dimensions.y * dimensions.z) * density;
        };

        this.startFarGrabEntity = function (controllerData, targetProps) {
            var controllerLocation = controllerData.controllerLocations[this.hand];
            var worldControllerPosition = controllerLocation.position;
            var worldControllerRotation = controllerLocation.orientation;
            // transform the position into room space
            var worldToSensorMat = Mat4.inverse(MyAvatar.getSensorToWorldMatrix());
            var roomControllerPosition = Mat4.transformPoint(worldToSensorMat, worldControllerPosition);

            var now = Date.now();

            // add the action and initialize some variables
            this.currentObjectPosition = targetProps.position;
            this.currentObjectRotation = targetProps.rotation;
            this.currentObjectTime = now;

            this.grabRadius = this.grabbedDistance;
            this.grabRadialVelocity = 0.0;

            // offset between controller vector at the grab radius and the entity position
            var targetPosition = Vec3.multiply(this.grabRadius, Quat.getUp(worldControllerRotation));
            targetPosition = Vec3.sum(targetPosition, worldControllerPosition);
            this.offsetPosition = Vec3.subtract(this.currentObjectPosition, targetPosition);

            // compute a constant based on the initial conditions which we use below to exaggerate hand motion
            // onto the held object
            this.radiusScalar = Math.log(this.grabRadius + 1.0);
            if (this.radiusScalar < 1.0) {
                this.radiusScalar = 1.0;
            }

            // compute the mass for the purpose of energy and how quickly to move object
            this.mass = this.getMass(targetProps.dimensions, targetProps.density);

            // Debounce haptic pules. Can occur as near grab controller module vacillates between being ready or not due to
            // changing positions and floating point rounding.
            if (Date.now() - this.endedGrab > this.MIN_HAPTIC_PULSE_INTERVAL) {
                Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, this.hand);
            }

            unhighlightTargetEntity(this.targetEntityID);
            var message = {
                hand: this.hand,
                entityID: this.targetEntityID
            };

            Messages.sendLocalMessage('Hifi-unhighlight-entity', JSON.stringify(message));

            var newTargetPosLocal = MyAvatar.worldToJointPoint(targetProps.position);
            var newTargetRotLocal = targetProps.rotation;
            this.setJointTranslation(newTargetPosLocal);
            this.setJointRotation(newTargetRotLocal);

            var args = [this.hand === RIGHT_HAND ? "right" : "left", MyAvatar.sessionUUID];
            Entities.callEntityMethod(targetProps.id, "startDistanceGrab", args);

            this.targetEntityID = targetProps.id;


            if (this.grabID) {
                MyAvatar.releaseGrab(this.grabID);
            }
            var farJointIndex = FAR_GRAB_JOINTS[this.hand];
            this.grabID = MyAvatar.grab(targetProps.id, farJointIndex,
                Entities.worldToLocalPosition(targetProps.position, MyAvatar.SELF_ID, farJointIndex),
                Entities.worldToLocalRotation(targetProps.rotation, MyAvatar.SELF_ID, farJointIndex));

            Messages.sendMessage('Hifi-Object-Manipulation', JSON.stringify({
                action: 'grab',
                grabbedEntity: targetProps.id,
                joint: this.hand === RIGHT_HAND ? "RightHand" : "LeftHand"
            }));
            this.grabbing = true;

            this.previousRoomControllerPosition = roomControllerPosition;
        };

        this.continueDistanceHolding = function (controllerData) {
            var controllerLocation = controllerData.controllerLocations[this.hand];
            var worldControllerPosition = controllerLocation.position;
            var worldControllerRotation = controllerLocation.orientation;

            // also transform the position into room space
            var worldToSensorMat = Mat4.inverse(MyAvatar.getSensorToWorldMatrix());
            var roomControllerPosition = Mat4.transformPoint(worldToSensorMat, worldControllerPosition);

            var targetProps = Entities.getEntityProperties(this.targetEntityID, DISPATCHER_PROPERTIES);
            var now = Date.now();
            var deltaObjectTime = (now - this.currentObjectTime) / MSECS_PER_SEC; // convert to seconds
            this.currentObjectTime = now;

            // the action was set up when this.distanceHolding was called.  update the targets.
            var radius = Vec3.distance(this.currentObjectPosition, worldControllerPosition) *
                this.radiusScalar * DISTANCE_HOLDING_RADIUS_FACTOR;
            if (radius < 1.0) {
                radius = 1.0;
            }

            var roomHandDelta = Vec3.subtract(roomControllerPosition, this.previousRoomControllerPosition);
            var worldHandDelta = Mat4.transformVector(MyAvatar.getSensorToWorldMatrix(), roomHandDelta);
            var handMoved = Vec3.multiply(worldHandDelta, radius);
            this.currentObjectPosition = Vec3.sum(this.currentObjectPosition, handMoved);

            var args = [this.hand === RIGHT_HAND ? "right" : "left", MyAvatar.sessionUUID];
            Entities.callEntityMethod(this.targetEntityID, "continueDistanceGrab", args);

            //  Update radialVelocity
            var lastVelocity = Vec3.multiply(worldHandDelta, 1.0 / deltaObjectTime);
            var delta = Vec3.normalize(Vec3.subtract(targetProps.position, worldControllerPosition));
            var newRadialVelocity = Vec3.dot(lastVelocity, delta);

            var VELOCITY_AVERAGING_TIME = 0.016;
            var blendFactor = deltaObjectTime / VELOCITY_AVERAGING_TIME;
            if (blendFactor < 0.0) {
                blendFactor = 0.0;
            } else if (blendFactor > 1.0) {
                blendFactor = 1.0;
            }
            this.grabRadialVelocity = blendFactor * newRadialVelocity + (1.0 - blendFactor) * this.grabRadialVelocity;

            var RADIAL_GRAB_AMPLIFIER = 10.0;
            if (Math.abs(this.grabRadialVelocity) > 0.0) {
                this.grabRadius = this.grabRadius + (this.grabRadialVelocity * deltaObjectTime *
                    this.grabRadius * RADIAL_GRAB_AMPLIFIER);
            }

            // don't let grabRadius go all the way to zero, because it can't come back from that
            var MINIMUM_GRAB_RADIUS = 0.1;
            if (this.grabRadius < MINIMUM_GRAB_RADIUS) {
                this.grabRadius = MINIMUM_GRAB_RADIUS;
            }
            var newTargetPosition = Vec3.multiply(this.grabRadius, Quat.getUp(worldControllerRotation));
            newTargetPosition = Vec3.sum(newTargetPosition, worldControllerPosition);
            newTargetPosition = Vec3.sum(newTargetPosition, this.offsetPosition);

            var newTargetPosLocal = MyAvatar.worldToJointPoint(newTargetPosition);

            // This block handles the user's ability to rotate the object they're FarGrabbing
            if (this.shouldManipulateTarget(controllerData)) {
                // Get the pose of the controller that is not grabbing.
                var pose = Controller.getPoseValue((this.getOffhand() ? Controller.Standard.RightHand : Controller.Standard.LeftHand));
                if (pose.valid) {
                    // If we weren't manipulating the object yet, initialize the entity's original position.
                    if (!this.manipulating) {
                        // This will only be triggered if we've let go of the off-hand trigger and pulled it again without ending a grab.
                        // Need to poll the entity's rotation again here.
                        if (!this.wasManipulating) {
                            this.initialEntityRotation = this.getTargetRotation();
                        }
                        // Save the original controller orientation, we only care about the delta between this rotation and wherever
                        // the controller rotates, so that we can apply it to the entity's rotation.
                        this.initialControllerRotation = Quat.multiply(pose.rotation, MyAvatar.orientation);
                        this.manipulating = true;
                    }
                }

                var rot = Quat.multiply(pose.rotation, MyAvatar.orientation);
                var rotBetween = this.calculateEntityRotationManipulation(rot);
                var doubleRot = Quat.multiply(rotBetween, rotBetween);
                this.lastJointRotation = Quat.multiply(doubleRot, this.initialEntityRotation);
                this.setJointRotation(this.lastJointRotation);
            } else {
                // If we were manipulating but the user isn't currently expressing this intent, we want to know so we preserve the rotation
                // between manipulations without ending the fargrab.
                if (this.manipulating) {
                    this.initialEntityRotation = this.lastJointRotation;
                    this.wasManipulating = true;
                }
                this.manipulating = false;
                // Reset the inital controller position.
                this.initialControllerRotation = Quat.IDENTITY;
            }
            this.setJointTranslation(newTargetPosLocal);

            this.previousRoomControllerPosition = roomControllerPosition;
        };

        this.endFarGrabEntity = function (controllerData) {
            if (this.grabID) {
                MyAvatar.releaseGrab(this.grabID);
                this.grabID = null;
            }

            this.endedGrab = Date.now();

            var args = [this.hand === RIGHT_HAND ? "right" : "left", MyAvatar.sessionUUID];
            Entities.callEntityMethod(this.targetEntityID, "releaseGrab", args);
            Messages.sendMessage('Hifi-Object-Manipulation', JSON.stringify({
                action: 'release',
                grabbedEntity: this.targetEntityID,
                joint: this.hand === RIGHT_HAND ? "RightHand" : "LeftHand"
            }));
            unhighlightTargetEntity(this.targetEntityID);
            this.grabbing = false;
            MyAvatar.clearJointData(FAR_GRAB_JOINTS[this.hand]);
            this.initialEntityRotation = Quat.IDENTITY;
            this.initialControllerRotation = Quat.IDENTITY;
            this.targetEntityID = null;
            this.manipulating = false;
            this.wasManipulating = false;
            var otherModule = this.getOtherModule();
            otherModule.disabled = false;
        };

        this.updateRecommendedArea = function () {
            var dims = Controller.getViewportDimensions();
            this.reticleMaxX = dims.x - MARGIN;
            this.reticleMaxY = dims.y - MARGIN;
        };

        this.calculateNewReticlePosition = function (intersection) {
            this.updateRecommendedArea();
            var point2d = HMD.overlayFromWorldPoint(intersection);
            point2d.x = Math.max(this.reticleMinX, Math.min(point2d.x, this.reticleMaxX));
            point2d.y = Math.max(this.reticleMinY, Math.min(point2d.y, this.reticleMaxY));
            return point2d;
        };

        this.notPointingAtEntity = function (controllerData) {
            var intersection = controllerData.rayPicks[this.hand];
            var entityProperty = Entities.getEntityProperties(intersection.objectID, DISPATCHER_PROPERTIES);
            var entityType = entityProperty.type;
            var hudRayPick = controllerData.hudRayPicks[this.hand];
            var point2d = this.calculateNewReticlePosition(hudRayPick.intersection);
            if ((intersection.type === Picks.INTERSECTED_ENTITY && entityType === "Web") ||
                intersection.type === Picks.INTERSECTED_OVERLAY || Window.isPointOnDesktopWindow(point2d)) {
                return true;
            }
            return false;
        };

        this.targetIsNull = function () {
            var properties = Entities.getEntityProperties(this.targetEntityID, DISPATCHER_PROPERTIES);
            if (Object.keys(properties).length === 0 && this.distanceHolding) {
                return true;
            }
            return false;
        };

        this.getTargetProps = function (controllerData) {
            var targetEntity = controllerData.rayPicks[this.hand].objectID;
            if (targetEntity) {
                var gtProps = Entities.getEntityProperties(targetEntity, DISPATCHER_PROPERTIES);
                if (entityIsGrabbable(gtProps)) {
                    // if we've attempted to grab a child, roll up to the root of the tree
                    var groupRootProps = findGrabbableGroupParent(controllerData, gtProps);
                    if (entityIsGrabbable(groupRootProps)) {
                        return groupRootProps;
                    }
                    return gtProps;
                }
            }
            return null;
        };

        this.isReady = function (controllerData) {
            if (HMD.active) {
                if (handsAreTracked()) {
                    return makeRunningValues(false, [], []);
                }
                if (this.notPointingAtEntity(controllerData)) {
                    return makeRunningValues(false, [], []);
                }

                this.distanceHolding = false;

                if (controllerData.triggerValues[this.hand] > TRIGGER_ON_VALUE && !this.disabled) {
                    var otherModule = this.getOtherModule();
                    otherModule.disabled = true;
                    return makeRunningValues(true, [], []);
                }
            }
            return makeRunningValues(false, [], []);
        };

        this.run = function (controllerData) {
            if (controllerData.triggerValues[this.hand] < TRIGGER_OFF_VALUE || this.targetIsNull()) {
                this.endFarGrabEntity(controllerData);
                return makeRunningValues(false, [], []);
            }
            this.intersectionDistance = controllerData.rayPicks[this.hand].distance;

            // gather up the readiness of the near-grab modules
            var nearGrabNames = [
                this.hand === RIGHT_HAND ? "RightScaleAvatar" : "LeftScaleAvatar",
                this.hand === RIGHT_HAND ? "RightFarTriggerEntity" : "LeftFarTriggerEntity",
                this.hand === RIGHT_HAND ? "RightNearGrabEntity" : "LeftNearGrabEntity"
            ];
            if (!this.grabbing) {
                nearGrabNames.push(this.hand === RIGHT_HAND ? "RightNearParentingGrabOverlay" : "LeftNearParentingGrabOverlay");
                nearGrabNames.push(this.hand === RIGHT_HAND ? "RightNearTabletHighlight" : "LeftNearTabletHighlight");
            }

            var nearGrabReadiness = [];
            for (var i = 0; i < nearGrabNames.length; i++) {
                var nearGrabModule = getEnabledModuleByName(nearGrabNames[i]);
                var ready = nearGrabModule ? nearGrabModule.isReady(controllerData) : makeRunningValues(false, [], []);
                nearGrabReadiness.push(ready);
            }

            if (this.targetEntityID) {
                // if we are doing a distance grab and the object gets close enough to the controller,
                // stop the far-grab so the near-grab or equip can take over.
                for (var k = 0; k < nearGrabReadiness.length; k++) {
                    if (nearGrabReadiness[k].active && (nearGrabReadiness[k].targets[0] === this.targetEntityID)) {
                        this.endFarGrabEntity(controllerData);
                        return makeRunningValues(false, [], []);
                    }
                }

                this.continueDistanceHolding(controllerData);
            } else {
                // if we are doing a distance search and this controller moves into a position
                // where it could near-grab something, stop searching.
                for (var j = 0; j < nearGrabReadiness.length; j++) {
                    if (nearGrabReadiness[j].active) {
                        this.endFarGrabEntity(controllerData);
                        return makeRunningValues(false, [], []);
                    }
                }

                var rayPickInfo = controllerData.rayPicks[this.hand];
                if (rayPickInfo.type === Picks.INTERSECTED_ENTITY) {
                    if (controllerData.triggerClicks[this.hand]) {
                        var entityID = rayPickInfo.objectID;
                        var targetProps = Entities.getEntityProperties(entityID, DISPATCHER_PROPERTIES);
                        if (targetProps.href !== "") {
                            AddressManager.handleLookupString(targetProps.href);
                            return makeRunningValues(false, [], []);
                        }

                        this.targetObject = new TargetObject(entityID, targetProps);
                        this.targetObject.parentProps = getEntityParents(targetProps);

                        Selection.removeFromSelectedItemsList("contextOverlayHighlightList", "entity", entityID);

                        var targetEntity = this.targetObject.getTargetEntity();
                        entityID = targetEntity.id;
                        targetProps = targetEntity.props;

                        if (entityIsGrabbable(targetProps) || entityIsGrabbable(this.targetObject.entityProps)) {

                            this.targetEntityID = entityID;
                            this.grabbedDistance = rayPickInfo.distance;
                            this.distanceHolding = true;
                            this.startFarGrabEntity(controllerData, targetProps);
                        }
                    }
                }
            }
            return this.exitIfDisabled(controllerData);
        };

        this.exitIfDisabled = function (controllerData) {
            var moduleName = this.hand === RIGHT_HAND ? "RightDisableModules" : "LeftDisableModules";
            var disableModule = getEnabledModuleByName(moduleName);
            if (disableModule) {
                if (disableModule.disableModules) {
                    this.endFarGrabEntity(controllerData);
                    Selection.removeFromSelectedItemsList(DISPATCHER_HOVERING_LIST, "entity", this.highlightedEntity);
                    this.highlightedEntity = null;
                    return makeRunningValues(false, [], []);
                }
            }
            var grabbedThing = this.distanceHolding ? this.targetObject.entityID : null;
            var offset = this.calculateOffset(controllerData);
            var laserLockInfo = makeLaserLockInfo(grabbedThing, false, this.hand, offset);
            return makeRunningValues(true, [], [], laserLockInfo);
        };

        this.calculateOffset = function (controllerData) {
            if (this.distanceHolding) {
                var targetProps = Entities.getEntityProperties(this.targetObject.entityID,
                    ["position", "rotation", "registrationPoint", "dimensions"]);
                return worldPositionToRegistrationFrameMatrix(targetProps, controllerData.rayPicks[this.hand].intersection);
            }
            return undefined;
        };
    }

    var leftFarGrabEntity = new FarGrabEntity(LEFT_HAND);
    var rightFarGrabEntity = new FarGrabEntity(RIGHT_HAND);

    enableDispatcherModule("LeftFarGrabEntity", leftFarGrabEntity);
    enableDispatcherModule("RightFarGrabEntity", rightFarGrabEntity);

    function cleanup() {
        disableDispatcherModule("LeftFarGrabEntity");
        disableDispatcherModule("RightFarGrabEntity");
    }
    Script.scriptEnding.connect(cleanup);
}());
