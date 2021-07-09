'use strict';

//
//  parentSelf.js
//
//  Created by Kalila L. on Nov 4 2020.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

function toggleParent (parentTo) {
    if (MyAvatar.getParentID() === null || MyAvatar.getParentID() === Uuid.NULL) {
        MyAvatar.setParentID(parentTo);
    } else {
        MyAvatar.setParentID(Uuid.NULL);
    }
}

module.exports = {
    toggleParent: toggleParent
}