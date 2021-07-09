'use strict';

//
//  sit.js
//
//  Created by Kalila L. on Nov 4 2020.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var PICK_FILTERS = Picks.PICK_ENTITIES | Picks.PICK_OVERLAYS | Picks.PICK_AVATARS | Picks.PICK_INCLUDE_NONCOLLIDABLE;
var HAND_JOINT = '_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND'.replace('RIGHT', MyAvatar.getDominantHand().toUpperCase());
var JOINT_NAME = HMD.active ? HAND_JOINT : 'Mouse';
var SETTING_NYX_PREFIX = 'nyx/';
var NYX_SETTINGS_SETTINGS = 'Nyx Settings'; // lol

var mainPick;
var lastTriggeredPick;
var isAvatarSitting = false;

function toggleSit () {
    if (!isAvatarSitting) {
        var position = lastTriggeredPick.intersection;
        var offset = retrieveOffset();
        var sitPosition = {
            x: position.x + offset.x,
            y: position.y + offset.y,
            z: position.z + offset.z
        };
        MyAvatar.beginSit(sitPosition, MyAvatar.orientation);
        isAvatarSitting = true;
    } else {
        MyAvatar.endSit(MyAvatar.position, MyAvatar.orientation);
        isAvatarSitting = false;
    }
}

function capturePickPosition () {
    lastTriggeredPick = Picks.getPrevPickResult(mainPick);
}

function retrieveOffset () {
    var nyxSettings = Settings.getValue(SETTING_NYX_PREFIX + NYX_SETTINGS_SETTINGS, '');
    var objectToReturn = {
        x: 0,
        y: 0,
        z: 0
    };
    
    if (nyxSettings !== '') {
        objectToReturn.x = Number(nyxSettings.sit.offset.position.x);
        objectToReturn.y = Number(nyxSettings.sit.offset.position.y);
        objectToReturn.z = Number(nyxSettings.sit.offset.position.z);
    }
    
    return objectToReturn;
}

function startup() {
    mainPick = Picks.createPick(PickType.Ray, {
        joint: JOINT_NAME,
        filter: PICK_FILTERS,
        enabled: true
    });
}

startup();

Script.scriptEnding.connect(function () {
    Picks.removePick(mainPick);
});

module.exports = {
    toggleSit: toggleSit,
    capturePickPosition: capturePickPosition
}