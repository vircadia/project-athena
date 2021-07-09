'use strict';

//
//  rotationUpdater.js
//
//  Created by Kalila L. on Jan 8 2021.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var DEFAULT_ROTATION_UPDATE_INTERVAL = 500; // 500 ms
var entityToUpdate;
var shouldUpdate = false;

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

function startup() {

}

startup();

Script.scriptEnding.connect(function () {

});

module.exports = {
    
}