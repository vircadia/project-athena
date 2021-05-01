"use strict";
/* jslint vars:true, plusplus:true, forin:true */
/* global Tablet, Settings, Script, AvatarList, Users, Entities,
    MyAvatar, Camera, Overlays, Vec3, Quat, HMD, Controller, Account,
    UserActivityLogger, Messages, Window, XMLHttpRequest, print, location, getControllerWorldLocation
*/
/* eslint indent: ["error", 4, { "outerIIFEBody": 0 }] */
//
// pal.js
//
// Created by Howard Stearns on December 9, 2016
// Copyright 2016 High Fidelity, Inc
//
// Distributed under the Apache License, Version 2.0
// See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function () { // BEGIN LOCAL_SCOPE

var request = Script.require('request').request;
var AppUi = Script.require('appUi');

var populateNearbyUserList, color, textures, removeOverlays,
    controllerComputePickRay, off,
    receiveMessage, avatarDisconnected, clearLocalQMLDataAndClosePAL,
    CHANNEL, getConnectionData,
    avatarAdded, avatarRemoved, avatarSessionChanged; // forward references;

// hardcoding these as it appears we cannot traverse the originalTextures in overlays???  Maybe I've missed
// something, will revisit as this is sorta horrible.
var UNSELECTED_TEXTURES = {
    "idle-D": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-idle.png"),
    "idle-E": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-idle.png")
};
var SELECTED_TEXTURES = {
    "idle-D": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-selected.png"),
    "idle-E": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-selected.png")
};
var HOVER_TEXTURES = {
    "idle-D": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-hover.png"),
    "idle-E": Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx/Avatar-Overlay-v1.fbm/avatar-overlay-hover.png")
};

var UNSELECTED_COLOR = { red: 0x1F, green: 0xC6, blue: 0xA6};
var SELECTED_COLOR = {red: 0xF3, green: 0x91, blue: 0x29};
var HOVER_COLOR = {red: 0xD0, green: 0xD0, blue: 0xD0}; // almost white for now
var METAVERSE_BASE = Account.metaverseServerURL;

Script.include("/~/system/libraries/controllers.js");

function projectVectorOntoPlane(normalizedVector, planeNormal) {
    return Vec3.cross(planeNormal, Vec3.cross(normalizedVector, planeNormal));
}
function angleBetweenVectorsInPlane(from, to, normal) {
    var projectedFrom = projectVectorOntoPlane(from, normal);
    var projectedTo = projectVectorOntoPlane(to, normal);
    return Vec3.orientedAngle(projectedFrom, projectedTo, normal);
}

//
// Overlays.
//
var overlays = {}; // Keeps track of all our extended overlay data objects, keyed by target identifier.

function ExtendedOverlay(key, type, properties, selected, hasModel) { // A wrapper around overlays to store the key it is associated with.
    overlays[key] = this;
    if (hasModel) {
        var modelKey = key + "-m";
        this.model = new ExtendedOverlay(modelKey, "model", {
            url: Script.resolvePath("./assets/models/Avatar-Overlay-v1.fbx"),
            textures: textures(selected),
            ignoreRayIntersection: true
        }, false, false);
    } else {
        this.model = undefined;
    }
    this.key = key;
    this.selected = selected || false; // not undefined
    this.hovering = false;
    this.activeOverlay = Overlays.addOverlay(type, properties); // We could use different overlays for (un)selected...
}
// Instance methods:
ExtendedOverlay.prototype.deleteOverlay = function () { // remove display and data of this overlay
    Overlays.deleteOverlay(this.activeOverlay);
    delete overlays[this.key];
};

ExtendedOverlay.prototype.editOverlay = function (properties) { // change display of this overlay
    Overlays.editOverlay(this.activeOverlay, properties);
};

function color(selected, hovering, level) {
    var base = hovering ? HOVER_COLOR : selected ? SELECTED_COLOR : UNSELECTED_COLOR;
    function scale(component) {
        var delta = 0xFF - component;
        return component + (delta * level);
    }
    return {red: scale(base.red), green: scale(base.green), blue: scale(base.blue)};
}

function textures(selected, hovering) {
    return hovering ? HOVER_TEXTURES : selected ? SELECTED_TEXTURES : UNSELECTED_TEXTURES;
}
// so we don't have to traverse the overlays to get the last one
var lastHoveringId = 0;
ExtendedOverlay.prototype.hover = function (hovering) {
    this.hovering = hovering;
    if (this.key === lastHoveringId) {
        if (hovering) {
            return;
        }
        lastHoveringId = 0;
    }
    this.editOverlay({color: color(this.selected, hovering, this.audioLevel)});
    if (this.model) {
        this.model.editOverlay({textures: textures(this.selected, hovering)});
    }
    if (hovering) {
        // un-hover the last hovering overlay
        if (lastHoveringId && lastHoveringId !== this.key) {
            ExtendedOverlay.get(lastHoveringId).hover(false);
        }
        lastHoveringId = this.key;
    }
};
ExtendedOverlay.prototype.select = function (selected) {
    if (this.selected === selected) {
        return;
    }

    UserActivityLogger.palAction(selected ? "avatar_selected" : "avatar_deselected", this.key);

    this.editOverlay({color: color(selected, this.hovering, this.audioLevel)});
    if (this.model) {
        this.model.editOverlay({textures: textures(selected)});
    }
    this.selected = selected;
};
// Class methods:
var selectedIds = [];
ExtendedOverlay.isSelected = function (id) {
    return -1 !== selectedIds.indexOf(id);
};
ExtendedOverlay.get = function (key) { // answer the extended overlay data object associated with the given avatar identifier
    return overlays[key];
};
ExtendedOverlay.some = function (iterator) { // Bails early as soon as iterator returns truthy.
    var key;
    for (key in overlays) {
        if (iterator(ExtendedOverlay.get(key))) {
            return;
        }
    }
};
ExtendedOverlay.unHover = function () { // calls hover(false) on lastHoveringId (if any)
    if (lastHoveringId) {
        ExtendedOverlay.get(lastHoveringId).hover(false);
    }
};

// hit(overlay) on the one overlay intersected by pickRay, if any.
// noHit() if no ExtendedOverlay was intersected (helps with hover)
ExtendedOverlay.applyPickRay = function (pickRay, hit, noHit) {
    // TODO: this could just include the necessary overlays for better performance
    var pickedOverlay = Overlays.findRayIntersection(pickRay, true); // Depends on nearer coverOverlays to extend closer to us than farther ones.
    if (!pickedOverlay.intersects) {
        if (noHit) {
            return noHit();
        }
        return;
    }
    ExtendedOverlay.some(function (overlay) { // See if pickedOverlay is one of ours.
        if ((overlay.activeOverlay) === pickedOverlay.overlayID) {
            hit(overlay);
            return true;
        }
    });
};


//
// Similar, for entities
//
function HighlightedEntity(id, entityProperties) {
    this.id = id;
    this.overlay = Overlays.addOverlay('cube', {
        position: entityProperties.position,
        rotation: entityProperties.rotation,
        dimensions: entityProperties.dimensions,
        solid: false,
        color: {
            red: 0xF3,
            green: 0x91,
            blue: 0x29
        },
        ignoreRayIntersection: true,
        drawInFront: false // Arguable. For now, let's not distract with mysterious wires around the scene.
    });
    HighlightedEntity.overlays.push(this);
}
HighlightedEntity.overlays = [];
HighlightedEntity.clearOverlays = function clearHighlightedEntities() {
    HighlightedEntity.overlays.forEach(function (highlighted) {
        Overlays.deleteOverlay(highlighted.overlay);
    });
    HighlightedEntity.overlays = [];
};
HighlightedEntity.updateOverlays = function updateHighlightedEntities() {
    HighlightedEntity.overlays.forEach(function (highlighted) {
        var properties = Entities.getEntityProperties(highlighted.id, ['position', 'rotation', 'dimensions']);
        Overlays.editOverlay(highlighted.overlay, {
            position: properties.position,
            rotation: properties.rotation,
            dimensions: properties.dimensions
        });
    });
};

/* this contains current gain for a given node (by session id).  More efficient than
 * querying it, plus there isn't a getGain function so why write one */
var sessionGains = {};
function convertDbToLinear(decibels) {
    // +20db = 10x, 0dB = 1x, -10dB = 0.1x, etc...
    // but, your perception is that something 2x as loud is +10db
    // so we go from -60 to +20 or 1/64x to 4x.  For now, we can
    // maybe scale the signal this way??
    return Math.pow(2, decibels / 10.0);
}
function fromQml(message) { // messages are {method, params}, like json-rpc. See also sendToQml.
    var data, connectionUserName, friendUserName;
    switch (message.method) {
    case 'selected':
        selectedIds = message.params;
        ExtendedOverlay.some(function (overlay) {
            var id = overlay.key;
            var selected = ExtendedOverlay.isSelected(id);
            overlay.select(selected);
        });

        HighlightedEntity.clearOverlays();
        if (selectedIds.length) {
            Entities.findEntitiesInFrustum(Camera.frustum).forEach(function (id) {
                // Because lastEditedBy is per session, the vast majority of entities won't match,
                // so it would probably be worth reducing marshalling costs by asking for just we need.
                // However, providing property name(s) is advisory and some additional properties are
                // included anyway. As it turns out, asking for 'lastEditedBy' gives 'position', 'rotation',
                // and 'dimensions', too, so we might as well make use of them instead of making a second
                // getEntityProperties call.
                // It would be nice if we could harden this against future changes by specifying all
                // and only these four in an array, but see
                // https://highfidelity.fogbugz.com/f/cases/2728/Entities-getEntityProperties-id-lastEditedBy-name-lastEditedBy-doesn-t-work
                var properties = Entities.getEntityProperties(id, 'lastEditedBy');
                if (ExtendedOverlay.isSelected(properties.lastEditedBy)) {
                    new HighlightedEntity(id, properties);
                }
            });
        }
        break;
    case 'refresh': // old name for refreshNearby
    case 'refreshNearby':
        data = {};
        ExtendedOverlay.some(function (overlay) { // capture the audio data
            data[overlay.key] = overlay;
        });
        removeOverlays();
        // If filter is specified from .qml instead of through settings, update the settings.
        if (message.params.filter !== undefined) {
            Settings.setValue('pal/filtered', !!message.params.filter);
        }
        populateNearbyUserList(message.params.selected, data);
        UserActivityLogger.palAction("refresh_nearby", "");
        break;
    case 'refreshConnections':
        print('Refreshing Connections...');
        UserActivityLogger.palAction("refresh_connections", "");
        break;
    case 'removeConnection':
        connectionUserName = message.params;
        request({
            uri: METAVERSE_BASE + '/api/v1/user/connections/' + connectionUserName,
            method: 'DELETE'
        }, function (error, response) {
            if (error || (response.status !== 'success')) {
                print("Error: unable to remove connection", connectionUserName, error || response.status);
                return;
            }
            sendToQml({ method: 'connectionRemoved', params: connectionUserName });
        });
        break;

    case 'removeFriend':
        friendUserName = message.params;
        print("Removing " + friendUserName + " from friends.");
        request({
            uri: METAVERSE_BASE + '/api/v1/user/friends/' + friendUserName,
            method: 'DELETE'
        }, function (error, response) {
            if (error || (response.status !== 'success')) {
                print("Error: unable to unfriend " + friendUserName, error || response.status);
                return;
            }
            getConnectionData(friendUserName);
        });
        break;
    case 'addFriend':
        friendUserName = message.params;
        print("Adding " + friendUserName + " to friends.");
        request({
            uri: METAVERSE_BASE + '/api/v1/user/friends',
            method: 'POST',
            json: true,
            body: {
                username: friendUserName,
            }
        }, function (error, response) {
            if (error || (response.status !== 'success')) {
                print("Error: unable to friend " + friendUserName, error || response.status);
                return;
            }
            getConnectionData(friendUserName);
        });
        break;
    case 'http.request':
        break; // Handled by request-service.
    case 'hideNotificationDot':
        shouldShowDot = false;
        ui.messagesWaiting(shouldShowDot);
        break;
    default:
        print('Unrecognized message from Pal.qml');
    }
}

function sendToQml(message) {
    ui.sendMessage(message);
}
function updateUser(data) {
    print('PAL update:', JSON.stringify(data));
    sendToQml({ method: 'updateUsername', params: data });
}
//
// User management services
//
// These are prototype versions that will be changed when the back end changes.

function requestJSON(url, callback) { // callback(data) if successfull. Logs otherwise.
    request({
        uri: url
    }, function (error, response) {
        if (error || (response.status !== 'success')) {
            print("Error: unable to get request",  error || response.status);
            return;
        }
        callback(response.data);
    });
}
function getProfilePicture(username, callback) { // callback(url) if successfull. (Logs otherwise)
    // FIXME Prototype scrapes profile picture. We should include in user status, and also make available somewhere for myself
    request({
        uri: METAVERSE_BASE + '/users/' + username
    }, function (error, html) {
        var matched = !error && html.match(/img class="users-img" src="([^"]*)"/);
        if (!matched) {
            print('Error: Unable to get profile picture for', username, error);
            callback('');
            return;
        }
        callback(matched[1]);
    });
}
var SAFETY_LIMIT = 400;
function getAvailableConnections(domain, callback, numResultsPerPage) { // callback([{usename, location}...]) if successfull. (Logs otherwise)
    var url = METAVERSE_BASE + '/api/v1/users?per_page=' + (numResultsPerPage || SAFETY_LIMIT) + '&';
    if (domain) {
        url += 'status=' + domain.slice(1, -1); // without curly braces
    } else {
        url += 'filter=connections'; // regardless of whether online
    }
    requestJSON(url, function (connectionsData) {
        callback(connectionsData.users);
    });
}
function getInfoAboutUser(specificUsername, callback) {
    var url = METAVERSE_BASE + '/api/v1/users?filter=connections&per_page=' + SAFETY_LIMIT + '&search=' + encodeURIComponent(specificUsername);
    requestJSON(url, function (connectionsData) {
        // You could have (up to SAFETY_LIMIT connections whose usernames contain the specificUsername.
        // Search returns all such matches.
        for (user in connectionsData.users) {
            if (connectionsData.users[user].username === specificUsername) {
                callback(connectionsData.users[user]);
                return;
            }
        }
        callback(false);
    });
}
function getConnectionData(specificUsername, domain) { // Update all the usernames that I am entitled to see, using my login but not dependent on canKick.
    function frob(user) { // get into the right format
        var formattedSessionId = user.location.node_id || '';
        if (formattedSessionId !== '' && formattedSessionId.indexOf("{") != 0) {
            formattedSessionId = "{" + formattedSessionId + "}";
        }
        return {
            sessionId: formattedSessionId,
            userName: user.username,
            connection: user.connection,
            profileUrl: user.images.thumbnail,
            placeName: (user.location.root || user.location.domain || {}).name || ''
        };
    }
    if (specificUsername) {
        getInfoAboutUser(specificUsername, function (user) {
            if (user) {
                updateUser(frob(user));
            } else {
                print('Error: Unable to find information about ' + specificUsername + ' in connectionsData!');
            }
        });
    } else if (domain) {
        getAvailableConnections(domain, function (users) {
            users.forEach(function (user) {
                updateUser(frob(user));
            });
        });
    } else {
        print("Error: unrecognized getConnectionData()");
    }
}

//
// Main operations.
//
function addAvatarNode(id) {
    var selected = ExtendedOverlay.isSelected(id);
    return new ExtendedOverlay(id, "sphere", {
        drawInFront: true,
        solid: true,
        alpha: 0.8,
        color: color(selected, false, 0.0),
        ignoreRayIntersection: false
    }, selected, true);
}
// Each open/refresh will capture a stable set of avatarsOfInterest, within the specified filter.
var avatarsOfInterest = {};
function populateNearbyUserList(selectData, oldAudioData) {
    var filter = Settings.getValue('pal/filtered') && {distance: Settings.getValue('pal/nearDistance')},
        data = [],
        avatars = AvatarList.getAvatarIdentifiers(),
        myPosition = filter && Camera.position,
        frustum = filter && Camera.frustum,
        verticalHalfAngle = filter && (frustum.fieldOfView / 2),
        horizontalHalfAngle = filter && (verticalHalfAngle * frustum.aspectRatio),
        orientation = filter && Camera.orientation,
        forward = filter && Quat.getForward(orientation),
        verticalAngleNormal = filter && Quat.getRight(orientation),
        horizontalAngleNormal = filter && Quat.getUp(orientation);
    avatarsOfInterest = {};

    var avatarData = AvatarList.getPalData().data;

    avatarData.forEach(function (currentAvatarData) {
        var id = currentAvatarData.sessionUUID;
        var name = currentAvatarData.sessionDisplayName;
        if (!name) {
            // Either we got a data packet but no identity yet, or something is really messed up. In any case,
            // we won't be able to do anything with this user, so don't include them.
            // In normal circumstances, a refresh will bring in the new user, but if we're very heavily loaded,
            // we could be losing and gaining people randomly.
            print('No avatar identity data for', currentAvatarData.sessionUUID);
            return;
        }
        if (id && myPosition && (Vec3.distance(currentAvatarData.position, myPosition) > filter.distance)) {
            return;
        }
        var normal = id && filter && Vec3.normalize(Vec3.subtract(currentAvatarData.position, myPosition));
        var horizontal = normal && angleBetweenVectorsInPlane(normal, forward, horizontalAngleNormal);
        var vertical = normal && angleBetweenVectorsInPlane(normal, forward, verticalAngleNormal);
        if (id && filter && ((Math.abs(horizontal) > horizontalHalfAngle) || (Math.abs(vertical) > verticalHalfAngle))) {
            return;
        }
        var oldAudio = oldAudioData && oldAudioData[id];
        var avatarPalDatum = {
            profileUrl: '',
            displayName: name,
            userName: '',
            connection: '',
            sessionId: id || '',
            audioLevel: (oldAudio && oldAudio.audioLevel) || 0.0,
            avgAudioLevel: (oldAudio && oldAudio.avgAudioLevel) || 0.0,
            admin: false,
            personalMute: !!id && Users.getPersonalMuteStatus(id), // expects proper boolean, not null
            ignore: !!id && Users.getIgnoreStatus(id), // ditto
            isPresent: true,
            isReplicated: currentAvatarData.isReplicated
        };
        // Everyone needs to see admin status. Username and fingerprint returns default constructor output if the requesting user isn't an admin.
        Users.requestUsernameFromID(id);
        if (id !== "") {
            addAvatarNode(id); // No overlay for ourselves
            avatarsOfInterest[id] = true;
        } else {
            // Return our username from the Account API
            avatarPalDatum.userName = Account.username;
        }
        data.push(avatarPalDatum);
        print('PAL data:', JSON.stringify(avatarPalDatum));
    });
    getConnectionData(false, location.domainID); // Even admins don't get relationship data in requestUsernameFromID (which is still needed for admin status, which comes from domain).
    sendToQml({ method: 'nearbyUsers', params: data });
    if (selectData) {
        selectData[2] = true;
        sendToQml({ method: 'select', params: selectData });
    }
}

// The function that handles the reply from the server
function usernameFromIDReply(id, username, machineFingerprint, isAdmin) {
    var data = {
        sessionId: (MyAvatar.sessionUUID === id) ? '' : id, // Pal.qml recognizes empty id specially.
        // If we get username (e.g., if in future we receive it when we're friends), use it.
        // Otherwise, use valid machineFingerprint (which is not valid when not an admin).
        userName: username || (Users.canKick && machineFingerprint) || '',
        admin: isAdmin
    };
    // Ship the data off to QML
    updateUser(data);
}

function updateAudioLevel(avatarData) {
    // the VU meter should work similarly to the one in AvatarInputs: log scale, exponentially averaged
    // But of course it gets the data at a different rate, so we tweak the averaging ratio and frequency
    // of updating (the latter for efficiency too).
    var audioLevel = 0.0;
    var avgAudioLevel = 0.0;

    var data = avatarData.sessionUUID === "" ? myData : ExtendedOverlay.get(avatarData.sessionUUID);

    if (data) {
        // we will do exponential moving average by taking some the last loudness and averaging
        data.accumulatedLevel = AVERAGING_RATIO * (data.accumulatedLevel || 0) + (1 - AVERAGING_RATIO) * (avatarData.audioLoudness);

        // add 1 to insure we don't go log() and hit -infinity.  Math.log is
        // natural log, so to get log base 2, just divide by ln(2).
        audioLevel = scaleAudio(Math.log(data.accumulatedLevel + 1) / LOG2);

        // decay avgAudioLevel
        avgAudioLevel = Math.max((1 - AUDIO_PEAK_DECAY) * (data.avgAudioLevel || 0), audioLevel);

        data.avgAudioLevel = avgAudioLevel;
        data.audioLevel = audioLevel;

        // now scale for the gain.  Also, asked to boost the low end, so one simple way is
        // to take sqrt of the value.  Lets try that, see how it feels.
        avgAudioLevel = Math.min(1.0, Math.sqrt(avgAudioLevel * (sessionGains[avatarData.sessionUUID] || 0.75)));
    }

    var param = {};
    var level = [audioLevel, avgAudioLevel];
    var userId = avatarData.sessionUUID;
    param[userId] = level;
    sendToQml({ method: 'updateAudioLevel', params: param });
}

var pingPong = true;
function updateOverlays() {
    var eye = Camera.position;

    var avatarData = AvatarList.getPalData().data;

    avatarData.forEach(function (currentAvatarData) {

        if (currentAvatarData.sessionUUID === "" || !avatarsOfInterest[currentAvatarData.sessionUUID]) {
            return; // don't update ourself, or avatars we're not interested in
        }
        updateAudioLevel(currentAvatarData);
        var overlay = ExtendedOverlay.get(currentAvatarData.sessionUUID);
        if (!overlay) { // For now, we're treating this as a temporary loss, as from the personal space bubble. Add it back.
            print('Adding non-PAL avatar node', currentAvatarData.sessionUUID);
            overlay = addAvatarNode(currentAvatarData.sessionUUID);
        }

        var target = currentAvatarData.position;
        var distance = Vec3.distance(target, eye);
        var offset = currentAvatarData.palOrbOffset;
        var diff = Vec3.subtract(target, eye); // get diff between target and eye (a vector pointing to the eye from avatar position)

        // move a bit in front, towards the camera
        target = Vec3.subtract(target, Vec3.multiply(Vec3.normalize(diff), offset));

        // now bump it up a bit
        target.y = target.y + offset;

        overlay.ping = pingPong;
        overlay.editOverlay({
            color: color(ExtendedOverlay.isSelected(currentAvatarData.sessionUUID), overlay.hovering, overlay.audioLevel),
            position: target,
            dimensions: 0.032 * distance
        });
        if (overlay.model) {
            overlay.model.ping = pingPong;
            overlay.model.editOverlay({
                position: target,
                scale: 0.2 * distance, // constant apparent size
                rotation: Camera.orientation
            });
        }
    });
    pingPong = !pingPong;
    ExtendedOverlay.some(function (overlay) { // Remove any that weren't updated. (User is gone.)
        if (overlay.ping === pingPong) {
            overlay.deleteOverlay();
        }
    });
    // We could re-populateNearbyUserList if anything added or removed, but not for now.
    HighlightedEntity.updateOverlays();
}
function removeOverlays() {
    selectedIds = [];
    lastHoveringId = 0;
    HighlightedEntity.clearOverlays();
    ExtendedOverlay.some(function (overlay) {
        overlay.deleteOverlay();
    });
}

//
// Clicks.
//
function handleClick(pickRay) {
    ExtendedOverlay.applyPickRay(pickRay, function (overlay) {
        // Don't select directly. Tell qml, who will give us back a list of ids.
        var message = {method: 'select', params: [[overlay.key], !overlay.selected, false]};
        sendToQml(message);
        return true;
    });
}
function handleMouseEvent(mousePressEvent) { // handleClick if we get one.
    if (!mousePressEvent.isLeftButton) {
        return;
    }
    handleClick(Camera.computePickRay(mousePressEvent.x, mousePressEvent.y));
}
function handleMouseMove(pickRay) { // given the pickRay, just do the hover logic
    ExtendedOverlay.applyPickRay(pickRay, function (overlay) {
        overlay.hover(true);
    }, function () {
        ExtendedOverlay.unHover();
    });
}

// handy global to keep track of which hand is the mouse (if any)
var currentHandPressed = 0;
var TRIGGER_CLICK_THRESHOLD = 0.85;
var TRIGGER_PRESS_THRESHOLD = 0.05;

function handleMouseMoveEvent(event) { // find out which overlay (if any) is over the mouse position
    var pickRay;
    if (HMD.active) {
        if (currentHandPressed !== 0) {
            pickRay = controllerComputePickRay(currentHandPressed);
        } else {
            // nothing should hover, so
            ExtendedOverlay.unHover();
            return;
        }
    } else {
        pickRay = Camera.computePickRay(event.x, event.y);
    }
    handleMouseMove(pickRay);
}
function handleTriggerPressed(hand, value) {
    // The idea is if you press one trigger, it is the one
    // we will consider the mouse.  Even if the other is pressed,
    // we ignore it until this one is no longer pressed.
    var isPressed = value > TRIGGER_PRESS_THRESHOLD;
    if (currentHandPressed === 0) {
        currentHandPressed = isPressed ? hand : 0;
        return;
    }
    if (currentHandPressed === hand) {
        currentHandPressed = isPressed ? hand : 0;
        return;
    }
    // otherwise, the other hand is still triggered
    // so do nothing.
}

// We get mouseMoveEvents from the handControllers, via handControllerPointer.
// But we don't get mousePressEvents.
var triggerMapping = Controller.newMapping(Script.resolvePath('') + '-click');
var triggerPressMapping = Controller.newMapping(Script.resolvePath('') + '-press');
function controllerComputePickRay(hand) {
    var controllerPose = getControllerWorldLocation(hand, true);
    if (controllerPose.valid) {
        return { origin: controllerPose.position, direction: Quat.getUp(controllerPose.orientation) };
    }
}
function makeClickHandler(hand) {
    return function (clicked) {
        if (clicked > TRIGGER_CLICK_THRESHOLD) {
            var pickRay = controllerComputePickRay(hand);
            handleClick(pickRay);
        }
    };
}
function makePressHandler(hand) {
    return function (value) {
        handleTriggerPressed(hand, value);
    };
}
triggerMapping.from(Controller.Standard.RTClick).peek().to(makeClickHandler(Controller.Standard.RightHand));
triggerMapping.from(Controller.Standard.LTClick).peek().to(makeClickHandler(Controller.Standard.LeftHand));
triggerPressMapping.from(Controller.Standard.RT).peek().to(makePressHandler(Controller.Standard.RightHand));
triggerPressMapping.from(Controller.Standard.LT).peek().to(makePressHandler(Controller.Standard.LeftHand));

var ui;
// Most apps can have people toggle the tablet closed and open again, and the app should remain "open" even while
// the tablet is not shown. However, for the pal, we explicitly close the app and return the tablet to it's 
// home screen (so that the avatar highlighting goes away).
function tabletVisibilityChanged() {
    if (!ui.tablet.tabletShown && ui.isOpen) {
        ui.close();
    }
}

var UPDATE_INTERVAL_MS = 100;
var updateInterval;
function createUpdateInterval() {
    return Script.setInterval(function () {
        updateOverlays();
    }, UPDATE_INTERVAL_MS);
}

var previousRequestsDomainListData = Users.requestsDomainListData;
function palOpened() {
    ui.sendMessage({
        method: 'changeConnectionsDotStatus',
        shouldShowDot: shouldShowDot
    });

    previousRequestsDomainListData = Users.requestsDomainListData;
    Users.requestsDomainListData = true;

    ui.tablet.tabletShownChanged.connect(tabletVisibilityChanged);
    updateInterval = createUpdateInterval();
    Controller.mousePressEvent.connect(handleMouseEvent);
    Controller.mouseMoveEvent.connect(handleMouseMoveEvent);
    Users.usernameFromIDReply.connect(usernameFromIDReply);
    triggerMapping.enable();
    triggerPressMapping.enable();
    populateNearbyUserList();
}

//
// Message from other scripts, such as edit.js
//
var CHANNEL = 'com.highfidelity.pal';
function receiveMessage(channel, messageString, senderID) {
    if ((channel !== CHANNEL) || (senderID !== MyAvatar.sessionUUID)) {
        return;
    }
    var message = JSON.parse(messageString);
    switch (message.method) {
    case 'select':
        if (!ui.isOpen) {
            ui.open();
            Script.setTimeout(function () { sendToQml(message); }, 1000);
        } else {
            sendToQml(message); // Accepts objects, not just strings.
        }
        break;
    }
}

var AVERAGING_RATIO = 0.05;
var LOUDNESS_FLOOR = 11.0;
var LOUDNESS_SCALE = 2.8 / 5.0;
var LOG2 = Math.log(2.0);
var AUDIO_PEAK_DECAY = 0.02;
var myData = {}; // we're not includied in ExtendedOverlay.get.

function scaleAudio(val) {
    var audioLevel = 0.0;
    if (val <= LOUDNESS_FLOOR) {
        audioLevel = val / LOUDNESS_FLOOR * LOUDNESS_SCALE;
    } else {
        audioLevel = (val - (LOUDNESS_FLOOR - 1)) * LOUDNESS_SCALE;
    }
    if (audioLevel > 1.0) {
        audioLevel = 1;
    }
    return audioLevel;
}

function avatarDisconnected(nodeID) {
    // remove from the pal list
    sendToQml({method: 'avatarDisconnected', params: [nodeID]});
}

function clearLocalQMLDataAndClosePAL() {
    sendToQml({ method: 'clearLocalQMLData' });
    if (ui.isOpen) {
        ui.close();
    }
}

function avatarAdded(avatarID) {
    sendToQml({ method: 'palIsStale', params: [avatarID, 'avatarAdded'] });
}

function avatarRemoved(avatarID) {
    sendToQml({ method: 'palIsStale', params: [avatarID, 'avatarRemoved'] });
}

function avatarSessionChanged(avatarID) {
    sendToQml({ method: 'palIsStale', params: [avatarID, 'avatarSessionChanged'] });
}

function notificationDataProcessPage(data) {
    return data.data.users;
}

var shouldShowDot = false;
var pingPong = false;
var storedOnlineUsers = {};
function notificationPollCallback(connectionsArray) {
    //
    // START logic for handling online/offline user changes
    //
    pingPong = !pingPong;
    var newOnlineUsers = 0;
    var message;

    connectionsArray.forEach(function (user) {
        var stored = storedOnlineUsers[user.username];
        var storedOrNew = stored || user;
        storedOrNew.pingPong = pingPong;
        if (stored) {
            return;
        }

        newOnlineUsers++;
        storedOnlineUsers[user.username] = user;

        if (!ui.isOpen && ui.notificationInitialCallbackMade[0]) {
            message = user.username + " is available in " +
                user.location.root.name + ". Open PEOPLE to join them.";
            ui.notificationDisplayBanner(message);
        }
    });
    var key;
    for (key in storedOnlineUsers) {
        if (storedOnlineUsers[key].pingPong !== pingPong) {
            delete storedOnlineUsers[key];
        }
    }
    shouldShowDot = newOnlineUsers > 0 || (Object.keys(storedOnlineUsers).length > 0 && shouldShowDot);
    //
    // END logic for handling online/offline user changes
    //

    if (!ui.isOpen) {
        ui.messagesWaiting(shouldShowDot);
        ui.sendMessage({
            method: 'changeConnectionsDotStatus',
            shouldShowDot: shouldShowDot
        });

        if (newOnlineUsers > 0 && !ui.notificationInitialCallbackMade[0]) {
            message = newOnlineUsers + " of your connections " +
                (newOnlineUsers === 1 ? "is" : "are") + " available online. Open PEOPLE to join them.";
            ui.notificationDisplayBanner(message);
        }
    }
}

function isReturnedDataEmpty(data) {
    var usersArray = data.data.users;
    return usersArray.length === 0;
}

function startup() {
    ui = new AppUi({
        buttonName: "PEOPLE",
        sortOrder: 7,
        home: "hifi/Pal.qml",
        onOpened: palOpened,
        onClosed: off,
        onMessage: fromQml,
        notificationPollEndpoint: ["/api/v1/users?filter=connections&status=online&per_page=10"],
        notificationPollTimeoutMs: [60000],
        notificationDataProcessPage: [notificationDataProcessPage],
        notificationPollCallback: [notificationPollCallback],
        notificationPollStopPaginatingConditionMet: [isReturnedDataEmpty],
        notificationPollCaresAboutSince: [false]
    });
    Window.domainChanged.connect(clearLocalQMLDataAndClosePAL);
    Window.domainConnectionRefused.connect(clearLocalQMLDataAndClosePAL);
    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(receiveMessage);
    Users.avatarDisconnected.connect(avatarDisconnected);
    AvatarList.avatarAddedEvent.connect(avatarAdded);
    AvatarList.avatarRemovedEvent.connect(avatarRemoved);
    AvatarList.avatarSessionChangedEvent.connect(avatarSessionChanged);
}
startup();

function off() {
    if (ui.isOpen) { // i.e., only when connected
        if (updateInterval) {
            Script.clearInterval(updateInterval);
        }
        Controller.mousePressEvent.disconnect(handleMouseEvent);
        Controller.mouseMoveEvent.disconnect(handleMouseMoveEvent);
        ui.tablet.tabletShownChanged.disconnect(tabletVisibilityChanged);
        Users.usernameFromIDReply.disconnect(usernameFromIDReply);
        triggerMapping.disable();
        triggerPressMapping.disable();
    }

    removeOverlays();
    Users.requestsDomainListData = previousRequestsDomainListData;
}

function shutdown() {
    Window.domainChanged.disconnect(clearLocalQMLDataAndClosePAL);
    Window.domainConnectionRefused.disconnect(clearLocalQMLDataAndClosePAL);
    Messages.subscribe(CHANNEL);
    Messages.messageReceived.disconnect(receiveMessage);
    Users.avatarDisconnected.disconnect(avatarDisconnected);
    AvatarList.avatarAddedEvent.disconnect(avatarAdded);
    AvatarList.avatarRemovedEvent.disconnect(avatarRemoved);
    AvatarList.avatarSessionChangedEvent.disconnect(avatarSessionChanged);
    off();
}
Script.scriptEnding.connect(shutdown);

}()); // END LOCAL_SCOPE
