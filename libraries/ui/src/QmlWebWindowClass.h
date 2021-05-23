//
//  Created by Bradley Austin Davis on 2015-12-15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ui_QmlWebWindowClass_h
#define hifi_ui_QmlWebWindowClass_h

#include <QtCore/QSharedPointer>

#include "QmlWindowClass.h"

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * A <code>OverlayWebWindow</code> displays an HTML window inside Interface.
 *
 * <p>Create using <code>new OverlayWebWindow(...)</code>.</p>
 *
 * @class OverlayWebWindow
 * @param {string|OverlayWindow.Properties} [titleOrProperties="WebWindow"] - The window's title or initial property values.
 * @param {string} [source="about:blank"] - The URL of the HTML to display. Not used unless the first parameter is the window 
 *     title.
 * @param {number} [width=0] - The width of the window interior, in pixels. Not used unless the first parameter is the window 
 *     title.
 * @param {number} [height=0] - The height of the window interior, in pixels. Not used unless the first parameter is the 
 *     window title.
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {string} url - The URL of the HTML displayed in the window. <em>Read-only.</em>
 * @property {Vec2} position - The position of the window, in pixels.
 * @property {Vec2} size - The size of the window interior, in pixels.
 * @property {boolean} visible - <code>true</code> if the window is visible, <code>false</code> if it isn't.
 *
 * @borrows OverlayWindow.initQml as initQml
 * @borrows OverlayWindow.isVisible as isVisible
 * @borrows OverlayWindow.setVisible as setVisible
 * @borrows OverlayWindow.getPosition as getPosition
 * @borrows OverlayWindow.setPosition as setPosition
 * @borrows OverlayWindow.getSize as getSize
 * @borrows OverlayWindow.setSize as setSize
 * @borrows OverlayWindow.setTitle as setTitle
 * @borrows OverlayWindow.raise as raise
 * @borrows OverlayWindow.close as close
 * @borrows OverlayWindow.getEventBridge as getEventBridge
 * @comment OverlayWindow.sendToQml - Not applicable to OverlayWebWindow; documented separately.
 * @comment OverlayWindow.clearDebugWindow - Not applicable to OverlayWebWindow; documented separately.
 * @comment OverlayWindow.emitScriptEvent  - Documented separately.
 * @borrows OverlayWindow.emitWebEvent as emitWebEvent
 * @borrows OverlayWindow.visibleChanged as visibleChanged
 * @borrows OverlayWindow.positionChanged as positionChanged
 * @borrows OverlayWindow.sizeChanged as sizeChanged
 * @borrows OverlayWindow.moved as moved
 * @borrows OverlayWindow.resized as resized
 * @borrows OverlayWindow.closed as closed
 * @comment OverlayWindow.fromQml - Not applicable to OverlayWebWindow; documented separately.
 * @borrows OverlayWindow.scriptEventReceived as scriptEventReceived
 * @comment OverlayWindow.webEventReceived - Documented separately.
 * @borrows OverlayWindow.hasMoved as hasMoved
 * @borrows OverlayWindow.hasClosed as hasClosed
 * @borrows OverlayWindow.qmlToScript as qmlToScript
 */

/*@jsdoc
 * @function OverlayWebWindow.clearDebugWindow
 * @deprecated This method is deprecated and will be removed.
 */

/*@jsdoc
 * @function OverlayWebWindow.sendToQML
 * @param {string | object} message - Message.
 * @deprecated This method is deprecated and will be removed.
 */

/*@jsdoc
 * @function OverlayWebWindow.fromQML
 * @param {object} message - Message.
 * @returns {Signal}
 * @deprecated This signal is deprecated and will be removed.
 */

/*@jsdoc
 * Sends a message to the HTML page. To receive the message, the HTML page's script must connect to the <code>EventBridge</code>
 * that is automatically provided for the script:
 * <pre class="prettyprint"><code>EventBridge.scriptEventReceived.connect(function(message) {
 *     ...
 * });</code></pre>
 * @function OverlayWebWindow.emitScriptEvent
 * @param {string|object} message - The message to send to the embedded HTML page.
 * @example <caption>Send and receive messages with an HTML window.</caption>
 * // JavaScript file.
 *
 * var overlayWebWindow = new OverlayWebWindow({
 *     title: "Overlay Web Window",
 *     source: Script.resolvePath("OverlayWebWindow.html"),
 *     width: 400,
 *     height: 300
 * });
 * 
 * overlayWebWindow.webEventReceived.connect(function (message) {
 *     print("Message received: " + message);
 * });
 * 
 * Script.setTimeout(function () {
 *     overlayWebWindow.emitScriptEvent("Hello world!");
 * }, 2000);
 *
 * @example
 * // HTML file, "OverlayWebWindow.html".
 *
 * <!DOCTYPE html>
 * <html>
 * <head>
 *     <meta charset="utf-8" />
 * </head>
 * <body>
 *     <p id="hello">...</p>
 * </body>
 * <script>
 *     EventBridge.scriptEventReceived.connect(function (message) {
 *         document.getElementById("hello").innerHTML = message;
 *         EventBridge.emitWebEvent("Hello back!");
 *     });
 * </script>
 * </html>
 */

/*@jsdoc
 * Triggered when a message from the HTML page is received. The HTML page can send a message by calling:
 * <pre class="prettyprint"><code>EventBridge.emitWebEvent(message);</code></pre>
 * @function OverlayWebWindow.webEventReceived
 * @param {string|object} message - The message received.
 * @returns {Signal}
 */


// FIXME refactor this class to be a QQuickItem derived type and eliminate the needless wrapping 
class QmlWebWindowClass : public QmlWindowClass {
    Q_OBJECT
    Q_PROPERTY(QString url READ getURL CONSTANT)

private:
    static ScriptValuePointer internal_constructor(ScriptContext* context, ScriptEngine* engine, bool restricted);
public:
    QmlWebWindowClass(bool restricted) : QmlWindowClass(restricted) {}

    static ScriptValuePointer constructor(ScriptContext* context, ScriptEngine* engine) {
        return internal_constructor(context, engine, false);
    }

    static ScriptValuePointer restricted_constructor(ScriptContext* context, ScriptEngine* engine ){
        return internal_constructor(context, engine, true);
    }

public slots:

    /*@jsdoc
     * Gets the URL of the HTML displayed.
     * @function OverlayWebWindow.getURL
     * @returns {string} - The URL of the HTML page displayed.
     */
    QString getURL();

    /*@jsdoc
     * Loads HTML into the window, replacing current window content.
     * @function OverlayWebWindow.setURL
     * @param {string} url - The URL of the HTML to display.
     */
    void setURL(const QString& url);

    /*@jsdoc
     * Injects a script into the HTML page, replacing any currently injected script.
     * @function OverlayWebWindow.setScriptURL
     * @param {string} url - The URL of the script to inject.
     */
    void setScriptURL(const QString& script);

signals:
    /*@jsdoc
     * Triggered when the window's URL changes.
     * @function OverlayWebWindow.urlChanged
     * @returns {Signal}
     */
    void urlChanged();

protected:
    QString qmlSource() const override { return "QmlWebWindow.qml"; }
};

#endif
