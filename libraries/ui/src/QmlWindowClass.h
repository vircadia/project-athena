//
//  Created by Bradley Austin Davis on 2015-12-15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ui_QmlWindowClass_h
#define hifi_ui_QmlWindowClass_h

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtQuick/QQuickItem>

#include <GLMHelpers.h>

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * A <code>OverlayWindow</code> displays a QML window inside Interface.
 *
 * <p>The QML can optionally include a <code>WebView</code> control that embeds an HTML-based windows. (The <code>WebView</code> 
 * control is defined by a "WebView.qml" file included in the Interface install.) Alternatively, an {@link OverlayWebWindow} 
 * can be used for HTML-based windows.</p>
 *
 * <p>Create using <code>new OverlayWindow(...)</code>.</p>
 *
 * @class OverlayWindow
 * @param {string|OverlayWindow.Properties} [titleOrProperties="WebWindow"] - The window's title or initial property values.
 * @param {string} [source] - The source of the QML to display. Not used unless the first parameter is the window title.
 * @param {number} [width=0] - The width of the window interior, in pixels. Not used unless the first parameter is the window
 *     title.
 * @param {number} [height=0] - The height of the window interior, in pixels. Not used unless the first parameter is the
 *     window title.
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {Vec2} position - The position of the window, in pixels.
 * @property {Vec2} size - The size of the window interior, in pixels.
 * @property {boolean} visible - <code>true</code> if the window is visible, <code>false</code> if it isn't.
 */

// FIXME refactor this class to be a QQuickItem derived type and eliminate the needless wrapping 
class QmlWindowClass : public QObject {
    Q_OBJECT
    Q_PROPERTY(glm::vec2 position READ getPosition WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(glm::vec2 size READ getSize WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)

private:
    static ScriptValuePointer internal_constructor(ScriptContext* context, ScriptEngine* engine, bool restricted);
public:
    static ScriptValuePointer constructor(ScriptContext* context, ScriptEngine* engine) {
        return internal_constructor(context, engine, false);
    }

    static ScriptValuePointer restricted_constructor(ScriptContext* context, ScriptEngine* engine ){
        return internal_constructor(context, engine, true);
    }

    QmlWindowClass(bool restricted);
    ~QmlWindowClass();

    /*@jsdoc
     * @function OverlayWindow.initQml
     * @param {OverlayWindow.Properties} properties - Properties.
     * @deprecated This method is deprecated and will be removed.
     */
    // FIXME: This shouldn't be in the API. It is an internal function used in object construction.
    Q_INVOKABLE virtual void initQml(QVariantMap properties);

    QQuickItem* asQuickItem() const;



public slots:

    /*@jsdoc
     * Gets whether the window is shown or hidden.
     * @function OverlayWindow.isVisible
     * @returns {boolean} code>true</code> if the window is shown, <code>false</code> if it is hidden.
     */
    bool isVisible();

    /*@jsdoc
     * Shows or hides the window.
     * @function OverlayWindow.setVisible
     * @param {boolean} visible - code>true</code> to show the window, <code>false</code> to hide it.
     */
    void setVisible(bool visible);


    /*@jsdoc
     * Gets the position of the window.
     * @function OverlayWindow.getPosition
     * @returns {Vec2} The position of the window, in pixels.
     */
    glm::vec2 getPosition();

    /*@jsdoc
     * Sets the position of the window, from a {@link Vec2}.
     * @function OverlayWindow.setPosition
     * @param {Vec2} position - The position of the window, in pixels.
     */
    void setPosition(const glm::vec2& position);

    /*@jsdoc
     * Sets the position of the window, from a pair of numbers.
     * @function OverlayWindow.setPosition
     * @param {number} x - The x position of the window, in pixels.
     * @param {number} y - The y position of the window, in pixels.
     */
    void setPosition(int x, int y);


    /*@jsdoc
     * Gets the size of the window interior.
     * @function OverlayWindow.getSize
     * @returns {Vec2} The size of the window interior, in pixels.
     */
    glm::vec2 getSize();

    /*@jsdoc
     * Sets the size of the window interior, from a {@link Vec2}.
     * @function OverlayWindow.setSize
     * @param {Vec2} size - The size of the window interior, in pixels.
     */
    void setSize(const glm::vec2& size);

    /*@jsdoc
     * Sets the size of the window interior, from a pair of numbers.
     * @function OverlayWindow.setSize
     * @param {number} width - The width of the window interior, in pixels.
     * @param {number} height - The height of the window interior, in pixels.
     */
    void setSize(int width, int height);

    /*@jsdoc
     * Sets the window title.
     * @function OverlayWindow.setTitle
     * @param {string} title - The window title.
     */
    void setTitle(const QString& title);

    /*@jsdoc
     * Raises the window to the top.
     * @function OverlayWindow.raise
     */
    Q_INVOKABLE void raise();

    /*@jsdoc
     * Closes the window.
     * <p>Note: The window also closes when the script ends.</p>
     * @function OverlayWindow.close
     */
    Q_INVOKABLE void close();

    /*@jsdoc
     * @function OverlayWindow.getEventBridge
     * @returns {object} Object.
     * @deprecated This method is deprecated and will be removed.
     */
    // Shouldn't be in the API: It returns the OverlayWindow object, not the even bridge.
    Q_INVOKABLE QObject* getEventBridge() { return this; };


    /*@jsdoc
     * Sends a message to the QML. To receive the message, the QML must implement a function:
     * <pre class="prettyprint"><code>function fromScript(message) {
     *   ...
     * }</code></pre>
     * @function OverlayWindow.sendToQml
     * @param {string | object} message - The message to send to the QML.
     * @example <caption>Send and receive messages with a QML window.</caption>
     * // JavaScript file.
     * 
     * var overlayWindow = new OverlayWindow({
     *     title: "Overlay Window",
     *     source: Script.resolvePath("OverlayWindow.qml"),
     *     width: 400,
     *     height: 300
     * });
     * 
     * overlayWindow.fromQml.connect(function (message) {
     *     print("Message received: " + message);
     * });
     * 
     * Script.setTimeout(function () {
     *     overlayWindow.sendToQml("Hello world!");
     * }, 2000);
     * 
     * @example
     * // QML file, "OverlayWindow.qml".
     *
     * import QtQuick 2.5
     * import QtQuick.Controls 1.4
     * 
     * Rectangle {
     *     width: parent.width
     *     height: parent.height
     * 
     *     function fromScript(message) {
     *         text.text = message;
     *         sendToScript("Hello back!");
     *     }
     * 
     *     Label{
     *         id: text
     *         anchors.centerIn : parent
     *         text : "..."
     *     }
     * }
     */
    // Scripts can use this to send a message to the QML object
    void sendToQml(const QVariant& message);

    /*@jsdoc
     * Calls a <code>clearWindow()</code> function if present in the QML.
     * @function OverlayWindow.clearDebugWindow
     */
    void clearDebugWindow();

    /*@jsdoc
     * Sends a message to an embedded HTML web page. To receive the message, the HTML page's script must connect to the
     * <code>EventBridge</code> that is automatically provided for the script:
     * <pre class="prettyprint"><code>EventBridge.scriptEventReceived.connect(function(message) {
     *     ...
     * });</code></pre>
     * @function OverlayWindow.emitScriptEvent
     * @param {string|object} message - The message to send to the embedded HTML page.
     */
    // QmlWindow content may include WebView requiring EventBridge.
    void emitScriptEvent(const QVariant& scriptMessage);

    /*@jsdoc
     * @function OverlayWindow.emitWebEvent
     * @param {object|string} message - The message.
     * @deprecated This function is deprecated and will be removed.
     */
    void emitWebEvent(const QVariant& webMessage);

signals:

    /*@jsdoc
     * Triggered when the window is hidden or shown.
     * @function OverlayWindow.visibleChanged
     * @returns {Signal}
     */
    void visibleChanged();

    /*@jsdoc
     * Triggered when the window changes position.
     * @function OverlayWindow.positionChanged
     * @returns {Signal}
     */
    void positionChanged();

    /*@jsdoc
     * Triggered when the window changes size.
     * @function OverlayWindow.sizeChanged
     * @returns {Signal}
     */
    void sizeChanged();

    /*@jsdoc
     * Triggered when the window changes position.
     * @function OverlayWindow.moved
     * @param {Vec2} position - The position of the window, in pixels.
     * @returns {Signal}
     */
    void moved(glm::vec2 position);

    /*@jsdoc
     * Triggered when the window changes size.
     * @function OverlayWindow.resized
     * @param {Size} size - The size of the window interior, in pixels.
     * @returns {Signal}
     */
    void resized(QSizeF size);

    /*@jsdoc
     * Triggered when the window is closed.
     * @function OverlayWindow.closed
     * @returns {Signal}
     */
    void closed();

    /*@jsdoc
     * Triggered when a message from the QML page is received. The QML page can send a message (string or object) by calling:
     * <pre class="prettyprint"><code>sendToScript(message);</code></pre>
     * @function OverlayWindow.fromQml
     * @param {object} message - The message received.
     * @returns {Signal}
     */
    // Scripts can connect to this signal to receive messages from the QML object
    void fromQml(const QVariant& message);


    /*@jsdoc
     * @function OverlayWindow.scriptEventReceived
     * @param {object} message - The message.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    // QmlWindow content may include WebView requiring EventBridge.
    void scriptEventReceived(const QVariant& message);

    /*@jsdoc
     * Triggered when a message from an embedded HTML page is received. The HTML page can send a message by calling:
     * <pre class="prettyprint"><code>EventBridge.emitWebEvent(message);</code></pre>
     * @function OverlayWindow.webEventReceived
     * @param {string|object} message - The message received.
     * @returns {Signal}
     */
    void webEventReceived(const QVariant& message);

protected slots:

    /*@jsdoc
     * @function OverlayWindow.hasMoved
     * @param {Vec2} position - Position.
     * @deprecated This method is deprecated and will be removed.
     */
    // Shouldn't be in the API: it is just connected to in order to emit a signal.
    void hasMoved(QVector2D);

    /*@jsdoc
     * @function OverlayWindow.hasClosed
     * @deprecated This method is deprecated and will be removed.
     */
    // Shouldn't be in the API: it is just connected to in order to emit a signal.
    void hasClosed();

    /*@jsdoc
     * @function OverlayWindow.qmlToScript
     * @param {object} message - Message.
     * @deprecated This method is deprecated and will be removed.
     */
    // Shouldn't be in the API: it is just connected to in order to emit a signal.
    void qmlToScript(const QVariant& message);

protected:
    static QVariantMap parseArguments(ScriptContext* context);
    static ScriptValuePointer internalConstructor(ScriptContext* context, ScriptEngine* engine, 
        std::function<QmlWindowClass*(QVariantMap)> function);

    virtual QString qmlSource() const { return "QmlWindow.qml"; }

    QPointer<QObject> _qmlWindow;
    QString _source;
    const bool _restricted;

private:
    // QmlWindow content may include WebView requiring EventBridge.
    void setKeyboardRaised(QObject* object, bool raised, bool numeric = false);

};

#endif
