"use strict";

//  webSurfaceLaserInput.js
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

/* global Script, Entities, enableDispatcherModule, disableDispatcherModule, makeRunningValues,
   makeDispatcherModuleParameters, Overlays, HMD, TRIGGER_ON_VALUE, TRIGGER_OFF_VALUE, getEnabledModuleByName,
   Picks, makeLaserParams, Settings, MyAvatar, RIGHT_HAND, LEFT_HAND, DISPATCHER_PROPERTIES
*/

Script.include("/~/system/libraries/controllerDispatcherUtils.js");
Script.include("/~/system/libraries/controllers.js");

(function() {
    const intersectionType = {
        None: 0,
        WebOverlay: 1,
        WebEntity: 2,
        HifiKeyboard: 3,
        Overlay: 4,
        HifiTablet: 5,
    };

    function WebSurfaceLaserInput(hand) {
        this.hand = hand;
        this.otherHand = this.hand === RIGHT_HAND ? LEFT_HAND : RIGHT_HAND;
        this.running = false;
        this.ignoredObjects = [];
        this.intersectedType = intersectionType["None"];

        this.parameters = makeDispatcherModuleParameters(
            160,
            this.hand === RIGHT_HAND ? ["rightHand"] : ["leftHand"],
            [],
            100,
            makeLaserParams(hand, true));

        this.getFarGrab = function () {
            return getEnabledModuleByName(this.hand === RIGHT_HAND ? ("RightFarGrabEntity") : ("LeftFarGrabEntity"));
        };

        this.farGrabActive = function () {
            var farGrab = this.getFarGrab();
            // farGrab will be null if module isn't loaded.
            if (farGrab) {
                return farGrab.targetIsNull();
            } else {
                return false;
            }
        };

        this.grabModuleWantsNearbyOverlay = function(controllerData) {
            if (controllerData.triggerValues[this.hand] > TRIGGER_ON_VALUE || controllerData.secondaryValues[this.hand] > BUMPER_ON_VALUE) {
                var nearGrabName = this.hand === RIGHT_HAND ? "RightNearParentingGrabOverlay" : "LeftNearParentingGrabOverlay";
                var nearGrabModule = getEnabledModuleByName(nearGrabName);
                if (nearGrabModule) {
                    var candidateOverlays = controllerData.nearbyOverlayIDs[this.hand];
                    var grabbableOverlays = candidateOverlays.filter(function(overlayID) {
                        return Overlays.getProperty(overlayID, "grabbable");
                    });
                    var target = nearGrabModule.getTargetID(grabbableOverlays, controllerData);
                    if (target) {
                        return true;
                    }
                }
                nearGrabName = this.hand === RIGHT_HAND ? "RightNearParentingGrabEntity" : "LeftNearParentingGrabEntity";
                nearGrabModule = getEnabledModuleByName(nearGrabName);
                if (nearGrabModule && nearGrabModule.isReady(controllerData)) {
                    // check for if near parent module is active.
                    var isNearGrabModuleActive = nearGrabModule.isReady(controllerData).active;
                    if (isNearGrabModuleActive) {
                        // if true, return true.
                        return isNearGrabModuleActive;
                    } else {
                        // check near action grab entity as a second pass.
                        nearGrabName = this.hand === RIGHT_HAND ? "RightNearActionGrabEntity" : "LeftNearActionGrabEntity";
                        nearGrabModule = getEnabledModuleByName(nearGrabName);
                        if (nearGrabModule && nearGrabModule.isReady(controllerData)) {
                            return nearGrabModule.isReady(controllerData).active;
                        }
                    }
                }
            }

            var nearTabletHighlightModule = getEnabledModuleByName(this.hand === RIGHT_HAND
                ? "RightNearTabletHighlight" : "LeftNearTabletHighlight");
            if (nearTabletHighlightModule) {
                return nearTabletHighlightModule.isNearTablet(controllerData);
            }

            return false;
        };

        this.getOtherModule = function() {
            return this.hand === RIGHT_HAND ? leftOverlayLaserInput : rightOverlayLaserInput;
        };

        this.addObjectToIgnoreList = function(controllerData) {
            if (Window.interstitialModeEnabled && !Window.isPhysicsEnabled()) {
                var intersection = controllerData.rayPicks[this.hand];
                var objectID = intersection.objectID;

                if (intersection.type === Picks.INTERSECTED_OVERLAY) {
                    var overlayIndex = this.ignoredObjects.indexOf(objectID);

                    var overlayName = Overlays.getProperty(objectID, "name");
                    if (overlayName !== "Loading-Destination-Card-Text" && overlayName !== "Loading-Destination-Card-GoTo-Image" &&
                        overlayName !== "Loading-Destination-Card-GoTo-Image-Hover") {
                        var data = {
                            action: 'add',
                            id: objectID
                        };
                        Messages.sendMessage('Hifi-Hand-RayPick-Blacklist', JSON.stringify(data));
                        this.ignoredObjects.push(objectID);
                    }
                } else if (intersection.type === Picks.INTERSECTED_ENTITY) {
                    var entityIndex = this.ignoredObjects.indexOf(objectID);
                    var data = {
                        action: 'add',
                        id: objectID
                    };
                    Messages.sendMessage('Hifi-Hand-RayPick-Blacklist', JSON.stringify(data));
                    this.ignoredObjects.push(objectID);
                }
            }
        };

        this.restoreIgnoredObjects = function() {
            for (var index = 0; index < this.ignoredObjects.length; index++) {
                var data = {
                    action: 'remove',
                    id: this.ignoredObjects[index]
                };
                Messages.sendMessage('Hifi-Hand-RayPick-Blacklist', JSON.stringify(data));
            }

            this.ignoredObjects = [];
        };

        this.getInteractableType = function(controllerData, triggerPressed, checkEntitiesOnly) {
            // allow pointing at tablet, unlocked web entities, or web overlays automatically without pressing trigger,
            // but for pointing at locked web entities or non-web overlays user must be pressing trigger
            var intersection = controllerData.rayPicks[this.hand];
            var objectID = intersection.objectID;
            if (intersection.type === Picks.INTERSECTED_OVERLAY && !checkEntitiesOnly) {
                if ((HMD.tabletID && objectID === HMD.tabletID) ||
                    (HMD.tabletScreenID && objectID === HMD.tabletScreenID) ||
                    (HMD.homeButtonID && objectID === HMD.homeButtonID)) {
                    return intersectionType["HifiTablet"];
                } else {
                    var overlayType = Overlays.getOverlayType(objectID);
                    var type = intersectionType["None"];
                    if (Keyboard.containsID(objectID) && !Keyboard.preferMalletsOverLasers) {
                        type = intersectionType["HifiKeyboard"];
                    } else if (overlayType === "web3d") {
                        type = intersectionType["WebOverlay"];
                    } else if (triggerPressed) {
                        type = intersectionType["Overlay"];
                    }

                    return type;
                }
            } else if (intersection.type === Picks.INTERSECTED_ENTITY) {
                var entityProperties = Entities.getEntityProperties(objectID, DISPATCHER_PROPERTIES);
                var entityType = entityProperties.type;
                var isLocked = entityProperties.locked;
                if (entityType === "Web" && (!isLocked || triggerPressed)) {
                    return intersectionType["WebEntity"];
                }
            }
            return intersectionType["None"];
        };

        this.updateAlwaysOn = function(type) {
            var PREFER_STYLUS_OVER_LASER = "preferStylusOverLaser";
            this.parameters.handLaser.alwaysOn = (!Settings.getValue(PREFER_STYLUS_OVER_LASER, false) || type === intersectionType["HifiKeyboard"]);
        };

        this.getDominantHand = function() {
            return MyAvatar.getDominantHand() === "right" ? 1 : 0;
        };

        this.dominantHandOverride = false;

        this.isReady = function (controllerData) {
            // Trivial rejection for when FarGrab is active.
            if (this.farGrabActive()) {
                return makeRunningValues(false, [], []);
            }

            var isTriggerPressed = controllerData.triggerValues[this.hand] > TRIGGER_OFF_VALUE &&
                                   controllerData.triggerValues[this.otherHand] <= TRIGGER_OFF_VALUE;
            var type = this.getInteractableType(controllerData, isTriggerPressed, false);

            if (type !== intersectionType["None"] && !this.grabModuleWantsNearbyOverlay(controllerData)) {
                if (type === intersectionType["WebOverlay"] || type === intersectionType["WebEntity"] || type === intersectionType["HifiTablet"]) {
                    var otherModuleRunning = this.getOtherModule().running;
                    otherModuleRunning = otherModuleRunning && this.getDominantHand() !== this.hand; // Auto-swap to dominant hand.
                    var allowThisModule = !otherModuleRunning || isTriggerPressed;

                    if (!allowThisModule) {
                        return makeRunningValues(true, [], []);
                    }

                    if (isTriggerPressed) {
                        this.dominantHandOverride = true; // Override dominant hand.
                        this.getOtherModule().dominantHandOverride = false;
                    }
                }

                this.updateAlwaysOn(type);
                if (this.parameters.handLaser.alwaysOn || isTriggerPressed) {
                    return makeRunningValues(true, [], []);
                }
            }

            if (Window.interstitialModeEnabled && Window.isPhysicsEnabled()) {
                this.restoreIgnoredObjects();
            }
            return makeRunningValues(false, [], []);
        };

        this.shouldThisModuleRun = function(controllerData) {
            var otherModuleRunning = this.getOtherModule().running;
            otherModuleRunning = otherModuleRunning && this.getDominantHand() !== this.hand; // Auto-swap to dominant hand.
            otherModuleRunning = otherModuleRunning || this.getOtherModule().dominantHandOverride; // Override dominant hand.
            var grabModuleNeedsToRun = this.grabModuleWantsNearbyOverlay(controllerData);
            // only allow for non-near grab
            return !otherModuleRunning && !grabModuleNeedsToRun;
        };

        this.run = function(controllerData, deltaTime) {
            this.addObjectToIgnoreList(controllerData);
            var isTriggerPressed = controllerData.triggerValues[this.hand] > TRIGGER_OFF_VALUE;
            var type = this.getInteractableType(controllerData, isTriggerPressed, false);
            var laserOn = isTriggerPressed || this.parameters.handLaser.alwaysOn;
            this.addObjectToIgnoreList(controllerData);

            if (type === intersectionType["HifiTablet"] && laserOn) {
                if (this.shouldThisModuleRun(controllerData)) {
                    this.running = true;
                    return makeRunningValues(true, [], []);
                }
            } else if ((type === intersectionType["WebOverlay"] || type === intersectionType["WebEntity"]) && laserOn) { // auto laser on WebEntities andWebOverlays
                if (this.shouldThisModuleRun(controllerData)) {
                    this.running = true;
                    return makeRunningValues(true, [], []);
                }
            } else if ((type === intersectionType["HifiKeyboard"] && laserOn) || type === intersectionType["Overlay"]) {
                this.running = true;
                return makeRunningValues(true, [], []);
            }

            this.running = false;
            this.dominantHandOverride = false;
            return makeRunningValues(false, [], []);
        };
    }

    var leftOverlayLaserInput = new WebSurfaceLaserInput(LEFT_HAND);
    var rightOverlayLaserInput = new WebSurfaceLaserInput(RIGHT_HAND);

    enableDispatcherModule("LeftWebSurfaceLaserInput", leftOverlayLaserInput);
    enableDispatcherModule("RightWebSurfaceLaserInput", rightOverlayLaserInput);

    function cleanup() {
        disableDispatcherModule("LeftWebSurfaceLaserInput");
        disableDispatcherModule("RightWebSurfaceLaserInput");
    }
    Script.scriptEnding.connect(cleanup);
}());
