'use strict';

//
//  nyx-helpers.js
//
//  Created by Kalila L. on Oct 31 2020.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
//  Bugs: When copying an entity with this script on it, the old one loses its connection?
//

var SETTING_NYX_PREFIX = "nyx/";
var NYX_UI_CHANNEL = "nyx-ui";

///////////////// BEGIN HELPERS

// REGISTER HELPER
function registerWithEntityMenu (entityID, actions) {
    var messageToSend = {
        'command': 'register-with-entity-menu',
        'entityID': entityID,
        'actions': actions
    };

    Messages.sendLocalMessage(NYX_UI_CHANNEL, JSON.stringify(messageToSend));
}

// ENTITY MENU TRIGGERED HELPER
// This is the callback of the function that's been connected, so fulfilling 
// this will fulfill the function that is using the NyxHelper.
var entityMenuCallBack = {};

function connectEntityMenu (entityID, callback) {
    entityMenuCallBack[entityID] = callback;
}

function disconnectEntityMenu (entityID, callback) {
    delete entityMenuCallBack[entityID];
}

// MAIN FUNCTIONALITY

function onLoad() {
    Messages.messageReceived.connect(onMessageReceived);
}

onLoad();

Script.scriptEnding.connect(function () {
    Messages.messageReceived.disconnect(onMessageReceived);
});

function onMessageReceived(channel, message, senderID, localOnly) {
    // print("NYX UI Message received:");
    // print("- channel: " + channel);
    // print("- message: " + message);
    // print("- sender: " + senderID);
    // print("- localOnly: " + localOnly);

    if (channel === NYX_UI_CHANNEL && MyAvatar.sessionUUID === senderID) {
        messageData = JSON.parse(message);
        
        if (messageData.command === "menu-item-triggered" && entityMenuCallBack[messageData.entityID]) {
            entityMenuCallBack[messageData.entityID](messageData.entityID, messageData.command, messageData.data);
        }
    }
}

module.exports = {
    registerWithEntityMenu: registerWithEntityMenu,
    entityMenuActionTriggered: {
        connect: connectEntityMenu,
        disconnect: disconnectEntityMenu
    },
    version: "0.0.2"
}