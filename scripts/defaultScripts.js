"use strict";
/* jslint vars: true, plusplus: true */

//
//  defaultScripts.js
//  examples
//
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var DEFAULT_SCRIPTS_COMBINED = [
    "system/request-service.js",
    "system/progress.js",
    "system/away.js",
    "system/audio.js",
    "system/hmd.js",
    "system/menu.js",
    "system/bubble.js",
    "system/snapshot.js",
    "system/pal.js", // "system/mod.js", // older UX, if you prefer
    "system/avatarapp.js",
    "system/graphicsSettings.js",
    "system/makeUserConnection.js",
    "system/notifications.js",
    "system/create/edit.js",
    "system/dialTone.js",
    "system/firstPersonHMD.js",
    "system/tablet-ui/tabletUI.js",
    "system/emote.js",
    "system/miniTablet.js",
    "system/audioMuteOverlay.js",
    "system/inspect.js",
    "system/keyboardShortcuts/keyboardShortcuts.js",
    "system/checkForUpdates.js",
    "system/onEscape.js",
    "system/onFirstRun.js",
    "system/appreciate/appreciate_app.js"
];
var DEFAULT_SCRIPTS_SEPARATE = [
    "system/controllers/controllerScripts.js",
    "system/ui/nyx/dist/nyx.js",
    "communityScripts/notificationCore/notificationCore.js",
    "simplifiedUI/ui/simplifiedNametag/simplifiedNametag.js",
    {"stable": "system/more/app-more.js", "beta": "https://cdn.vircadia.com/community-apps/more/app-more.js"},
    {"stable": "communityScripts/explore/explore.js", "beta": "https://metaverse.vircadia.com/interim/d-goto/app/explore.js"},
    {"stable": "communityScripts/chat/FloofChat.js", "beta": "https://content.fluffy.ws/scripts/chat/FloofChat.js"}
    //"system/chat.js"
];

if (Window.interstitialModeEnabled) {
    // Insert interstitial scripts at front so that they're started first.
    DEFAULT_SCRIPTS_COMBINED.splice(0, 0, "system/interstitialPage.js", "system/redirectOverlays.js");
}

// add a menu item for debugging
var MENU_CATEGORY = "Developer > Scripting";
var MENU_ITEM = "Debug defaultScripts.js";

var MENU_BETA_DEFAULT_SCRIPTS_CATEGORY = "Developer > Scripting";
var MENU_BETA_DEFAULT_SCRIPTS_ITEM = "Enable Beta Default Scripts";

var SETTINGS_KEY = '_debugDefaultScriptsIsChecked';
var SETTINGS_KEY_BETA = '_betaDefaultScriptsIsChecked';
var previousSetting = Settings.getValue(SETTINGS_KEY, false);
var previousSettingBeta = Settings.getValue(SETTINGS_KEY_BETA, false);

if (previousSetting === '' || previousSetting === 'false') {
    previousSetting = false;
}

if (previousSetting === 'true') {
    previousSetting = true;
}

if (previousSettingBeta === '' || previousSettingBeta === 'false') {
    previousSettingBeta = false;
}

if (previousSettingBeta === 'true') {
    previousSettingBeta = true;
}

if (Menu.menuExists(MENU_CATEGORY) && !Menu.menuItemExists(MENU_CATEGORY, MENU_ITEM)) {
    Menu.addMenuItem({
        menuName: MENU_CATEGORY,
        menuItemName: MENU_ITEM,
        isCheckable: true,
        isChecked: previousSetting
    });
}

if (Menu.menuExists(MENU_BETA_DEFAULT_SCRIPTS_CATEGORY) 
    && !Menu.menuItemExists(MENU_BETA_DEFAULT_SCRIPTS_CATEGORY, MENU_BETA_DEFAULT_SCRIPTS_ITEM)) {
        Menu.addMenuItem({
            menuName: MENU_BETA_DEFAULT_SCRIPTS_CATEGORY,
            menuItemName: MENU_BETA_DEFAULT_SCRIPTS_ITEM,
            isCheckable: true,
            isChecked: previousSettingBeta
        });
}

function loadSeparateDefaults() {
    var currentlyRunningScripts = ScriptDiscoveryService.getRunning();

    for (var i in DEFAULT_SCRIPTS_SEPARATE) {
        var shouldLoadCurrentDefaultScript = true;
        var scriptItem = DEFAULT_SCRIPTS_SEPARATE[i];
        if (typeof scriptItem === "object") {
            if (previousSettingBeta) {
                console.log("Loading Beta item " + scriptItem.beta);
                scriptItem = scriptItem.beta;
            } else {
                scriptItem = scriptItem.stable;
            }
        }

        for (var j = 0; j < currentlyRunningScripts.length; j++) {
            var currentRunningScriptObject = currentlyRunningScripts[j];
            var currentDefaultScriptName = scriptItem.substr((scriptItem.lastIndexOf("/") + 1), scriptItem.length);
            if (currentDefaultScriptName === currentRunningScriptObject.name) {
                if (currentRunningScriptObject.url !== scriptItem) {
                    ScriptDiscoveryService.stopScript(currentRunningScriptObject.url);
                } else {
                    shouldLoadCurrentDefaultScript = false;
                }
            }
        }

        if (shouldLoadCurrentDefaultScript) {
            Script.load(scriptItem);
        }
    }
}

function runDefaultsTogether() {
    var currentlyRunningScripts = ScriptDiscoveryService.getRunning();

    for (var i = 0; i < DEFAULT_SCRIPTS_COMBINED.length; i++) {
        var shouldIncludeCurrentDefaultScript = true;

        for (var j = 0; j < currentlyRunningScripts.length; j++) {
            var currentRunningScriptObject = currentlyRunningScripts[j];
            var currentDefaultScriptName = DEFAULT_SCRIPTS_COMBINED[i].substr((DEFAULT_SCRIPTS_COMBINED[i].lastIndexOf("/") + 1), DEFAULT_SCRIPTS_COMBINED[i].length);
            if (currentDefaultScriptName === currentRunningScriptObject.name) {
                shouldIncludeCurrentDefaultScript = false;
            }
        }

        if (shouldIncludeCurrentDefaultScript) {
            Script.include(DEFAULT_SCRIPTS_COMBINED[i]);
        }
    }
}

function runDefaultsSeparately() {
    var currentlyRunningScripts = ScriptDiscoveryService.getRunning();

    for (var i in DEFAULT_SCRIPTS_COMBINED) {
        var shouldLoadCurrentDefaultScript = true;

        for (var j = 0; j < currentlyRunningScripts.length; j++) {
            var currentRunningScriptObject = currentlyRunningScripts[j];
            var currentDefaultScriptName = DEFAULT_SCRIPTS_COMBINED[i].substr((DEFAULT_SCRIPTS_COMBINED[i].lastIndexOf("/") + 1), DEFAULT_SCRIPTS_COMBINED[i].length);
            if (currentDefaultScriptName === currentRunningScriptObject.name) {
                shouldLoadCurrentDefaultScript = false;
            }
        }

        if (shouldLoadCurrentDefaultScript) {
            Script.load(DEFAULT_SCRIPTS_COMBINED[i]);
        }
    }
}

// start all scripts
if (Menu.isOptionChecked(MENU_ITEM)) {
    // we're debugging individual default scripts
    // so we load each into its own ScriptEngine instance
    runDefaultsSeparately();
} else {
    // include all default scripts into this ScriptEngine
    runDefaultsTogether();
}
loadSeparateDefaults();

function menuItemEvent(menuItem) {
    if (menuItem === MENU_ITEM) {
        var isChecked = Menu.isOptionChecked(MENU_ITEM);
        if (isChecked) {
            Settings.setValue(SETTINGS_KEY, true);
        } else {
            Settings.setValue(SETTINGS_KEY, false);
        }
        Menu.triggerOption("Reload All Scripts");
    } 
    if (menuItem === MENU_BETA_DEFAULT_SCRIPTS_ITEM) {
        var isChecked = Menu.isOptionChecked(MENU_BETA_DEFAULT_SCRIPTS_ITEM);
        if (isChecked) {
            Settings.setValue(SETTINGS_KEY_BETA, true);
        } else {
            Settings.setValue(SETTINGS_KEY_BETA, false);
        }
    }
}

function removeMenuItems() {
    if (!Menu.isOptionChecked(MENU_ITEM)) {
        Menu.removeMenuItem(MENU_CATEGORY, MENU_ITEM);
    }
    if (!Menu.isOptionChecked(MENU_BETA_DEFAULT_SCRIPTS_ITEM)) {
        Menu.removeMenuItem(MENU_BETA_DEFAULT_SCRIPTS_CATEGORY, MENU_BETA_DEFAULT_SCRIPTS_ITEM);
    }
}

Script.scriptEnding.connect(function () {
    removeMenuItems();
});

Menu.menuItemEvent.connect(menuItemEvent);
