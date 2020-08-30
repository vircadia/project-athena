"use strict";
/*jslint vars:true, plusplus:true, forin:true*/
/*global Tablet, Script, Entities, MyAvatar, Camera, Quat, HMD, Account, UserActivityLogger, Messages, print,
  AvatarBookmarks, AddressManager
*/
/* eslint indent: ["error", 4, { "outerIIFEBody": 0 }] */
//
// avatarapp.js
//
// Created by Alexander Ivash on April 30, 2018
// Copyright 2016 High Fidelity, Inc
//
// Distributed under the Apache License, Version 2.0
// See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() { // BEGIN LOCAL_SCOPE

var AVATARAPP_QML_SOURCE = "hifi/AvatarApp.qml";
Script.include("/~/system/libraries/controllers.js");

// constants from AvatarBookmarks.h
var ENTRY_AVATAR_URL = "avatarUrl";
var ENTRY_AVATAR_ENTITIES = "avatarEntites";
var ENTRY_AVATAR_SCALE = "avatarScale";

function executeLater(callback) {
    Script.setTimeout(callback, 300);
}

function isWearable(avatarEntity) {
    return avatarEntity.properties.visible === true &&
        (avatarEntity.properties.parentID === MyAvatar.sessionUUID || avatarEntity.properties.parentID === MyAvatar.SELF_ID);
}

function getMyAvatarWearables() {
    var entitiesArray = MyAvatar.getAvatarEntitiesVariant();
    var wearablesArray = [];

    for (var i = 0; i < entitiesArray.length; ++i) {
        var entity = entitiesArray[i];
        if (!isWearable(entity)) {
            continue;
        }

        var localRotation = entity.properties.localRotation;
        entity.properties.localRotationAngles = Quat.safeEulerAngles(localRotation);
        wearablesArray.push(entity);
    }

    return wearablesArray;
}

function getMyAvatar() {
    var avatar = {};
    avatar[ENTRY_AVATAR_URL] = MyAvatar.skeletonModelURL;
    avatar[ENTRY_AVATAR_SCALE] = MyAvatar.getAvatarScale();
    avatar[ENTRY_AVATAR_ENTITIES] = getMyAvatarWearables();
    return avatar;
}

function getMyAvatarSettings() {
    return {
        dominantHand: MyAvatar.getDominantHand(),
        hmdAvatarAlignmentType: MyAvatar.getHmdAvatarAlignmentType(),
        collisionsEnabled: MyAvatar.getCollisionsEnabled(),
        otherAvatarsCollisionsEnabled: MyAvatar.getOtherAvatarsCollisionsEnabled(),
        collisionSoundUrl : MyAvatar.collisionSoundURL,
        animGraphUrl: MyAvatar.getAnimGraphUrl(),
        animGraphOverrideUrl : MyAvatar.getAnimGraphOverrideUrl(),
    };
}

function updateAvatarWearables(avatar, callback, wearablesOverride) {
    executeLater(function() {
        var wearables = wearablesOverride ? wearablesOverride : getMyAvatarWearables();
        avatar[ENTRY_AVATAR_ENTITIES] = wearables;

        sendToQml({'method' : 'wearablesUpdated', 'wearables' : wearables});
        sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});

        if(callback)
            callback();
    });
}

var adjustWearables = {
    opened : false,
    cameraMode : '',
    setOpened : function(value) {
        if(this.opened !== value) {
            if(value) {
                this.cameraMode = Camera.mode;

                if(!HMD.active) {
                    Camera.mode = 'mirror';
                }
            } else {
                Camera.mode = this.cameraMode;
            }

            this.opened = value;
        }
    }
};

var currentAvatarWearablesBackup = null;
var currentAvatar = null;
var currentAvatarSettings = getMyAvatarSettings();

var notifyScaleChanged = true;
function onTargetScaleChanged() {
    if(currentAvatar.scale !== MyAvatar.getAvatarScale()) {
        currentAvatar.scale = MyAvatar.getAvatarScale();
        if(notifyScaleChanged) {
            sendToQml({'method' : 'scaleChanged', 'value' : currentAvatar.scale});
        }
    }
}

function onSkeletonModelURLChanged() {
    if(currentAvatar || (currentAvatar.skeletonModelURL !== MyAvatar.skeletonModelURL)) {
        fromQml({'method' : 'getAvatars'});
    }
}

function onDominantHandChanged(dominantHand) {
    if(currentAvatarSettings.dominantHand !== dominantHand) {
        currentAvatarSettings.dominantHand = dominantHand;
        sendToQml({'method' : 'settingChanged', 'name' : 'dominantHand', 'value' : dominantHand});
    }
}

function onHmdAvatarAlignmentTypeChanged(type) {
    if (currentAvatarSettings.hmdAvatarAlignmentType !== type) {
        currentAvatarSettings.hmdAvatarAlignmentType = type;
        sendToQml({'method' : 'settingChanged', 'name' : 'hmdAvatarAlignmentType', 'value' : type});
    }
}

function onCollisionsEnabledChanged(enabled) {
    if(currentAvatarSettings.collisionsEnabled !== enabled) {
        currentAvatarSettings.collisionsEnabled = enabled;
        sendToQml({'method' : 'settingChanged', 'name' : 'collisionsEnabled', 'value' : enabled});
    }
}

function onOtherAvatarsCollisionsEnabledChanged(enabled) {
    if (currentAvatarSettings.otherAvatarsCollisionsEnabled !== enabled) {
        currentAvatarSettings.otherAvatarsCollisionsEnabled = enabled;
        sendToQml({ 'method': 'settingChanged', 'name': 'otherAvatarsCollisionsEnabled', 'value': enabled });
    }
}

function onNewCollisionSoundUrl(url) {
    if(currentAvatarSettings.collisionSoundUrl !== url) {
        currentAvatarSettings.collisionSoundUrl = url;
        sendToQml({'method' : 'settingChanged', 'name' : 'collisionSoundUrl', 'value' : url});
    }
}

function onAnimGraphUrlChanged(url) {
    if (currentAvatarSettings.animGraphUrl !== url) {
        currentAvatarSettings.animGraphUrl = url;
        sendToQml({ 'method': 'settingChanged', 'name': 'animGraphUrl', 'value': currentAvatarSettings.animGraphUrl });

        if (currentAvatarSettings.animGraphOverrideUrl !== MyAvatar.getAnimGraphOverrideUrl()) {
            currentAvatarSettings.animGraphOverrideUrl = MyAvatar.getAnimGraphOverrideUrl();
            sendToQml({ 'method': 'settingChanged', 'name': 'animGraphOverrideUrl',
                        'value': currentAvatarSettings.animGraphOverrideUrl });
        }
    }
}

var selectedAvatarEntityID = null;
var grabbedAvatarEntityChangeNotifier = null;

var MARKETPLACES_INJECT_SCRIPT_URL = Script.resolvePath("html/js/marketplacesInject.js");

function getWearablesFrozen() {
    var wearablesFrozen = true;
    var wearablesArray = getMyAvatarWearables();
    wearablesArray.forEach(function(wearable) {
        if (isGrabbable(wearable.id)) {
            wearablesFrozen = false;
        }
    });

    return wearablesFrozen;
}

function freezeWearables() {
    var wearablesArray = getMyAvatarWearables();
    wearablesArray.forEach(function(wearable) {
        setGrabbable(wearable.id, false);
    });
}

function unfreezeWearables() {
    var wearablesArray = getMyAvatarWearables();
    wearablesArray.forEach(function(wearable) {
        setGrabbable(wearable.id, true);
    });
}


function fromQml(message) { // messages are {method, params}, like json-rpc. See also sendToQml.
    switch (message.method) {
    case 'getAvatars':
        currentAvatar = getMyAvatar();
        currentAvatarSettings = getMyAvatarSettings();

        message.data = {
            'bookmarks' : AvatarBookmarks.getBookmarks(),
            'displayName' : MyAvatar.displayName,
            'currentAvatar' : currentAvatar,
            'currentAvatarSettings' : currentAvatarSettings
        };

        for(var bookmarkName in message.data.bookmarks) {
            var bookmark = message.data.bookmarks[bookmarkName];

            if (bookmark.avatarEntites) {
                bookmark.avatarEntites.forEach(function(avatarEntity) {
                    avatarEntity.properties.localRotationAngles = Quat.safeEulerAngles(avatarEntity.properties.localRotation);
                });
            }
        }

        sendToQml(message);
        break;
    case 'selectAvatar':
        Entities.addingWearable.disconnect(onAddingWearable);
        Entities.deletingWearable.disconnect(onDeletingWearable);
        AvatarBookmarks.loadBookmark(message.name);
        Entities.addingWearable.connect(onAddingWearable);
        Entities.deletingWearable.connect(onDeletingWearable);
        sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});
        break;
    case 'deleteAvatar':
        AvatarBookmarks.removeBookmark(message.name);
        break;
    case 'addAvatar':
        AvatarBookmarks.addBookmark(message.name);
        break;
    case 'adjustWearable':
        if(message.properties.localRotationAngles) {
            message.properties.localRotation = Quat.fromVec3Degrees(message.properties.localRotationAngles);
        }

        Entities.editEntity(message.entityID, message.properties);
        message.properties = Entities.getEntityProperties(message.entityID, Object.keys(message.properties));

        if(message.properties.localRotation) {
            message.properties.localRotationAngles = Quat.safeEulerAngles(message.properties.localRotation);
        }

        sendToQml({'method' : 'wearableUpdated', 'entityID' : message.entityID, wearableIndex : message.wearableIndex, properties : message.properties, updateUI : false});
        break;
    case 'adjustWearablesOpened':
        currentAvatarWearablesBackup = getMyAvatarWearables();
        adjustWearables.setOpened(true);
        unfreezeWearables();

        Entities.mousePressOnEntity.connect(onSelectedEntity);
        Messages.subscribe('Hifi-Object-Manipulation');
        Messages.messageReceived.connect(handleWearableMessages);
        break;
    case 'adjustWearablesClosed':
        if(!message.save) {
            // revert changes using snapshot of wearables
            if(currentAvatarWearablesBackup !== null) {
                AvatarBookmarks.updateAvatarEntities(currentAvatarWearablesBackup);
                updateAvatarWearables(currentAvatar, null, currentAvatarWearablesBackup);
            }
        } else {
            sendToQml({'method' : 'updateAvatarInBookmarks'});
        }

        adjustWearables.setOpened(false);
        ensureWearableSelected(null);
        Entities.mousePressOnEntity.disconnect(onSelectedEntity);
        Messages.messageReceived.disconnect(handleWearableMessages);
        Messages.unsubscribe('Hifi-Object-Manipulation');
        break;
    case 'addWearable':

        var joints = MyAvatar.getJointNames();
        var hipsIndex = -1;

        for(var i = 0; i < joints.length; ++i) {
            if(joints[i] === 'Hips') {
                hipsIndex = i;
                break;
            }
        }

        var properties = {
            name: "Custom wearable",
            type: "Model",
            modelURL: message.url,
            parentID: MyAvatar.sessionUUID,
            relayParentJoints: false,
            parentJointIndex: hipsIndex
        };

        Entities.addingWearable.disconnect(onAddingWearable);
        var entityID = Entities.addEntity(properties, true);
        Entities.addingWearable.connect(onAddingWearable);

        updateAvatarWearables(currentAvatar, function() {
            onSelectedEntity(entityID);
        });
        break;
    case 'selectWearable':
        ensureWearableSelected(message.entityID);
        break;
    case 'deleteWearable':

        Entities.deletingWearable.disconnect(onDeletingWearable);
        Entities.deleteEntity(message.entityID);
        Entities.deletingWearable.connect(onDeletingWearable);

        updateAvatarWearables(currentAvatar);
        break;
    case 'changeDisplayName':
        if (MyAvatar.displayName !== message.displayName) {
            MyAvatar.displayName = message.displayName;
            UserActivityLogger.palAction("display_name_change", message.displayName);
        }
        break;
    case 'applyExternalAvatar':
        var currentAvatarURL = MyAvatar.getFullAvatarURLFromPreferences();
        if(currentAvatarURL !== message.avatarURL) {
            MyAvatar.useFullAvatarURL(message.avatarURL);
            sendToQml({'method' : 'externalAvatarApplied', 'avatarURL' : message.avatarURL});
        }
        break;
    case 'navigate':
        var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
        if(message.url.indexOf('hifi://') === 0) {
            AddressManager.handleLookupString(message.url, false);
        } else if(message.url.indexOf('https://') === 0 || message.url.indexOf('http://') === 0) {
            tablet.gotoWebScreen(message.url, MARKETPLACES_INJECT_SCRIPT_URL);
        }

        break;
    case 'setScale':
        notifyScaleChanged = false;
        MyAvatar.setAvatarScale(message.avatarScale);
        currentAvatar.avatarScale = message.avatarScale;
        notifyScaleChanged = true;
        break;
    case 'revertScale':
        MyAvatar.setAvatarScale(message.avatarScale);
        currentAvatar.avatarScale = message.avatarScale;
        break;
    case 'saveSettings':
        MyAvatar.setAvatarScale(message.avatarScale);
        currentAvatar.avatarScale = message.avatarScale;

        MyAvatar.setDominantHand(message.settings.dominantHand);
        MyAvatar.setHmdAvatarAlignmentType(message.settings.hmdAvatarAlignmentType);
        MyAvatar.setOtherAvatarsCollisionsEnabled(message.settings.otherAvatarsCollisionsEnabled);
        MyAvatar.setCollisionsEnabled(message.settings.collisionsEnabled);
        MyAvatar.collisionSoundURL = message.settings.collisionSoundUrl;
        MyAvatar.setAnimGraphOverrideUrl(message.settings.animGraphOverrideUrl);

        currentAvatarSettings = getMyAvatarSettings();
        break;
    case 'toggleWearablesFrozen':
        var wearablesFrozen = getWearablesFrozen();
        wearablesFrozen = !wearablesFrozen;
        if (wearablesFrozen) {
            freezeWearables();
        } else {
            unfreezeWearables();
        }
        sendToQml({'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : wearablesFrozen});
        break;
    default:
        print('Unrecognized message from AvatarApp.qml');
    }
}

function isGrabbable(entityID) {
    if(entityID === null) {
        return false;
    }

    var properties = Entities.getEntityProperties(entityID, ['avatarEntity', 'grab.grabbable']);
    if (properties.avatarEntity) {
        return properties.grab.grabbable;
    }

    return false;
}

function setGrabbable(entityID, grabbable) {
    var properties = Entities.getEntityProperties(entityID, ['avatarEntity', 'grab.grabbable']);
    if (properties.avatarEntity && properties.grab.grabbable != grabbable) {
        var editProps = { grab: { grabbable: grabbable }};
        Entities.editEntity(entityID, editProps);
        sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});
    }
}

function ensureWearableSelected(entityID) {
    if(selectedAvatarEntityID !== entityID) {
        if(grabbedAvatarEntityChangeNotifier !== null) {
            Script.clearInterval(grabbedAvatarEntityChangeNotifier);
            grabbedAvatarEntityChangeNotifier = null;
        }
        selectedAvatarEntityID = entityID;
        return true;
    }

    return false;
}

function isEntityBeingWorn(entityID) {
    return Entities.getEntityProperties(entityID, 'parentID').parentID === MyAvatar.sessionUUID;
}

function onSelectedEntity(entityID, pointerEvent) {
    if(selectedAvatarEntityID !== entityID && isEntityBeingWorn(entityID))
    {
        if(ensureWearableSelected(entityID)) {
            sendToQml({'method' : 'selectAvatarEntity', 'entityID' : selectedAvatarEntityID});
        }
    }
}

function onAddingWearable(entityID) {
    updateAvatarWearables(currentAvatar, function() {
        sendToQml({'method' : 'updateAvatarInBookmarks'});
    });
    sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});
}

function onDeletingWearable(entityID) {
    updateAvatarWearables(currentAvatar, function() {
        sendToQml({'method' : 'updateAvatarInBookmarks'});
    });
    sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});
}

function handleWearableMessages(channel, message, sender) {
    if (channel !== 'Hifi-Object-Manipulation') {
        return;
    }

    var parsedMessage = null;

    try {
        parsedMessage = JSON.parse(message);
    } catch (e) {
        return;
    }

    var entityID = parsedMessage.grabbedEntity;

    var updateWearable = function() {
        // for some reasons Entities.getEntityProperties returns more than was asked..
        var propertyNames = ['localPosition', 'localRotation', 'dimensions', 'naturalDimensions'];
        var entityProperties = Entities.getEntityProperties(selectedAvatarEntityID, propertyNames);
        var properties = {};

        propertyNames.forEach(function(propertyName) {
            properties[propertyName] = entityProperties[propertyName];
        });

        properties.localRotationAngles = Quat.safeEulerAngles(properties.localRotation);
        sendToQml({'method' : 'wearableUpdated', 'entityID' : selectedAvatarEntityID,
                   'wearableIndex' : -1, 'properties' : properties, updateUI : true});

    };

    if(parsedMessage.action === 'grab') {
        if(selectedAvatarEntityID !== entityID) {
            ensureWearableSelected(entityID);
            sendToQml({'method' : 'selectAvatarEntity', 'entityID' : selectedAvatarEntityID});
        }

        grabbedAvatarEntityChangeNotifier = Script.setInterval(updateWearable, 1000);
    } else if(parsedMessage.action === 'release') {
        if(grabbedAvatarEntityChangeNotifier !== null) {
            Script.clearInterval(grabbedAvatarEntityChangeNotifier);
            grabbedAvatarEntityChangeNotifier = null;
            updateWearable();
        }
    }
}

function sendToQml(message) {
    tablet.sendToQml(message);
}

function onBookmarkLoaded(bookmarkName) {
    executeLater(function() {
        currentAvatar = getMyAvatar();
        sendToQml({'method' : 'bookmarkLoaded', 'data' : {'name' : bookmarkName, 'currentAvatar' : currentAvatar} });
    });
}

function onBookmarkDeleted(bookmarkName) {
    sendToQml({'method' : 'bookmarkDeleted', 'name' : bookmarkName});
}

function onBookmarkAdded(bookmarkName) {
    var bookmark = AvatarBookmarks.getBookmark(bookmarkName);
    bookmark.avatarEntites.forEach(function(avatarEntity) {
        avatarEntity.properties.localRotationAngles = Quat.safeEulerAngles(avatarEntity.properties.localRotation);
    });

    sendToQml({ 'method': 'bookmarkAdded', 'bookmarkName': bookmarkName, 'bookmark': bookmark });
}

//
// Manage the connection between the button and the window.
//
var button;
var buttonName = "AVATAR";
var tablet = null;

function startup() {
    tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    button = tablet.addButton({
        text: buttonName,
        icon: "icons/tablet-icons/avatar-i.svg",
        activeIcon: "icons/tablet-icons/avatar-a.svg",
        sortOrder: 7
    });
    button.clicked.connect(onTabletButtonClicked);
    tablet.screenChanged.connect(onTabletScreenChanged);
}

startup();

var isWired = false;
function off() {
    if(adjustWearables.opened) {
        adjustWearables.setOpened(false);
        ensureWearableSelected(null);
        Entities.mousePressOnEntity.disconnect(onSelectedEntity);

        Messages.messageReceived.disconnect(handleWearableMessages);
        Messages.unsubscribe('Hifi-Object-Manipulation');
    }

    if (isWired) { // It is not ok to disconnect these twice, hence guard.
        isWired = false;

        AvatarBookmarks.bookmarkLoaded.disconnect(onBookmarkLoaded);
        AvatarBookmarks.bookmarkDeleted.disconnect(onBookmarkDeleted);
        AvatarBookmarks.bookmarkAdded.disconnect(onBookmarkAdded);

        Entities.addingWearable.disconnect(onAddingWearable);
        Entities.deletingWearable.disconnect(onDeletingWearable);
        MyAvatar.skeletonModelURLChanged.disconnect(onSkeletonModelURLChanged);
        MyAvatar.dominantHandChanged.disconnect(onDominantHandChanged);
        MyAvatar.hmdAvatarAlignmentTypeChanged.disconnect(onHmdAvatarAlignmentTypeChanged);
        MyAvatar.collisionsEnabledChanged.disconnect(onCollisionsEnabledChanged);
        MyAvatar.otherAvatarsCollisionsEnabledChanged.disconnect(onOtherAvatarsCollisionsEnabledChanged);
        MyAvatar.newCollisionSoundURL.disconnect(onNewCollisionSoundUrl);
        MyAvatar.animGraphUrlChanged.disconnect(onAnimGraphUrlChanged);
        MyAvatar.targetScaleChanged.disconnect(onTargetScaleChanged);
    }
}

function on() {

    if (!isWired) { // It is not ok to connect these twice, hence guard.
        isWired = true;

        AvatarBookmarks.bookmarkLoaded.connect(onBookmarkLoaded);
        AvatarBookmarks.bookmarkDeleted.connect(onBookmarkDeleted);
        AvatarBookmarks.bookmarkAdded.connect(onBookmarkAdded);

        Entities.addingWearable.connect(onAddingWearable);
        Entities.deletingWearable.connect(onDeletingWearable);
        MyAvatar.skeletonModelURLChanged.connect(onSkeletonModelURLChanged);
        MyAvatar.dominantHandChanged.connect(onDominantHandChanged);
        MyAvatar.hmdAvatarAlignmentTypeChanged.connect(onHmdAvatarAlignmentTypeChanged);
        MyAvatar.collisionsEnabledChanged.connect(onCollisionsEnabledChanged);
        MyAvatar.otherAvatarsCollisionsEnabledChanged.connect(onOtherAvatarsCollisionsEnabledChanged);
        MyAvatar.newCollisionSoundURL.connect(onNewCollisionSoundUrl);
        MyAvatar.animGraphUrlChanged.connect(onAnimGraphUrlChanged);
        MyAvatar.targetScaleChanged.connect(onTargetScaleChanged);
    }
}

function onTabletButtonClicked() {
    if (onAvatarAppScreen) {
        // for toolbar-mode: go back to home screen, this will close the window.
        tablet.gotoHomeScreen();
    } else {
        tablet.loadQMLSource(AVATARAPP_QML_SOURCE);
    }
}
var hasEventBridge = false;
function wireEventBridge(on) {
    if (on) {
        if (!hasEventBridge) {
            tablet.fromQml.connect(fromQml);
            hasEventBridge = true;
        }
    } else {
        if (hasEventBridge) {
            tablet.fromQml.disconnect(fromQml);
            hasEventBridge = false;
        }
    }
}

var onAvatarAppScreen = false;
function onTabletScreenChanged(type, url) {
    var onAvatarAppScreenNow = (type === "QML" && url === AVATARAPP_QML_SOURCE);
    wireEventBridge(onAvatarAppScreenNow);
    // for toolbar mode: change button to active when window is first openend, false otherwise.
    button.editProperties({isActive: onAvatarAppScreenNow});

    if (!onAvatarAppScreen && onAvatarAppScreenNow) {
        on();
    } else if(onAvatarAppScreen && !onAvatarAppScreenNow) {
        off();
    }

    onAvatarAppScreen = onAvatarAppScreenNow;

    if(onAvatarAppScreenNow) {
        sendToQml({ 'method' : 'initialize', 'data' : { jointNames : MyAvatar.getJointNames() }});
        sendToQml({ 'method' : 'wearablesFrozenChanged', 'wearablesFrozen' : getWearablesFrozen()});
    }
}

function shutdown() {
    if (onAvatarAppScreen) {
        tablet.gotoHomeScreen();
    }
    button.clicked.disconnect(onTabletButtonClicked);
    tablet.removeButton(button);
    tablet.screenChanged.disconnect(onTabletScreenChanged);

    off();
}

//
// Cleanup.
//
Script.scriptEnding.connect(shutdown);

}()); // END LOCAL_SCOPE
