//
//  WebSocketServerClass.h
//  libraries/script-engine/src/
//
//  Created by Thijs Wenker on 8/10/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WebSocketServerClass_h
#define hifi_WebSocketServerClass_h

#include <QObject>
#include <QWebSocketServer>
#include "WebSocketClass.h"
#include <QtCore/QSharedPointer>

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * Manages {@link WebSocket}s in server entity and assignment client scripts.
 *
 * <p>Create using <code>new WebSocketServer(...)</code>.</p>
 *
 * @class WebSocketServer
 *
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {string} url - The URL that the server is listening on. <em>Read-only.</em>
 * @property {number} port - The port that the server is listening on. <em>Read-only.</em>
 * @property {boolean} listening - <code>true</code> if the server is listening for incoming connections, <code>false</code> if 
 *     it isn't. <em>Read-only.</em>
 *
 * @example <caption>Echo a message back to sender.</caption>
 * // Server entity script. Echoes received message back to sender.
 * (function () {
 *     print("Create WebSocketServer");
 *     var webSocketServer = new WebSocketServer();
 *     print("Server url:", webSocketServer.url);
 * 
 *     function onNewConnection(webSocket) {
 *         print("New connection");
 * 
 *         webSocket.onmessage = function (message) {
 *             print("Message received:", message.data);
 * 
 *             var returnMessage = message.data + " back!";
 *             print("Echo a message back:", returnMessage);
 *             webSocket.send(message.data + " back!");
 *         };
 *     }
 * 
 *     webSocketServer.newConnection.connect(onNewConnection);
 * })
 * 
 * @example
 * // Interface script. Bounces message off server entity script.
 * // Use the server URL reported by the server entity script.
 * var WEBSOCKET_PING_URL = "ws://127.0.0.1:nnnnn";
 * var TEST_MESSAGE = "Hello";
 * 
 * print("Create WebSocket");
 * var webSocket = new WebSocket(WEBSOCKET_PING_URL);
 * 
 * webSocket.onmessage = function(data) {
 *     print("Message received:", data.data);
 * };
 * 
 * webSocket.onopen = function() {
 *     print("WebSocket opened");
 *     print("Send test message:", TEST_MESSAGE);
 *     webSocket.send(TEST_MESSAGE);
 * };
 */
class WebSocketServerClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString url READ getURL)
    Q_PROPERTY(quint16 port READ getPort)
    Q_PROPERTY(bool listening READ isListening)

public:
    WebSocketServerClass(ScriptEngine* engine, const QString& serverName, const quint16 port);
    ~WebSocketServerClass();

    QString getURL() { return _webSocketServer.serverUrl().toDisplayString(); }
    quint16 getPort() { return _webSocketServer.serverPort(); }
    bool isListening() { return _webSocketServer.isListening(); }

    static ScriptValuePointer constructor(ScriptContext* context, ScriptEngine* engine);

public slots:

    /*@jsdoc
     * Closes all connections and closes the WebSocketServer.
     * @function WebSocketServer.close
     */
    void close();

private:
    QWebSocketServer _webSocketServer;
    ScriptEngine* _engine;
    QList<WebSocketClass*> _clients;

private slots:
    void onNewConnection();

signals:

    /*@jsdoc
     * Triggered when there is a new connection.
     * @function WebSocketServer.newConnection
     * @param {WebSocket} webSocket - The {@link WebSocket} for the new connection.
     * @returns {Signal}
     */
    void newConnection(WebSocketClass* client);

};

#endif // hifi_WebSocketServerClass_h
