'use strict';

//
//  resizer.js
//
//  Created by Kalila L. on Feb 14 2021.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var resizeBlock;
var initialPressPoint;

function createResizeBlock (parentTo) {
    var parentProperties = Entities.getEntityProperties(parentTo);
    
    var resizeBlock = Entities.addEntity({
        type: "Box",
        position: parentProperties.boundingBox.tfl,
        dimensions: Vec3.HALF,
        parentID: parentTo
    }, "local");
    
    return resizeBlock;
}

function destroyResizeBlock() {
    Entities.deleteEntity(resizeBlock);
    resizeBlock = null;
}

function getResizeBlock() {
    return resizeBlock;
}

function onEntityClicked (entityID, event) {
    if (entityID === resizeBlock) {
        
    }
});

///////////////// BOOTSTRAPPING

function startup() {
    Entities.clickDownOnEntity.connect(onEntityClicked);
}

startup();

Script.scriptEnding.connect(function () {
    Entities.clickDownOnEntity.disconnect(onEntityClicked);
    destroyResizeBlock();
});

module.exports = {
    createResizeBlock: createResizeBlock,
    destroyResizeBlock: destroyResizeBlock,
    getResizeBlock: getResizeBlock
}