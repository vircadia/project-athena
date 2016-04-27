"use strict";
/*jslint vars: true, plusplus: true*/
/*globals Script, Overlays, Controller, Reticle, HMD, Camera, Entities, MyAvatar, Settings, Menu, ScriptDiscoveryService, Window, Vec3, Quat, print */

//
//  handControllerPointer.js
//  examples/controllers
//
//  Created by Howard Stearns on 2016/04/22
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

print('handControllerPointer version', 10);

// Control the "mouse" using hand controller. (HMD and desktop.)
// For now:
// Button 3 is left-mouse, button 4 is right-mouse.
// First-person only.
// Right hand only.
// Partial trigger squeeze toggles a laser visualization. When on, you can also click on objects in-world, not just HUD.
// On Windows, the upper left corner of Interface must be in the upper left corner of the screen, and the title bar must be 50px high. (System bug.)
//
// Bugs:
// Don't turn off hand controllers on simulated click (only on real mouse click).
// Turn in-world click off when moving by hand controller.
// Trigger toggle is flakey.
// When clicking on in-world objects, the click acts on the red ball, not the termination of the blue line.

function checkForDepthReticleScript() {
    ScriptDiscoveryService.getRunning().forEach(function (script) {
        if (script.name === 'depthReticle.js') {
            Window.alert('Please shut down depthReticle script.\n' + script.path +
                         '\nMost of the behavior is included here in\n' +
                         Script.resolvePath(''));
            // Some current deviations are listed below as fixmes.
        }
    });
}


// UTILITIES -------------
//
var counter = 0, skip = 0; //fixme 50;
function debug() { // Display the arguments not just [Object object].
    if (skip && (counter++ % skip)) { return; }
    print.apply(null, [].map.call(arguments, JSON.stringify));
}

// Utility to make it easier to setup and disconnect cleanly.
function setupHandler(event, handler) {
    event.connect(handler);
    Script.scriptEnding.connect(function () { event.disconnect(handler); });
}
// If some capability is not available until expiration milliseconds after the last update.
function TimeLock(expiration) {
    var last = 0;
    this.update = function (optionalNow) {
        last = optionalNow || Date.now();
    };
    this.expired = function (optionalNow) {
        return ((optionalNow || Date.now()) - last) > expiration;
    };
}
var handControllerLockOut = new TimeLock(2000);

// Calls onFunction() or offFunction() when swtich(on), but only if it is to a new value.
function LatchedToggle(onFunction, offFunction, state) {
    this.setState = function (on) {
        if (state === on) { return; }
        state = on;
        if (on) {
            onFunction();
        } else {
            offFunction();
        }
    };
}


// VERTICAL FIELD OF VIEW ---------
//
// Cache the verticalFieldOfView setting and update it every so often.
var verticalFieldOfView, DEFAULT_VERTICAL_FIELD_OF_VIEW = 45; // degrees
function updateFieldOfView() {
    verticalFieldOfView = Settings.getValue('fieldOfView') || DEFAULT_VERTICAL_FIELD_OF_VIEW;
}

// SHIMS ----------
//
// Define customizable versions of some standard operators. Alternative are at the bottom of the file.
var getControllerPose = Controller.getPoseValue;
var getValue = Controller.getValue;
var getOverlayAtPoint = Overlays.getOverlayAtPoint;
var setReticleVisible = function (on) { Reticle.visible = on; };

var weMovedReticle = false;
function handControllerMovedReticle() { // I.e., change in cursor position is from this script, not the mouse.
    // Only we know if we moved it, which is why this script has to replace depthReticle.js
    if (!weMovedReticle) { return false; }
    weMovedReticle = false;
    return true;
}
var setReticlePosition = function (point2d) {
    if (!HMD.active) {
        // FIX SYSEM BUG: On Windows, setPosition is setting relative to screen origin, not the content area of the window.
        point2d = {x: point2d.x, y: point2d.y + 50};
    }
    weMovedReticle = true;
    Reticle.setPosition(point2d);
};

// Generalizations of utilities that work with system and overlay elements.
function findRayIntersection(pickRay) {
    // Check 3D overlays and entities. Argument is an object with origin and direction.
    var result = Overlays.findRayIntersection(pickRay);
    if (!result.intersects) {
        result = Entities.findRayIntersection(pickRay, true);
    }
    return result;
}
function isPointingAtOverlay(optionalHudPosition2d) {
    return Reticle.pointingAtSystemOverlay || Overlays.getOverlayAtPoint(optionalHudPosition2d || Reticle.position);
}

// Generalized HUD utilities, with or without HMD:
// These two "vars" are for documentation. Do not change their values!
var SPHERICAL_HUD_DISTANCE = 1; // meters.
var PLANAR_PERPENDICULAR_HUD_DISTANCE = SPHERICAL_HUD_DISTANCE;
function calculateRayUICollisionPoint(position, direction) {
    // Answer the 3D intersection of the HUD by the given ray, or falsey if no intersection.
    if (HMD.active) {
        return HMD.calculateRayUICollisionPoint(position, direction);
    }
    // interect HUD plane, 1m in front of camera, using formula:
    //   scale = hudNormal dot (hudPoint - position) / hudNormal dot direction
    //   intersection = postion + scale*direction
    var hudNormal = Quat.getFront(Camera.getOrientation());
    var hudPoint = Vec3.sum(Camera.getPosition(), hudNormal); // must also scale if PLANAR_PERPENDICULAR_HUD_DISTANCE!=1
    var denominator = Vec3.dot(hudNormal, direction);
    if (denominator === 0) { return null; } // parallel to plane
    var numerator = Vec3.dot(hudNormal, Vec3.subtract(hudPoint, position));
    var scale = numerator / denominator;
    return Vec3.sum(position, Vec3.multiply(scale, direction));
}
var DEGREES_TO_HALF_RADIANS = Math.PI / 360;
function overlayFromWorldPoint(point) {
    // Answer the 2d pixel-space location in the HUD that covers the given 3D point.
    // REQUIRES: that the 3d point be on the hud surface!
    // Note that this is based on the Camera, and doesn't know anything about any
    // ray that may or may not have been used to compute the point. E.g., the
    // overlay point is NOT the intersection of some non-camera ray with the HUD.
    if (HMD.active) {
        return HMD.overlayFromWorldPoint(point);
    }
    var cameraToPoint = Vec3.subtract(point, Camera.getPosition());
    var cameraX = Vec3.dot(cameraToPoint, Quat.getRight(Camera.getOrientation()));
    var cameraY = Vec3.dot(cameraToPoint, Quat.getUp(Camera.getOrientation()));
    var size = Controller.getViewportDimensions();
    var hudHeight = 2 * Math.tan(verticalFieldOfView * DEGREES_TO_HALF_RADIANS); // must adjust if PLANAR_PERPENDICULAR_HUD_DISTANCE!=1
    var hudWidth = hudHeight * size.x / size.y;
    var horizontalFraction = (cameraX / hudWidth + 0.5);
    var verticalFraction = 1 - (cameraY / hudHeight + 0.5);
    var horizontalPixels = size.x * horizontalFraction;
    var verticalPixels = size.y * verticalFraction;
    return { x: horizontalPixels, y: verticalPixels };
}

// CONTROLLER MAPPING ---------
//
// Synthesize left and right mouse click from controller:
var MAPPING_NAME = Script.resolvePath('');
var mapping = Controller.newMapping(MAPPING_NAME);
function mapToAction(controller, button, action) {
    if (!Controller.Hardware[controller]) { return; } // FIXME: recheck periodically!
    mapping.from(Controller.Hardware[controller][button]).peek().to(action);
}
function handControllerClick(input) {
    if (!input) { return; } // We get both a down (with input 1) and up (with input 0)
    if (isPointingAtOverlay()) { print('OVERLAY CLICK'); return; }
    print('FIXME controller click');
}
mapToAction('Hydra', 'R3', Controller.Actions.ReticleClick); // handControllerClick);
mapToAction('Hydra', 'L3', handControllerClick);
mapToAction('Vive', 'LeftPrimaryThumb', handControllerClick);
mapToAction('Vive', 'RightPrimaryThumb', handControllerClick);
mapToAction('Hydra', 'R4', Controller.Actions.ContextMenu);
mapToAction('Hydra', 'L4', Controller.Actions.ContextMenu);
Script.scriptEnding.connect(mapping.disable);
mapping.enable();
//var toggleMap = new LatchedToggle(mapping.enable, mapping.disable);

// MOUSE ACTIVITY --------
//
var mouseCursorActivity = new TimeLock(5000);
var APPARENT_MAXIMUM_DEPTH = 100.0; // this is a depth at which things all seem sufficiently distant
function updateMouseActivity(isClick) {
    if (handControllerMovedReticle()) { return; }
    var now = Date.now();
    mouseCursorActivity.update(now);
    if (isClick) { return; } // FIXME: mouse clicks should keep going. Just not hand controller clicks
    handControllerLockOut.update(now);
    // Turn off mouse cursor after inactivity (as in depthReticle.js), and turn off hand controller mouse for a while.
    // FIXME: Does not yet seek to lookAt upon waking.
    // FIXME not unless Reticle.allowMouseCapture
    setReticleVisible(true);
}
function expireMouseCursor(now) {
    if (!isPointingAtOverlay() && mouseCursorActivity.expired(now)) {
        setReticleVisible(false);
    }
}
function onMouseMove() {
    // Display cursor at correct depth (as in depthReticle.js), and updateMouseActivity.
    if (handControllerMovedReticle()) { return; }

    if (HMD.active) { // set depth
        // FIXME: does not yet adjust slowly.
        if (isPointingAtOverlay()) {
            Reticle.depth = SPHERICAL_HUD_DISTANCE; // NOT CORRECT IF WE SWITCH TO OFFSET SPHERE!
        } else {
            var result = findRayIntersection(Camera.computePickRay(Reticle.position.x, Reticle.position.y));
            Reticle.depth = result.intersects ? result.distance : APPARENT_MAXIMUM_DEPTH;
        }
    }
    updateMouseActivity(); // After the above, just in case the depth movement is awkward when becoming visible.
}
function onMouseClick() {
    updateMouseActivity(true);
}
setupHandler(Controller.mouseMoveEvent, onMouseMove);
setupHandler(Controller.mousePressEvent, onMouseClick);
setupHandler(Controller.mouseDoublePressEvent, onMouseClick);


// VISUAL AID -----------
var LASER_COLOR = {red: 10, green: 10, blue: 255};
var laserLine = Overlays.addOverlay("line3d", { // same properties as handControllerGrab search line
    lineWidth: 5,
    // FIX SYSTEM BUG: If you don't supply a start and end at creation, it will never show up, even after editing.
    start: MyAvatar.position,
    end: Vec3.ZERO,
    color: LASER_COLOR,
    ignoreRayIntersection: true,
    visible: false,
    alpha: 1
});
var BALL_SIZE = 0.011;
var BALL_ALPHA = 0.5;
var laserBall = Overlays.addOverlay("sphere", { // Same properties as handControllerGrab search sphere
    size: BALL_SIZE,
    color: LASER_COLOR,
    ignoreRayIntersection: true,
    alpha: BALL_ALPHA,
    visible: false,
    solid: true,
    drawInFront: true // Even when burried inside of something, show it.
});
var fakeProjectionBall = Overlays.addOverlay("sphere", { // Same properties as handControllerGrab search sphere
    size: 5 * BALL_SIZE,
    color: {red: 255, green: 10, blue: 10},
    ignoreRayIntersection: true,
    alpha: BALL_ALPHA,
    visible: false,
    solid: true,
    drawInFront: true // Even when burried inside of something, show it.
});
var overlays = [laserBall, laserLine, fakeProjectionBall];
Script.scriptEnding.connect(function () { overlays.forEach(Overlays.deleteOverlay); });
var visualizationIsShowing = false; // Not whether it desired, but simply whether it is. Just an optimziation.
function turnOffLaser(optionalEnableClicks) {
    //toggleMap.setState(optionalEnableClicks);
    if (!optionalEnableClicks) { expireMouseCursor(); }
    if (!visualizationIsShowing) { return; }
    visualizationIsShowing = false;
    overlays.forEach(function (overlay) {
        Overlays.editOverlay(overlay, {visible: false});
    });
}
var MAX_RAY_SCALE = 32000; // Anything large. It's a scale, not a distance.
var wantsVisualization = false;
function updateLaser(controllerPosition, controllerDirection, hudPosition3d) {
    //toggleMap.setState(true);
    if (!wantsVisualization) { return false; }
    // Show the laser and intersect it with 3d overlays and entities.
    function intersection3d(position, direction) {
        var pickRay = {origin: position, direction: direction};
        var result = findRayIntersection(pickRay);
        return result.intersects ? result.intersection : Vec3.sum(position, Vec3.multiply(MAX_RAY_SCALE, direction));
    }
    var termination = intersection3d(controllerPosition, controllerDirection);
    visualizationIsShowing = true;
    setReticleVisible(false);
    Overlays.editOverlay(laserLine, {visible: true, start: controllerPosition, end: termination});
    // We show the ball at the hud intersection rather than at the termination because:
    // 1) As you swing the laser in space, it's hard to judge where it will intersect with a HUD element,
    //    unless the intersection of the laser with the HUD is marked. But it's confusing to do that
    //    with the pointer, so we use the ball.
    // 2) On some objects, the intersection is just enough inside the object that we're not going to see
    //    the ball anyway.
    Overlays.editOverlay(laserBall, {visible: true, position: hudPosition3d});

    // We really want in-world interactions to take place at termination:
    //   - We could do some of that with callEntityMethod (e.g., light switch entity script)
    //   - But we would have to alter edit.js to accept synthetic mouse data.
    // So for now, we present a false projection of the cursor onto whatever is below it. This is different from
    // the laser termination because the false projection is from the camera, while the laser termination is from the hand.
    var eye = Camera.getPosition();
    var falseProjection = intersection3d(eye, Vec3.subtract(hudPosition3d, eye));
    Overlays.editOverlay(fakeProjectionBall, {visible: true, position: falseProjection});
    return true;
}
var toggleLockout = new TimeLock(500);
function maybeToggleVisualization(trigger, now) {
    if (!trigger) { return; }
    if (toggleLockout.expired(now)) {
        wantsVisualization = !wantsVisualization;
        print('Toggled visualization', wantsVisualization ? 'on' : 'off');
    } else {
        toggleLockout.update(now);
    }
}

// MAIN OPERATIONS -----------
//
var FULL_TRIGGER_THRESHOLD = 0.9; // 0 to 1. Non-linear.
function update() {
    var now = Date.now();
    if (!handControllerLockOut.expired(now)) { return turnOffLaser(); } // Let them use mouse it in peace.
    if (!Menu.isOptionChecked("First Person")) { return turnOffLaser(); }  // What to do? menus can be behind hand!
    var trigger = getValue(Controller.Standard.RT);
    if (trigger > FULL_TRIGGER_THRESHOLD) { handControllerLockOut.update(now); return turnOffLaser(); } // Interferes with other scripts.
    maybeToggleVisualization(trigger, now);
    var hand = Controller.Standard.RightHand;
    var controllerPose = getControllerPose(hand);
    if (!controllerPose.valid) { wantsVisualization = false; return turnOffLaser(); } // Controller is cradled.
    var controllerPosition = Vec3.sum(Vec3.multiplyQbyV(MyAvatar.orientation, controllerPose.translation),
                                      MyAvatar.position);
    // This gets point direction right, but if you want general quaternion it would be more complicated:
    var controllerDirection = Quat.getUp(Quat.multiply(MyAvatar.orientation, controllerPose.rotation));

    var hudPoint3d = calculateRayUICollisionPoint(controllerPosition, controllerDirection);
    if (!hudPoint3d) { print('Controller is parallel to HUD'); return turnOffLaser(); }
    var hudPoint2d = overlayFromWorldPoint(hudPoint3d);
    // We don't know yet if we'll want to make the cursor visble, but we need to move it to see if
    // it's pointing at a QML tool (aka system overlay).
    setReticlePosition(hudPoint2d);

    // If there's a HUD element at the (newly moved) reticle, just make it visible and bail.
    if (isPointingAtOverlay(hudPoint2d)) {
        setReticleVisible(true);
        Reticle.depth = SPHERICAL_HUD_DISTANCE; // NOT CORRECT IF WE SWITCH TO OFFSET SPHERE!
        return turnOffLaser(true);
    }
    // We are not pointing at a HUD element (but it could be a 3d overlay).
    if (!updateLaser(controllerPosition, controllerDirection, hudPoint3d)) {
        setReticleVisible(false);
        turnOffLaser();
    }
    /*
    // Hack: Move the pointer again, this time to the intersection. This allows "clicking" on
    // 2D and 3D entities without rewriting other parts of the system, but it isn't right,
    // because the line from camera to the new mouse position might intersect different things
    // than the line from controllerPosition to termination.
    var eye = Camera.getPosition();
    var apparentHudTermination3d = calculateRayUICollisionPoint(eye, Vec3.subtract(termination, eye));
    var apparentHudTermination2d = overlayFromWorldPoint(apparentHudTermination3d);
    Overlays.editOverlay(fakeReticle, {x: apparentHudTermination2d.x - reticleHalfSize, y: apparentHudTermination2d.y - reticleHalfSize});
    //Reticle.visible = false;
    setReticlePosition(apparentHudTermination2d);
*/
}

var UPDATE_INTERVAL = 20; // milliseconds. Script.update is too frequent.
var updater = Script.setInterval(update, UPDATE_INTERVAL);
Script.scriptEnding.connect(function () { Script.clearInterval(updater); });

// Check periodically for changes to setup.
var SETTINGS_CHANGE_RECHECK_INTERVAL = 10 * 1000; // milliseconds
function checkSettings() {
    updateFieldOfView();
    checkForDepthReticleScript()    
}
checkSettings();
var settingsChecker = Script.setInterval(checkSettings, SETTINGS_CHANGE_RECHECK_INTERVAL);
Script.scriptEnding.connect(function () { Script.clearInterval(settingsChecker); });


// DEBUGGING WITHOUT HYDRA -----------------------
//
// The rest of this is for debugging without working hand controllers, using a line from camera to mouse, and an image for cursor.
var CONTROLLER_ROTATION = Quat.fromPitchYawRollDegrees(90, 180, -90);
if (false && !Controller.Hardware.Hydra) {
    print('WARNING: no hand controller detected. Using mouse!');
    var mouseKeeper = {x: 0, y: 0};
    var onMouseMoveCapture = function (event) { mouseKeeper.x = event.x; mouseKeeper.y = event.y; };
    setupHandler(Controller.mouseMoveEvent, onMouseMoveCapture);
    getControllerPose = function () {
        var size = Controller.getViewportDimensions();
        var handPoint = Vec3.subtract(Camera.getPosition(), MyAvatar.position); // Pretend controller is at camera

        // In world-space 3D meters:
        var rotation = Camera.getOrientation();
        var normal = Quat.getFront(rotation);
        var hudHeight = 2 * Math.tan(verticalFieldOfView * DEGREES_TO_HALF_RADIANS);
        var hudWidth = hudHeight * size.x / size.y;
        var rightFraction = mouseKeeper.x / size.x - 0.5;
        var rightMeters = rightFraction * hudWidth;
        var upFraction = mouseKeeper.y / size.y - 0.5;
        var upMeters = upFraction * hudHeight * -1;
        var right = Vec3.multiply(Quat.getRight(rotation), rightMeters);
        var up = Vec3.multiply(Quat.getUp(rotation), upMeters);
        var direction = Vec3.sum(normal, Vec3.sum(right, up));
        var mouseRotation = Quat.rotationBetween(normal, direction);

        var controllerRotation = Quat.multiply(Quat.multiply(mouseRotation, rotation), CONTROLLER_ROTATION);
        var inverseAvatar = Quat.inverse(MyAvatar.orientation);
        return {
            valid: true,
            translation: Vec3.multiplyQbyV(inverseAvatar, handPoint),
            rotation: Quat.multiply(inverseAvatar, controllerRotation)
        };
    };
    // We can't set the mouse if we're using the mouse as a fake controller. So stick an image where we would be putting the mouse.
    // WARNING: This fake cursor is an overlay that will be the target of clicks and drags rather than other overlays underneath it!
    var reticleHalfSize = 16;
    var fakeReticle = Overlays.addOverlay("image", {
        imageURL: "http://s3.amazonaws.com/hifi-public/images/delete.png",
        width: 2 * reticleHalfSize,
        height: 2 * reticleHalfSize,
        alpha: 0.7
    });
    Script.scriptEnding.connect(function () { Overlays.deleteOverlay(fakeReticle); });
    setReticlePosition = function (hudPoint2d) {
        weMovedReticle = true;
        Overlays.editOverlay(fakeReticle, {x: hudPoint2d.x - reticleHalfSize, y: hudPoint2d.y - reticleHalfSize});
    };
    setReticleVisible = function (on) {
        Reticle.visible = on; // FIX SYSTEM BUG: doesn't work on mac.
        Overlays.editOverlay(fakeReticle, {visible: on});
    };
    // The idea here is that we not return a truthy result constantly when we display the fake reticle.
    // But this is done wrong when we're over another overlay as well: if we hit the fakeReticle, we incorrectly answer null here.
    // FIXME: display fake reticle slightly off to the side instead.
    getOverlayAtPoint = function (point2d) {
        var overlay = Overlays.getOverlayAtPoint(point2d);
        if (overlay === fakeReticle) { return null; }
        return overlay;
    };
    var fakeTrigger = 0;
    getValue = function () { var trigger = fakeTrigger; fakeTrigger = 0; return trigger; };
    setupHandler(Controller.keyPressEvent, function (event) {
        switch (event.text) {
        case '`':
            fakeTrigger = 0.4;
            break;
        case '~':
            fakeTrigger = 0.9;
            break;
        }
    });
}
