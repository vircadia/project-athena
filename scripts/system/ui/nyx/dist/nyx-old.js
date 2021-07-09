'use strict';

//
//  nyx.js
//
//  Created by Kalila L. on Oct 6 2020.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

Script.include("/~/system/libraries/utils.js");

var SETTING_NYX_PREFIX = "nyx/";

// BEGIN ENTITY MENU OVERLAY

var enableEntityWebMenu = true;
var entityWebMenu;
var entityWebMenuActive = false;
var registeredEntityMenus = {};
var lastTriggeredEntityInfo = {};
var MENU_WEB_ENTITY_SCALE = {
    x: 1,
    y: 1.5,
    z: 0.01
};
var MENU_WEB_ENTITY_ROTATION_UPDATE_INTERVAL = 500; // 500 ms
var BOOTSTRAP_MENU_WEB_ENTITY_SCALE = {
    x: 0.01,
    y: 0.01,
    z: 0.01
};
var BOOTSTRAP_MENU_WEB_ENTITY_POSITION = Vec3.ZERO;
var BOOTSTRAP_MENU_WEB_ENTITY_SOURCE = Script.resolvePath("./index.html");
var BOOTSTRAP_MENU_WEB_ENTITY_DPI = 7;
var NYX_UI_CHANNEL = "nyx-ui";

function registerWithEntityMenu(messageData) {
    registeredEntityMenus[messageData.entityID] = messageData.menuItems;

    var dataToSend = {
        registeredEntityMenus: registeredEntityMenus
    };

    sendToWeb(entityWebMenu, 'script-to-web-registered-entity-menus', dataToSend);
}

function deregisterWithEntityMenu(messageData) {
    delete registeredEntityMenus[messageData.entityID];
    
    var dataToSend = {
        registeredEntityMenus: registeredEntityMenus
    };

    sendToWeb(entityWebMenu, 'script-to-web-registered-entity-menus', dataToSend);
}

function toggleEntityMenu(pressedEntityID) {
    if (!entityWebMenuActive && enableEntityWebMenu === true) {
        var triggeredEntityProperties = Entities.getEntityProperties(pressedEntityID);
        var lastEditedByName;

        if (AvatarManager.getPalData([triggeredEntityProperties.lastEditedBy]).data[0]) {
            lastEditedByName = AvatarManager.getPalData([triggeredEntityProperties.lastEditedBy]).data[0].sessionDisplayName
        } else {
            lastEditedByName = triggeredEntityProperties.lastEditedBy;
        }

        lastTriggeredEntityInfo = {
            id: pressedEntityID,
            name: triggeredEntityProperties.name,
            type: triggeredEntityProperties.type,
            lastEditedBy: triggeredEntityProperties.lastEditedBy,
            lastEditedByName: lastEditedByName,
            entityHostType: triggeredEntityProperties.entityHostType,
            description: triggeredEntityProperties.description,
            position: triggeredEntityProperties.position,
            rotation: triggeredEntityProperties.rotation
        };

        sendToWeb(entityWebMenu, 'script-to-web-triggered-entity-info', lastTriggeredEntityInfo);

        Entities.editEntity(entityWebMenu, {
            position: Entities.getEntityProperties(pressedEntityID, ['position']).position,
            dimensions: MENU_WEB_ENTITY_SCALE,
            rotation: Camera.orientation,
            visible: true
        });
        
        entityWebMenuActive = true;

        updateEntityMenuRotation();
    } else if (entityWebMenuActive && pressedEntityID !== entityWebMenu) {
        Entities.editEntity(entityWebMenu, {
            position: BOOTSTRAP_MENU_WEB_ENTITY_POSITION,
            dimensions: BOOTSTRAP_MENU_WEB_ENTITY_SCALE
        });
        
        entityWebMenuActive = false;
    }
}

function updateEntityMenuRotation() {
    if (entityWebMenuActive) {
        Script.setTimeout(function() {
            Entities.editEntity(entityWebMenu, {
                rotation: Camera.orientation
            });
            
            updateEntityMenuRotation();
        }, MENU_WEB_ENTITY_ROTATION_UPDATE_INTERVAL);
    }
}

function bootstrapEntityMenu() {
    entityWebMenu = Entities.addEntity({
        type: "Web",
        billboardMode: 'full',
        renderLayer: 'hud',
        visible: false,
        grab: {
            'grabbable': false
        },
        rotation: Camera.orientation,
        maxFPS: 30,
        sourceUrl: BOOTSTRAP_MENU_WEB_ENTITY_SOURCE,
        position: BOOTSTRAP_MENU_WEB_ENTITY_POSITION,
        dimensions: BOOTSTRAP_MENU_WEB_ENTITY_SCALE,
        dpi: BOOTSTRAP_MENU_WEB_ENTITY_DPI
    }, 'local');
}

// END ENTITY MENU OVERLAY

function sendToWeb(sendToEntity, command, data) {
    var dataToSend = {
        "command": command,
        "data": data
    };

    Entities.emitScriptEvent(sendToEntity, JSON.stringify(dataToSend));
}

function onWebEventReceived(sendingEntityID, event) {
    if (sendingEntityID === entityWebMenu) {
        var eventJSON = JSON.parse(event);

        if (eventJSON.command === "ready") {
            var dataToSend = {
                registeredEntityMenus: registeredEntityMenus,
                lastTriggeredEntityInfo: lastTriggeredEntityInfo
            };

            sendToWeb(entityWebMenu, 'script-to-web-registered-entity-menus', dataToSend);
        }
        
        if (eventJSON.command === "menu-item-triggered") {
            var dataToSend = {
                entityID: eventJSON.data.triggeredEntityID,
                menuItem: eventJSON.data.menuItem
            };

            Messages.sendLocalMessage(NYX_UI_CHANNEL, JSON.stringify(dataToSend));
            
            if (entityWebMenuActive) {
                toggleEntityMenu(); // Close the menu if a menu item was pressed.
            }
        }
        
        if (eventJSON.command === "close-entity-menu") {
            if (entityWebMenuActive) {
                toggleEntityMenu(); // Close the menu if it is active.
            }
        }

    }
}

function onMessageReceived(channel, message, senderID, localOnly) {
    // print("NYX UI Message received:");
    // print("- channel: " + channel);
    // print("- message: " + message);
    // print("- sender: " + senderID);
    // print("- localOnly: " + localOnly);

    if (channel === NYX_UI_CHANNEL && MyAvatar.sessionUUID === senderID) {
        messageData = JSON.parse(message);
        
        if (messageData.command === 'register-with-entity-menu') {
            registerWithEntityMenu(messageData);
        }
        
        if (messageData.command === 'deregister-with-entity-menu') {
            deregisterWithEntityMenu(messageData);
        }
    }
}

function onMousePressOnEntity (pressedEntityID, event) {
    if (event.isPrimaryHeld && event.isSecondaryHeld && !isInEditMode()) {
        toggleEntityMenu(pressedEntityID);
    }
}

// Nyx Menu

var NYX_MAIN_MENU = "Settings > Nyx";
var NYX_ENTITY_MENU_ENABLED = "Enable Entity Menu";

function handleMenuEvent(menuItem) {
    if (menuItem === NYX_ENTITY_MENU_ENABLED) {
        enableEntityWebMenu = Menu.isOptionChecked(NYX_ENTITY_MENU_ENABLED);
    }
}

function bootstrapNyxMenu() {
    if (!Menu.menuExists(NYX_MAIN_MENU)) {
        Menu.addMenu(NYX_MAIN_MENU);
        Menu.addMenuItem({
            menuName: NYX_MAIN_MENU,
            menuItemName: NYX_ENTITY_MENU_ENABLED,
            isCheckable: true,
            isChecked: Settings.getValue(SETTING_NYX_PREFIX + NYX_ENTITY_MENU_ENABLED, true)
        });
    }
}

function unloadNyxMenu() {
    Settings.setValue(SETTING_NYX_PREFIX + NYX_ENTITY_MENU_ENABLED, Menu.isOptionChecked(NYX_ENTITY_MENU_ENABLED));
    
    Menu.removeMenuItem(NYX_MAIN_MENU, NYX_ENTITY_MENU_ENABLED);
    Menu.removeMenu(NYX_MAIN_MENU);
}

// Nyx Menu

// BOOTSTRAPPING

function startup() {
    Messages.messageReceived.connect(onMessageReceived);
    Entities.mousePressOnEntity.connect(onMousePressOnEntity);
    Entities.webEventReceived.connect(onWebEventReceived);
    Menu.menuItemEvent.connect(handleMenuEvent);
    
    bootstrapNyxMenu();
    bootstrapEntityMenu();
}

startup();

Script.scriptEnding.connect(function () {
    Messages.messageReceived.disconnect(onMessageReceived);
    Entities.mousePressOnEntity.disconnect(onMousePressOnEntity);
    Entities.webEventReceived.disconnect(onWebEventReceived);
    Menu.menuItemEvent.disconnect(handleMenuEvent);

    unloadNyxMenu();

    Entities.deleteEntity(entityWebMenu);
    entityWebMenu = null;
});

// BOOTSTRAPPING TESTING CODE

// var messageToSend = {
//     'command': 'register-with-entity-menu',
//     'entityID': '{768542d0-e962-49e3-94fb-85651d56f5ae}',
//     'menuItems': ['This', 'Is', 'Nice']
// };
// 
// Messages.sendLocalMessage(NYX_UI_CHANNEL, JSON.stringify(messageToSend));