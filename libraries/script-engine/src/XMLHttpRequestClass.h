//
//  XMLHttpRequestClass.h
//  libraries/script-engine/src/
//
//  Created by Ryan Huffman on 5/2/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_XMLHttpRequestClass_h
#define hifi_XMLHttpRequestClass_h

#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>
#include <QtCore/QSharedPointer>

#include "ScriptEngine.h"

class ScriptContext;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*
XMlHttpRequest object
XMlHttpRequest.objectName string
XMlHttpRequest.response undefined
XMlHttpRequest.responseText string
XMlHttpRequest.responseType string
XMlHttpRequest.status number
XMlHttpRequest.statusText string
XMlHttpRequest.readyState number
XMlHttpRequest.errorCode number
XMlHttpRequest.timeout number
XMlHttpRequest.UNSENT number
XMlHttpRequest.OPENED number
XMlHttpRequest.HEADERS_RECEIVED number
XMlHttpRequest.LOADING number
XMlHttpRequest.DONE number
XMlHttpRequest.ontimeout object
XMlHttpRequest.onreadystatechange object
XMlHttpRequest.destroyed(QObject*) function
XMlHttpRequest.destroyed() function
XMlHttpRequest.objectNameChanged(QString) function
XMlHttpRequest.deleteLater() function
XMlHttpRequest.requestComplete() function
XMlHttpRequest.abort() function
XMlHttpRequest.setRequestHeader(QString,QString) function
XMlHttpRequest.open(QString,QString,bool,QString,QString) function
XMlHttpRequest.open(QString,QString,bool,QString) function
XMlHttpRequest.open(QString,QString,bool) function
XMlHttpRequest.open(QString,QString) function
XMlHttpRequest.send() function
XMlHttpRequest.send(ScriptValuePointer) function
XMlHttpRequest.getAllResponseHeaders() function
XMlHttpRequest.getResponseHeader(QString) function
*/

/*@jsdoc
 * Provides a means to interact with web servers. It is a near-complete implementation of the XMLHttpRequest API described in 
 * the Mozilla docs:
 * <a href="https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest">https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest</a>.
 * 
 * <p>Create using <code>new XMLHttpRequest(...)</code>.</p>
 *
 * @class XMLHttpRequest
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {*} response - The response data.
 *     <em>Read-only.</em>
 * @property {string} responseText - The response data as text.
 *     <em>Read-only.</em>
 * @property {string} responseType - The response type required or received (e.g., <code>"text"</code>, <code>"json"</code>, 
 *     <code>"arraybuffer"</code>, ...).
 * @property {number} status - The <a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Status">HTTP response status 
 *      code</a> (<code>100</code> &ndash; <code>599</code>).
 *     <em>Read-only.</em>
 * @property {string} statusText - The HTTP response status text.
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.ReadyState} readyState - The status of the request.
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.NetworkError} errorCode - The network result of the request: including, <code>0</code> (NoError) 
 *     if there was no error, <code>4</code> (TimeoutError) if the request timed out.
 *     <em>Read-only.</em>
 * @property {number} timeout - The time a request can take before timing out, in ms.
 *
 * @property {XMLHttpRequest.ReadyState} UNSENT - Request has been created; {@link XMLHttpRequest.open} not called yet.
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.ReadyState} OPENED - {@link XMLHttpRequest.open} has been called. 
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.ReadyState} HEADERS_RECEIVED - {@link XMLHttpRequest.send} has been called; headers and status 
 *     are available.
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.ReadyState} LOADING - Downloading; {@link XMLHttpRequest|XMLHttpRequest.responseText} has partial 
 *     data.
 *     <em>Read-only.</em>
 * @property {XMLHttpRequest.ReadyState} DONE - Operation complete.
 *     <em>Read-only.</em>
 *
 * @property {XMLHttpRequest~onTimeoutCallback} ontimeout - Function called when the request times out.
 *     <p>Note: This is called in addition to any function set for <code>onreadystatechange</code>.</p>
 * @property {XMLHttpRequest~onReadyStateChangeCallback} onreadystatechange - Function called when the request's ready state 
 *     changes.
 *
 * @example <caption>Get a web page's HTML.</caption>
 * var URL = "https://www.highfidelity.com/";
 * 
 * var req = new XMLHttpRequest();
 * req.onreadystatechange = function () {
 *     if (req.readyState === req.DONE) {
 *         if (req.status === 200) {
 *             print("Success");
 *             print("Content type:", req.getResponseHeader("content-type"));
 *             print("Content:", req.responseText.slice(0, 100), "...");
 * 
 *         } else {
 *             print("Error", req.status, req.statusText);
 *         }
 * 
 *         req = null;
 *     }
 * };
 * 
 * req.open("GET", URL);
 * req.send();
 *
 * @example <caption>Get a web page's HTML &mdash; alternative method.</caption>
 * var URL = "https://www.highfidelity.com/";
 * 
 * var req = new XMLHttpRequest();
 * req.requestComplete.connect(function () {
 *     if (req.status === 200) {
 *         print("Success");
 *         print("Content type:", req.getResponseHeader("content-type"));
 *         print("Content:", req.responseText.slice(0, 100), "...");
 * 
 *     } else {
 *         print("Error", req.status, req.statusText);
 *     }
 * 
 *     req = null;
 * });
 * 
 * req.open("GET", URL);
 * req.send();
 */
class XMLHttpRequestClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(ScriptValuePointer response READ getResponse)
    Q_PROPERTY(ScriptValuePointer responseText READ getResponseText)
    Q_PROPERTY(QString responseType READ getResponseType WRITE setResponseType)
    Q_PROPERTY(ScriptValuePointer status READ getStatus)
    Q_PROPERTY(QString statusText READ getStatusText)
    Q_PROPERTY(ScriptValuePointer readyState READ getReadyState)
    Q_PROPERTY(ScriptValuePointer errorCode READ getError)
    Q_PROPERTY(int timeout READ getTimeout WRITE setTimeout)

    Q_PROPERTY(int UNSENT READ getUnsent)
    Q_PROPERTY(int OPENED READ getOpened)
    Q_PROPERTY(int HEADERS_RECEIVED READ getHeadersReceived)
    Q_PROPERTY(int LOADING READ getLoading)
    Q_PROPERTY(int DONE READ getDone)

    // Callbacks
    Q_PROPERTY(ScriptValuePointer ontimeout READ getOnTimeout WRITE setOnTimeout)
    Q_PROPERTY(ScriptValuePointer onreadystatechange READ getOnReadyStateChange WRITE setOnReadyStateChange)
public:
    XMLHttpRequestClass(ScriptEngine* engine);
    ~XMLHttpRequestClass();

    static const int MAXIMUM_REDIRECTS = 5;

    /*@jsdoc
     * <p>The state of an XMLHttpRequest.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>0</code></td><td>UNSENT</td><td>Request has been created; {@link XMLHttpRequest.open} not called 
     *       yet.</td></tr>
     *     <tr><td><code>1</code></td><td>OPENED</td><td>{@link XMLHttpRequest.open} has been called.</td></tr>
     *     <tr><td><code>2</code></td><td>HEADERS_RECEIVED</td><td>{@link XMLHttpRequest.send} has been called; headers and 
     *       status are available.</td></tr>
     *     <tr><td><code>3</code></td><td>LOADING</td><td>Downloading; {@link XMLHttpRequest|XMLHttpRequest.responseText} has 
     *       partial data.</td></tr>
     *     <tr><td><code>4</code></td><td>DONE</td><td>Operation complete.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {number} XMLHttpRequest.ReadyState
     */
    enum ReadyState {
        UNSENT = 0,
        OPENED,
        HEADERS_RECEIVED,
        LOADING,
        DONE
    };

    int getUnsent() const { return UNSENT; };
    int getOpened() const { return OPENED; };
    int getHeadersReceived() const { return HEADERS_RECEIVED; };
    int getLoading() const { return LOADING; };
    int getDone() const { return DONE; };

    static ScriptValuePointer constructor(ScriptContext* context, ScriptEngine* engine);

    int getTimeout() const { return _timeout; }
    void setTimeout(int timeout) { _timeout = timeout; }
    ScriptValuePointer getResponse() const { return _responseData; }
    ScriptValuePointer getResponseText() const { return _engine->newValue(QString(_rawResponseData.data())); }
    QString getResponseType() const { return _responseType; }
    void setResponseType(const QString& responseType) { _responseType = responseType; }
    ScriptValuePointer getReadyState() const { return _engine->newValue(_readyState); }
    ScriptValuePointer getError() const { return _engine->newValue(_errorCode); }
    ScriptValuePointer getStatus() const;
    QString getStatusText() const;

    ScriptValuePointer getOnTimeout() const { return _onTimeout; }
    void setOnTimeout(ScriptValuePointer function) { _onTimeout = function; }
    ScriptValuePointer getOnReadyStateChange() const { return _onReadyStateChange; }
    void setOnReadyStateChange(ScriptValuePointer function) { _onReadyStateChange = function; }

public slots:

    /*@jsdoc
     * Aborts the request.
     * @function XMLHttpRequest.abort
     */
    void abort();

    /*@jsdoc
     * Sets the value of an HTTP request header. Must be called after {@link XMLHttpRequest.open} but before 
     * {@link XMLHttpRequest.send};
     * @function XMLHttpRequest.setRequestHeader
     * @param {string} name - The name of the header to set.
     * @param {string} value - The value of the header.
     */
    void setRequestHeader(const QString& name, const QString& value);

    /*@jsdoc
     * Initializes a request.
     * @function XMLHttpRequest.open
     * @param {string} method - The <a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods">HTTP request method</a> 
     *     to use, e.g., <code>"GET"</code>, <code>"POST"</code>, <code>"PUT"</code>, or <code>"DELETE"</code>.
     * @param {string} url - The URL to send the request to.
     * @param {boolean} [async=true] - <code>true</code> if the method returns without waiting for the response, 
     *     <code>false</code> if the method returns only once the request is complete.
     * @param {string} [username=""] - User name for authentication.
     * @param {string} [password=""] - Password for authentication.
     */
    void open(const QString& method, const QString& url, bool async = true, const QString& username = "",
              const QString& password = "");

    /*@jsdoc
     * Sends the request to the server.
     * @function XMLHttpRequest.send
     * @param {*} [data] - The data to send.
     */
    void send();
    void send(const ScriptValuePointer& data);

    /*@jsdoc
     * Gets the response headers.
     * @function XMLHttpRequest.getAllResponseHeaders
     * @returns {string} The response headers, separated by <code>"\n"</code> characters.
     */
    ScriptValuePointer getAllResponseHeaders() const;

    /*@jsdoc
     * Gets a response header.
     * @function XMLHttpRequest.getResponseHeader
     * @param {string} name - 
     * @returns {string} The response header.
     */
    ScriptValuePointer getResponseHeader(const QString& name) const;

signals:

    /*@jsdoc
     * Triggered when the request is complete &mdash; with or without error (incl. timeout).
     * @function XMLHttpRequest.requestComplete
     * @returns {Signal}
     */
    void requestComplete();

private:
    void setReadyState(ReadyState readyState);
    void doSend();
    void connectToReply(QNetworkReply* reply);
    void disconnectFromReply(QNetworkReply* reply);
    void abortRequest();

    ScriptEngine* _engine { nullptr };
    bool _async { true };
    QUrl _url;
    QString _method;
    QString _responseType;
    QNetworkRequest _request;
    QNetworkReply* _reply { nullptr };
    QByteArray _sendData;
    QByteArray _rawResponseData;
    ScriptValuePointer _responseData;
    ScriptValuePointer _onTimeout;
    ScriptValuePointer _onReadyStateChange;
    ReadyState _readyState { XMLHttpRequestClass::UNSENT };

    /*@jsdoc
     * <p>The type of network error.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>0</code></td><td>NoError</td><td>No error.</td></tr>
     *     <tr><td><code>1</code></td><td>ConnectionRefusedError</td><td>The server refused the connection.</td></tr>
     *     <tr><td><code>2</code></td><td>RemoteHostClosedError</td><td>The server closed the connection.</td></tr>
     *     <tr><td><code>3</code></td><td>HostNotFoundError</td><td>Host name not found.</td></tr>
     *     <tr><td><code>4</code></td><td>TimeoutError</td><td>Connection timed out</td></tr>
     *     <tr><td><code>5</code></td><td>OperationCanceledError</td><td>Operation canceled by 
     *       {@link XMLHttpRequest.abort}.</td></tr>
     *     <tr><td><code>6</code></td><td>SslHandshakeFailedError</td><td>SSL/TLS handshake failed.</td></tr>
     *     <tr><td><code>7</code></td><td>TemporaryNetworkFailureError</td><td>Temporarily disconnected from the 
     *       network.</td></tr>
     *     <tr><td><code>8</code></td><td>NetworkSessionFailedError</td><td>Disconnection from the network.</td></tr>
     *     <tr><td><code>9</code></td><td>BackgroundRequestNotAllowedError</td><td>Background request not allowed.</td></tr>
     *     <tr><td><code>10</code></td><td>TooManyRedirectsError</td><td>Too many redirects.</td></tr>
     *     <tr><td><code>11</code></td><td>InsecureRedirectError</td><td>Redirect from secure to insecure protocol.</td></tr>
     *     <tr><td><code>101</code></td><td>ProxyConnectionRefusedError</td><td>Connection to proxy server refused.</td></tr>
     *     <tr><td><code>102</code></td><td>ProxyConnectionClosedError</td><td>Proxy server closed the connection.</td></tr>
     *     <tr><td><code>103</code></td><td>ProxyNotFoundError</td><td>Proxy host name not found.</td></tr>
     *     <tr><td><code>104</code></td><td>ProxyTimeoutError</td><td>Proxy connection timed out.</td></tr>
     *     <tr><td><code>105</code></td><td>ProxyAuthenticationRequiredError</td><td>Proxy requires authentication.</td></tr>
     *     <tr><td><code>201</code></td><td>ContentAccessDenied</td><td>Access denied.</td></tr>
     *     <tr><td><code>202</code></td><td>ContentOperationNotPermittedError</td><td>Operation not permitted.</td></tr>
     *     <tr><td><code>203</code></td><td>ContentNotFoundError</td><td>Content not found.</td></tr>
     *     <tr><td><code>204</code></td><td>AuthenticationRequiredError</td><td>Authentication required.</td></tr>
     *     <tr><td><code>205</code></td><td>ContentReSendError</td><td>Resend failed.</td></tr>
     *     <tr><td><code>206</code></td><td>ContentConflictError</td><td>Resource state conflict.</td></tr>
     *     <tr><td><code>207</code></td><td>ContentGoneError</td><td>Resource no longer available.</td></tr>
     *     <tr><td><code>401</code></td><td>InternalServerError</td><td>Internal server error.</td></tr>
     *     <tr><td><code>402</code></td><td>OperationNotImplementedError</td><td>Operation not supported.</td></tr>
     *     <tr><td><code>403</code></td><td>ServiceUnavailableError</td><td>Request not able to be handled at this 
     *       time.</td></tr>
     *     <tr><td><code>301</code></td><td>ProtocolUnknownError</td><td>Protocol unknown.</td></tr>
     *     <tr><td><code>302</code></td><td>ProtocolInvalidOperationError</td><td>Operation invalid fro protocol.</td></tr>
     *     <tr><td><code>99</code></td><td>UnknownNetworkError</td><td>Unknown network-related error.</td></tr>
     *     <tr><td><code>199</code></td><td>UnknownProxyError</td><td>Unknown proxy-related error.</td></tr>
     *     <tr><td><code>299</code></td><td>UnknownContentError</td><td>Unknown content-related error.</td></tr>
     *     <tr><td><code>399</code></td><td>ProtocolFailure</td><td>Protocol error.</td></tr>
     *     <tr><td><code>499</code></td><td>UnknownServerError</td><td>Unknown server response error.</td></tr>

     *   </tbody>
     * </table>
     * @typedef {number} XMLHttpRequest.NetworkError
     */
    QNetworkReply::NetworkError _errorCode { QNetworkReply::NoError };

    int _timeout { 0 };
    QTimer _timer;
    int _numRedirects { 0 };

private slots:
    void requestFinished();
    void requestError(QNetworkReply::NetworkError code);
    void requestMetaDataChanged();
    void requestDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void requestTimeout();
};

#endif // hifi_XMLHttpRequestClass_h
