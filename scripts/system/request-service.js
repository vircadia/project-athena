"use strict";
//
// request-service.js
//
// Created by Howard Stearns on May 22, 2018
// Copyright 2018 High Fidelity, Inc
//
// Distributed under the Apache License, Version 2.0
// See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() { // BEGIN LOCAL_SCOPE

    // QML has its own XMLHttpRequest, but:
    // - npm request is easier to use.
    // - It is not easy to hack QML's XMLHttpRequest to use our MetaverseServer, and to supply the user's auth when contacting it.
    //   a. Our custom XMLHttpRequestClass object only works with ScriptEngine, not QML's javascript.
    //   b. We have hacked profiles that intercept requests to our MetavserseServer (providing the correct auth), but those
    //      only work in QML WebEngineView. Setting up communication between ordinary QML and a hiddent WebEngineView is
    //      tantamount to the following anyway, and would still have to duplicate the code from request.js.

    // So, this script does two things:
    // 1. Allows any root .qml to signal sendToScript({id: aString, method: 'http.request', params: byNameOptions})
    //    We will then asynchonously call fromScript({id: theSameString, method: 'http.response', error: errorOrFalsey, response: body})
    //    on that root object.  
    //    RootHttpRequest.qml does this.
    // 2. If the uri used (computed from byNameOptions, see request.js) is to our metaverse, we will use the appropriate auth.

    var request = Script.require('request').request;
    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    function fromQml(message) { // messages are {id, method, params}, like json-rpc. See also sendToQml.
        switch (message.method) {
        case 'http.request':
            request(message.params, function (error, response) {
                tablet.sendToQml({
                    id: message.id,
                    method: 'http.response',
                    error: error, // Alas, this isn't always a JSON-RPC conforming error object.
                    response: response,
                   jsonrpc: '2.0'
                });
            });
            break;
        }
    }
    tablet.fromQml.connect(fromQml);
    Script.scriptEnding.connect(function () { tablet.fromQml.disconnect(fromQml); });
}()); // END LOCAL_SCOPE    
