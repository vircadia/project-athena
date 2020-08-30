"use strict";

//  farActionGrabEntity.js
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

/* jslint bitwise: true */

/* global Script, Controller, RIGHT_HAND, LEFT_HAND, Mat4, MyAvatar, Vec3, Camera, Quat,
   getEnabledModuleByName, makeRunningValues, Entities,
   enableDispatcherModule, disableDispatcherModule, entityIsDistanceGrabbable, entityIsGrabbable,
   makeDispatcherModuleParameters, MSECS_PER_SEC, HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION,
   TRIGGER_OFF_VALUE, TRIGGER_ON_VALUE, ZERO_VEC, ensureDynamic,
   getControllerWorldLocation, projectOntoEntityXYPlane, HMD,
   Picks, makeLaserLockInfo, makeLaserParams, AddressManager, getEntityParents, Selection, DISPATCHER_HOVERING_LIST,
   worldPositionToRegistrationFrameMatrix, DISPATCHER_PROPERTIES, Uuid, Picks, handsAreTracked, Messages
*/

Script.include("/~/system/libraries/controllerDispatcherUtils.js");
Script.include("/~/system/libraries/controllers.js");

(function() {

    var MARGIN = 25;

    function TargetObject(entityID, entityProps) {
        this.entityID = entityID;
        this.entityProps = entityProps;
        this.targetEntityID = null;
        this.targetEntityProps = null;
        this.previousCollisionStatus = null;
        this.madeDynamic = null;

        this.makeDynamic = function() {
            if (this.targetEntityID) {
                var newProps = {
                    dynamic: true,
                    collisionless: true
                };
                this.previousCollisionStatus = this.targetEntityProps.collisionless;
                Entities.editEntity(this.targetEntityID, newProps);
                this.madeDynamic = true;
            }
        };

        this.restoreTargetEntityOriginalProps = function() {
            if (this.madeDynamic) {
                var props = {};
                props.dynamic = false;
                props.collisionless = this.previousCollisionStatus;
                var zeroVector = {x: 0, y: 0, z:0};
                props.localVelocity = zeroVector;
                props.localRotation = zeroVector;
                Entities.editEntity(this.targetEntityID, props);
            }
        };

        this.getTargetEntity = function() {
            var parentPropsLength = this.parentProps.length;
            if (parentPropsLength !== 0) {
                var targetEntity = {
                    id: this.parentProps[parentPropsLength - 1].id,
                    props: this.parentProps[parentPropsLength - 1]};
                this.targetEntityID = targetEntity.id;
                this.targetEntityProps = targetEntity.props;
                return targetEntity;
            }
            this.targetEntityID = this.entityID;
            this.targetEntityProps = this.entityProps;
            return {
                id: this.entityID,
                props: this.entityProps};
        };
    }

    function FarActionGrabEntity(hand) {
        this.hand = hand;
        this.grabbedThingID = null;
        this.targetObject = null;
        this.actionID = null; // action this script created...
        this.entityToLockOnto = null;
        this.locked = false;
        this.reticleMinX = MARGIN;
        this.reticleMaxX = null;
        this.reticleMinY = MARGIN;
        this.reticleMaxY = null;

        this.ignoredEntities = [];

        var ACTION_TTL = 15; // seconds

        var DISTANCE_HOLDING_RADIUS_FACTOR = 3.5; // multiplied by distance between hand and object
        var DISTANCE_HOLDING_ACTION_TIMEFRAME = 0.1; // how quickly objects move to their new position
        var DISTANCE_HOLDING_UNITY_MASS = 1200; //  The mass at which the distance holding action timeframe is unmodified
        var DISTANCE_HOLDING_UNITY_DISTANCE = 6; //  The distance at which the distance holding action timeframe is unmodified

        this.parameters = makeDispatcherModuleParameters(
            550,
            this.hand === RIGHT_HAND ? ["rightHand"] : ["leftHand"],
            [],
            100,
            makeLaserParams(this.hand, false));


        this.handToController = function() {
            return (this.hand === RIGHT_HAND) ? Controller.Standard.RightHand : Controller.Standard.LeftHand;
        };

        this.distanceGrabTimescale = function(mass, distance) {
            var timeScale = DISTANCE_HOLDING_ACTION_TIMEFRAME * mass /
                DISTANCE_HOLDING_UNITY_MASS * distance /
                DISTANCE_HOLDING_UNITY_DISTANCE;
            if (timeScale < DISTANCE_HOLDING_ACTION_TIMEFRAME) {
                timeScale = DISTANCE_HOLDING_ACTION_TIMEFRAME;
            }
            return timeScale;
        };

        this.getMass = function(dimensions, density) {
            return (dimensions.x * dimensions.y * dimensions.z) * density;
        };

        this.startFarGrabAction = function (controllerData, grabbedProperties) {
            var controllerLocation = controllerData.controllerLocations[this.hand];
            var worldControllerPosition = controllerLocation.position;
            var worldControllerRotation = controllerLocation.orientation;

            // transform the position into room space
            var worldToSensorMat = Mat4.inverse(MyAvatar.getSensorToWorldMatrix());
            var roomControllerPosition = Mat4.transformPoint(worldToSensorMat, worldControllerPosition);

            var now = Date.now();

            // add the action and initialize some variables
            this.currentObjectPosition = grabbedProperties.position;
            this.currentObjectRotation = grabbedProperties.rotation;
            this.currentObjectTime = now;
            this.currentCameraOrientation = Camera.orientation;

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
            this.mass = this.getMass(grabbedProperties.dimensions, grabbedProperties.density);
            var distanceToObject = Vec3.length(Vec3.subtract(MyAvatar.position, grabbedProperties.position));
            var timeScale = this.distanceGrabTimescale(this.mass, distanceToObject);
            this.linearTimeScale = timeScale;
            this.actionID = Entities.addAction("far-grab", this.grabbedThingID, {
                targetPosition: this.currentObjectPosition,
                linearTimeScale: timeScale,
                targetRotation: this.currentObjectRotation,
                angularTimeScale: timeScale,
                tag: "far-grab-" + MyAvatar.sessionUUID,
                ttl: ACTION_TTL
            });
            if (this.actionID === Uuid.NULL) {
                this.actionID = null;
            }

            if (this.actionID !== null) {
                var args = [this.hand === RIGHT_HAND ? "right" : "left", MyAvatar.sessionUUID];
                Entities.callEntityMethod(this.grabbedThingID, "startDistanceGrab", args);
            }

            Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, this.hand);
            this.previousRoomControllerPosition = roomControllerPosition;
        };

        this.continueDistanceHolding = function(controllerData) {
            var controllerLocation = controllerData.controllerLocations[this.hand];
            var worldControllerPosition = controllerLocation.position;
            var worldControllerRotation = controllerLocation.orientation;

            // also transform the position into room space
            var worldToSensorMat = Mat4.inverse(MyAvatar.getSensorToWorldMatrix());
            var roomControllerPosition = Mat4.transformPoint(worldToSensorMat, worldControllerPosition);

            var grabbedProperties = Entities.getEntityProperties(this.grabbedThingID, DISPATCHER_PROPERTIES);
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
            Entities.callEntityMethod(this.grabbedThingID, "continueDistanceGrab", args);

            //  Update radialVelocity
            var lastVelocity = Vec3.multiply(worldHandDelta, 1.0 / deltaObjectTime);
            var delta = Vec3.normalize(Vec3.subtract(grabbedProperties.position, worldControllerPosition));
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

            // XXX
            // this.maybeScale(grabbedProperties);

            var distanceToObject = Vec3.length(Vec3.subtract(MyAvatar.position, this.currentObjectPosition));

            this.linearTimeScale = (this.linearTimeScale / 2);
            if (this.linearTimeScale <= DISTANCE_HOLDING_ACTION_TIMEFRAME) {
                this.linearTimeScale = DISTANCE_HOLDING_ACTION_TIMEFRAME;
            }
            var success = Entities.updateAction(this.grabbedThingID, this.actionID, {
                targetPosition: newTargetPosition,
                linearTimeScale: this.linearTimeScale,
                targetRotation: this.currentObjectRotation,
                angularTimeScale: this.distanceGrabTimescale(this.mass, distanceToObject),
                ttl: ACTION_TTL
            });
            if (!success) {
                print("continueDistanceHolding -- updateAction failed: " + this.actionID);
                this.actionID = null;
            }

            this.previousRoomControllerPosition = roomControllerPosition;
        };

        this.endFarGrabAction = function () {
            ensureDynamic(this.grabbedThingID);
            this.distanceHolding = false;
            this.distanceRotating = false;
            Entities.deleteAction(this.grabbedThingID, this.actionID);

            var args = [this.hand === RIGHT_HAND ? "right" : "left", MyAvatar.sessionUUID];
            Entities.callEntityMethod(this.grabbedThingID, "releaseGrab", args);
            if (this.targetObject) {
                this.targetObject.restoreTargetEntityOriginalProps();
            }
            this.actionID = null;
            this.grabbedThingID = null;
            this.targetObject = null;
        };

        this.updateRecommendedArea = function() {
            var dims = Controller.getViewportDimensions();
            this.reticleMaxX = dims.x - MARGIN;
            this.reticleMaxY = dims.y - MARGIN;
        };

        this.calculateNewReticlePosition = function(intersection) {
            this.updateRecommendedArea();
            var point2d = HMD.overlayFromWorldPoint(intersection);
            point2d.x = Math.max(this.reticleMinX, Math.min(point2d.x, this.reticleMaxX));
            point2d.y = Math.max(this.reticleMinY, Math.min(point2d.y, this.reticleMaxY));
            return point2d;
        };

        this.restoreIgnoredEntities = function() {
            for (var i = 0; i < this.ignoredEntities.length; i++) {
                var data = {
                    action: 'remove',
                    id: this.ignoredEntities[i]
                };
                Messages.sendMessage('Hifi-Hand-RayPick-Blacklist', JSON.stringify(data));
            }
            this.ignoredEntities = [];
        };

        this.notPointingAtEntity = function(controllerData) {
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

        this.distanceRotate = function(otherFarGrabModule) {
            this.distanceRotating = true;
            this.distanceHolding = false;

            var worldControllerRotation = getControllerWorldLocation(this.handToController(), true).orientation;
            var controllerRotationDelta =
                Quat.multiply(worldControllerRotation, Quat.inverse(this.previousWorldControllerRotation));
            // Rotate entity by twice the delta rotation.
            controllerRotationDelta = Quat.multiply(controllerRotationDelta, controllerRotationDelta);

            // Perform the rotation in the translation controller's action update.
            otherFarGrabModule.currentObjectRotation = Quat.multiply(controllerRotationDelta,
                otherFarGrabModule.currentObjectRotation);

            this.previousWorldControllerRotation = worldControllerRotation;
        };

        this.prepareDistanceRotatingData = function(controllerData) {
            var intersection = controllerData.rayPicks[this.hand];

            var controllerLocation = getControllerWorldLocation(this.handToController(), true);
            var worldControllerPosition = controllerLocation.position;
            var worldControllerRotation = controllerLocation.orientation;

            var grabbedProperties = Entities.getEntityProperties(intersection.objectID, DISPATCHER_PROPERTIES);
            this.currentObjectPosition = grabbedProperties.position;
            this.grabRadius = intersection.distance;

            // Offset between controller vector at the grab radius and the entity position.
            var targetPosition = Vec3.multiply(this.grabRadius, Quat.getUp(worldControllerRotation));
            targetPosition = Vec3.sum(targetPosition, worldControllerPosition);
            this.offsetPosition = Vec3.subtract(this.currentObjectPosition, targetPosition);

            // Initial controller rotation.
            this.previousWorldControllerRotation = worldControllerRotation;
        };

        this.targetIsNull = function() {
            var properties = Entities.getEntityProperties(this.grabbedThingID, DISPATCHER_PROPERTIES);
            if (Object.keys(properties).length === 0 && this.distanceHolding) {
                return true;
            }
            return false;
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
                this.distanceRotating = false;

                if (controllerData.triggerValues[this.hand] > TRIGGER_ON_VALUE) {
                    this.prepareDistanceRotatingData(controllerData);
                    return makeRunningValues(true, [], []);
                } else {
                    return makeRunningValues(false, [], []);
                }
            }
            return makeRunningValues(false, [], []);
        };

        this.run = function (controllerData) {

            var intersection = controllerData.rayPicks[this.hand];
            if (intersection.type === Picks.INTERSECTED_ENTITY && !Window.isPhysicsEnabled()) {
                // add to ignored items.
                if (this.ignoredEntities.indexOf(intersection.objectID) === -1) {
                    var data = {
                        action: 'add',
                        id: intersection.objectID
                    };
                    Messages.sendMessage('Hifi-Hand-RayPick-Blacklist', JSON.stringify(data));
                    this.ignoredEntities.push(intersection.objectID);
                }
            }
            if (controllerData.triggerValues[this.hand] < TRIGGER_OFF_VALUE ||
                (this.notPointingAtEntity(controllerData) && Window.isPhysicsEnabled()) || this.targetIsNull()) {
                this.endFarGrabAction();
                this.restoreIgnoredEntities();
                return makeRunningValues(false, [], []);
            }
            this.intersectionDistance = controllerData.rayPicks[this.hand].distance;

            var otherModuleName =this.hand === RIGHT_HAND ? "LeftFarActionGrabEntity" : "RightFarActionGrabEntity";
            var otherFarGrabModule = getEnabledModuleByName(otherModuleName);

            // gather up the readiness of the near-grab modules
            var nearGrabNames = [
                this.hand === RIGHT_HAND ? "RightScaleAvatar" : "LeftScaleAvatar",
                this.hand === RIGHT_HAND ? "RightFarTriggerEntity" : "LeftFarTriggerEntity",
                this.hand === RIGHT_HAND ? "RightNearActionGrabEntity" : "LeftNearActionGrabEntity",
                this.hand === RIGHT_HAND ? "RightNearParentingGrabEntity" : "LeftNearParentingGrabEntity",
                this.hand === RIGHT_HAND ? "RightNearParentingGrabOverlay" : "LeftNearParentingGrabOverlay",
                this.hand === RIGHT_HAND ? "RightNearTabletHighlight" : "LeftNearTabletHighlight"
            ];

            var nearGrabReadiness = [];
            for (var i = 0; i < nearGrabNames.length; i++) {
                var nearGrabModule = getEnabledModuleByName(nearGrabNames[i]);
                var ready = nearGrabModule ? nearGrabModule.isReady(controllerData) : makeRunningValues(false, [], []);
                nearGrabReadiness.push(ready);
            }

            if (this.actionID) {
                // if we are doing a distance grab and the object or tablet gets close enough to the controller,
                // stop the far-grab so the near-grab or equip can take over.
                for (var k = 0; k < nearGrabReadiness.length; k++) {
                    if (nearGrabReadiness[k].active && (nearGrabReadiness[k].targets[0] === this.grabbedThingID ||
                        HMD.tabletID && nearGrabReadiness[k].targets[0] === HMD.tabletID)) {
                        this.endFarGrabAction();
                        this.restoreIgnoredEntities();
                        return makeRunningValues(false, [], []);
                    }
                }

                this.continueDistanceHolding(controllerData);
            } else {
                // if we are doing a distance search and this controller moves into a position
                // where it could near-grab something, stop searching.
                for (var j = 0; j < nearGrabReadiness.length; j++) {
                    if (nearGrabReadiness[j].active) {
                        this.endFarGrabAction();
                        this.restoreIgnoredEntities();
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
                            this.restoreIgnoredEntities();
                            return makeRunningValues(false, [], []);
                        }

                        this.targetObject = new TargetObject(entityID, targetProps);
                        this.targetObject.parentProps = getEntityParents(targetProps);

                        Selection.removeFromSelectedItemsList("contextOverlayHighlightList", "entity", entityID);

                        var targetEntity = this.targetObject.getTargetEntity();
                        entityID = targetEntity.id;
                        targetProps = targetEntity.props;

                        if (entityIsGrabbable(targetProps) || entityIsGrabbable(this.targetObject.entityProps)) {
                            if (!entityIsDistanceGrabbable(targetProps)) {
                                this.targetObject.makeDynamic();
                            }

                            if (!this.distanceRotating) {
                                this.grabbedThingID = entityID;
                                this.grabbedDistance = rayPickInfo.distance;
                            }

                            if (otherFarGrabModule.grabbedThingID === this.grabbedThingID &&
                                otherFarGrabModule.distanceHolding) {
                                this.prepareDistanceRotatingData(controllerData);
                                this.distanceRotate(otherFarGrabModule);
                            } else {
                                this.distanceHolding = true;
                                this.distanceRotating = false;
                                this.startFarGrabAction(controllerData, targetProps);
                            }
                        }
                    }
                } else if (this.distanceRotating) {
                    this.distanceRotate(otherFarGrabModule);
                }
            }
            return this.exitIfDisabled(controllerData);
        };

        this.exitIfDisabled = function(controllerData) {
            var moduleName = this.hand === RIGHT_HAND ? "RightDisableModules" : "LeftDisableModules";
            var disableModule = getEnabledModuleByName(moduleName);
            if (disableModule) {
                if (disableModule.disableModules) {
                    this.endFarGrabAction();
                    Selection.removeFromSelectedItemsList(DISPATCHER_HOVERING_LIST, "entity",
                        this.highlightedEntity);
                    this.highlightedEntity = null;
                    this.restoreIgnoredEntities();
                    return makeRunningValues(false, [], []);
                }
            }
            var grabbedThing = (this.distanceHolding || this.distanceRotating) ? this.targetObject.entityID : null;
            var offset = this.calculateOffset(controllerData);
            var laserLockInfo = makeLaserLockInfo(grabbedThing, false, this.hand, offset);
            return makeRunningValues(true, [], [], laserLockInfo);
        };

        this.calculateOffset = function(controllerData) {
            if (this.distanceHolding || this.distanceRotating) {
                var targetProps = Entities.getEntityProperties(this.targetObject.entityID,
                                                               [ "position", "rotation", "registrationPoint", "dimensions" ]);
                return worldPositionToRegistrationFrameMatrix(targetProps, controllerData.rayPicks[this.hand].intersection);
            }
            return undefined;
        };
    }

    var leftFarActionGrabEntity = new FarActionGrabEntity(LEFT_HAND);
    var rightFarActionGrabEntity = new FarActionGrabEntity(RIGHT_HAND);

    enableDispatcherModule("LeftFarActionGrabEntity", leftFarActionGrabEntity);
    enableDispatcherModule("RightFarActionGrabEntity", rightFarActionGrabEntity);

    function cleanup() {
        disableDispatcherModule("LeftFarActionGrabEntity");
        disableDispatcherModule("RightFarActionGrabEntity");
    }
    Script.scriptEnding.connect(cleanup);
}());
