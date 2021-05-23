//
//  WebSocketClass.cpp
//  libraries/script-engine/src/
//
//  Created by Thijs Wenker on 8/4/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  This class is an implementation of the WebSocket object for scripting use.  It provides a near-complete implementation
//  of the class described in the Mozilla docs: https://developer.mozilla.org/en-US/docs/Web/API/WebSocket
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WebSocketClass.h"

#include "ScriptContext.h"
#include "ScriptEngine.h"
#include "ScriptEngineLogging.h"
#include "ScriptValue.h"

WebSocketClass::WebSocketClass(ScriptEngine* engine, QString url) :
    _webSocket(new QWebSocket()),
    _engine(engine)
{
    initialize();
    _webSocket->open(url);
}

WebSocketClass::WebSocketClass(ScriptEngine* engine, QWebSocket* qWebSocket) :
    _webSocket(qWebSocket),
    _engine(engine)
{
    initialize();
}

void WebSocketClass::initialize() {
    connect(_webSocket, &QWebSocket::disconnected, this, &WebSocketClass::handleOnClose);
    connect(_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketClass::handleOnMessage);
    connect(_webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketClass::handleOnBinaryMessage);
    connect(_webSocket, &QWebSocket::connected, this, &WebSocketClass::handleOnOpen);
    connect(_webSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
        &WebSocketClass::handleOnError);
    _binaryType = QStringLiteral("arraybuffer");
}

ScriptValuePointer WebSocketClass::constructor(ScriptContext* context, ScriptEngine* engine) {
    QString url;
    if (context->argumentCount() > 0) {
        url = context->argument(0)->toString();
    }
    return engine->newQObject(new WebSocketClass(engine, url), ScriptEngine::ScriptOwnership);
}

WebSocketClass::~WebSocketClass() {
    _webSocket->deleteLater();
}

void WebSocketClass::send(ScriptValuePointer message) {
    if (message->isObject()) {
        QByteArray ba = scriptvalue_cast<QByteArray>(message);
        _webSocket->sendBinaryMessage(ba);
    } else {
        _webSocket->sendTextMessage(message->toString());
    }
}

void WebSocketClass::close() {
    this->close(QWebSocketProtocol::CloseCodeNormal);
}

void WebSocketClass::close(QWebSocketProtocol::CloseCode closeCode) {
    this->close(closeCode, QStringLiteral(""));
}

void WebSocketClass::close(QWebSocketProtocol::CloseCode closeCode, QString reason) {
    _webSocket->close(closeCode, reason);
}

/*@jsdoc
 * Called when the connection closes.
 * @callback WebSocket~onCloseCallback
 * @param {WebSocket.CloseData} data - Information on the connection closure.
 */
/*@jsdoc
 * Information on a connection being closed.
 * @typedef {object} WebSocket.CloseData
 * @property {WebSocket.CloseCode} code - The reason why the connection was closed.
 * @property {string} reason - Description of the reason why the connection was closed.
 * @property {boolean} wasClean - <code>true</code> if the connection closed cleanly, <code>false</code> if it didn't.
 */
void WebSocketClass::handleOnClose() {
    bool hasError = (_webSocket->error() != QAbstractSocket::UnknownSocketError);
    if (_onCloseEvent->isFunction()) {
        ScriptValueList args;
        ScriptValuePointer arg = _engine->newObject();
        arg->setProperty("code", hasError ? QWebSocketProtocol::CloseCodeAbnormalDisconnection : _webSocket->closeCode());
        arg->setProperty("reason", _webSocket->closeReason());
        arg->setProperty("wasClean", !hasError);
        args << arg;
        _onCloseEvent->call(ScriptValuePointer(), args);
    }
}

/*@jsdoc
 * Called when an error occurs.
 * @callback WebSocket~onErrorCallback
 * @param {WebSocket.SocketError} error - The error.
 */
/*@jsdoc
 * <p>The type of socket error.</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>ConnectionRefusedError</td><td>The connection was refused or timed out.</td></tr>
 *     <tr><td><code>1</code></td><td>RemoteHostClosedError</td><td>The remote host closed the connection.</td></tr>
 *     <tr><td><code>2</code></td><td>HostNotFoundError</td><td>The host address was not found.</td></tr>
 *     <tr><td><code>3</code></td><td>SocketAccessError</td><td>The socket operation failed because the application doesn't have
 *       the necessary privileges.</td></tr>
 *     <tr><td><code>4</code></td><td>SocketResourceError</td><td>The local system ran out of resources (e.g., too many 
 *       sockets).</td></tr>
 *     <tr><td><code>5</code></td><td>SocketTimeoutError</td><td>The socket operation timed out.</td></tr>
 *     <tr><td><code>6</code></td><td>DatagramTooLargeError</td><td>The datagram was larger than the OS's limit.</td></tr>
 *     <tr><td><code>7</code></td><td>NetworkError</td><td>An error occurred with the network.</td></tr>
 *     <tr><td><code>8</code></td><td>AddressInUseError</td><td>The is already in use and cannot be reused.</td></tr>
 *     <tr><td><code>9</code></td><td>SocketAddressNotAvailableError</td><td>The address specified does not belong to the 
 *       host.</td></tr>
 *     <tr><td><code>10</code></td><td>UnsupportedSocketOperationError</td><td>The requested socket operation is not supported
 *       by the local OS.</td></tr>
 *     <tr><td><code>11</code></td><td>ProxyAuthenticationRequiredError</td><td>The socket is using a proxy and requires 
 *       authentication.</td></tr>
 *     <tr><td><code>12</code></td><td>SslHandshakeFailedError</td><td>The SSL/TLS handshake failed.</td></tr>
 *     <tr><td><code>13</code></td><td>UnfinishedSocketOperationError</td><td>The last operation has not finished yet.</td></tr>
 *     <tr><td><code>14</code></td><td>ProxyConnectionRefusedError</td><td>Could not contact the proxy server because connection 
 *       was denied.</td></tr>
 *     <tr><td><code>15</code></td><td>ProxyConnectionClosedError</td><td>The connection to the proxy server was unexpectedly 
 *       closed.</td></tr>
 *     <tr><td><code>16</code></td><td>ProxyConnectionTimeoutError</td><td>The connection to the proxy server timed
 *       out.</td></tr>
 *     <tr><td><code>17</code></td><td>ProxyNotFoundError</td><td>The proxy address was not found.</td></tr>
 *     <tr><td><code>18</code></td><td>ProxyProtocolError</td><td>Connection to the proxy server failed because the server 
 *       response could not be understood.</td></tr>
 *     <tr><td><code>19</code></td><td>OperationError</td><td>An operation failed because the socket state did not permit 
 *       it.</td></tr>
 *     <tr><td><code>20</code></td><td>SslInternalError</td><td>Internal error in the SSL library being used.</td></tr>
 *     <tr><td><code>21</code></td><td>SslInvalidUserDataError</td><td>Error in the SSL library because of invalid 
 *       data.</td></tr>
 *     <tr><td><code>22</code></td><td>TemporaryError</td><td>A temporary error occurred.</td></tr>
 *     <tr><td><code>-1</code></td><td>UnknownSocketError</td><td>An unknown error occurred.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {number} WebSocket.SocketError
 */
void WebSocketClass::handleOnError(QAbstractSocket::SocketError error) {
    if (_onErrorEvent->isFunction()) {
        _onErrorEvent->call();
    }
}

/*@jsdoc
 * Triggered when a message is received.
 * @callback WebSocket~onMessageCallback
 * @param {WebSocket.MessageData} message - The message received.
 */
/*@jsdoc
 * A message received on a WebSocket connection.
 * @typedef {object} WebSocket.MessageData
 * @property {string} data - The message content.
 */
void WebSocketClass::handleOnMessage(const QString& message) {
    if (_onMessageEvent->isFunction()) {
        ScriptValueList args;
        ScriptValuePointer arg = _engine->newObject();
        arg->setProperty("data", message);
        args << arg;
        _onMessageEvent->call(ScriptValuePointer(), args);
    }
}

void WebSocketClass::handleOnBinaryMessage(const QByteArray& message) {
    if (_onMessageEvent->isFunction()) {
        ScriptValueList args;
        ScriptValuePointer arg = _engine->newObject();
        ScriptValuePointer arrayBuffer = _engine->newArrayBuffer(message);
        if (arrayBuffer->isUndefined()) {
            qCWarning(scriptengine) << "WebSocketClass::handleOnBinaryMessage !ArrayBuffer";
        }
        arg->setProperty("data", arrayBuffer);
        args << arg;
        _onMessageEvent->call(ScriptValuePointer(), args);
    }
}

/*@jsdoc
 * Called when the connection opens.
 * @callback WebSocket~onOpenCallback
 */
void WebSocketClass::handleOnOpen() {
    if (_onOpenEvent->isFunction()) {
        _onOpenEvent->call();
    }
}

ScriptValuePointer qWSCloseCodeToScriptValue(ScriptEngine* engine, const QWebSocketProtocol::CloseCode &closeCode) {
    return engine->newValue(closeCode);
}

void qWSCloseCodeFromScriptValue(const ScriptValuePointer &object, QWebSocketProtocol::CloseCode &closeCode) {
    closeCode = (QWebSocketProtocol::CloseCode)object->toUInt16();
}

ScriptValuePointer webSocketToScriptValue(ScriptEngine* engine, WebSocketClass* const &in) {
    return engine->newQObject(in, ScriptEngine::ScriptOwnership);
}

void webSocketFromScriptValue(const ScriptValuePointer &object, WebSocketClass* &out) {
    out = qobject_cast<WebSocketClass*>(object->toQObject());
}

ScriptValuePointer wscReadyStateToScriptValue(ScriptEngine* engine, const WebSocketClass::ReadyState& readyState) {
    return engine->newValue(readyState);
}

void wscReadyStateFromScriptValue(const ScriptValuePointer& object, WebSocketClass::ReadyState& readyState) {
    readyState = (WebSocketClass::ReadyState)object->toUInt16();
}
