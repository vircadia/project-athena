//
//  Created by Bradley Austin Davis on 2015-12-15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "QmlWindowClass.h"

#include <mutex>

#include <QtCore/QThread>

#include <QtQuick/QQuickItem>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <shared/QtHelpers.h>
#include "OffscreenUi.h"
#include "ui/types/HFWebEngineProfile.h"
#include "ui/types/FileTypeProfile.h"
#include <ScriptContext.h>
#include <ScriptEngine.h>
#include <ScriptManager.h>
#include <ScriptValue.h>

static const char* const SOURCE_PROPERTY = "source";
static const char* const TITLE_PROPERTY = "title";
static const char* const EVENT_BRIDGE_PROPERTY = "eventBridge";
static const char* const WIDTH_PROPERTY = "width";
static const char* const HEIGHT_PROPERTY = "height";
static const char* const VISIBILE_PROPERTY = "visible";
static const uvec2 MAX_QML_WINDOW_SIZE { 1280, 720 };
static const uvec2 MIN_QML_WINDOW_SIZE { 120, 80 };

QVariantMap QmlWindowClass::parseArguments(ScriptContext* context) {
    const auto argumentCount = context->argumentCount();
    QVariantMap properties;
    if (argumentCount > 1) {
        if (!context->argument(0)->isUndefined()) {
            properties[TITLE_PROPERTY] = context->argument(0)->toString();
        }
        if (!context->argument(1)->isUndefined()) {
            properties[SOURCE_PROPERTY] = context->argument(1)->toString();
        }
        if (context->argument(2)->isNumber()) {
            properties[WIDTH_PROPERTY] = context->argument(2)->toInt32();
        }
        if (context->argument(3)->isNumber()) {
            properties[HEIGHT_PROPERTY] = context->argument(3)->toInt32();
        }
    } else {
        properties = context->argument(0)->toVariant().toMap();
    }

    QUrl url { properties[SOURCE_PROPERTY].toString() };
    // If the passed URL doesn't correspond to a known scheme, assume it's a local file path
    if (url.scheme() != "http" && url.scheme() != "https" && url.scheme() != "file" && url.scheme() != "about" &&
            url.scheme() != "atp" && url.scheme() != "qrc") {
        properties[SOURCE_PROPERTY] = QUrl::fromLocalFile(url.toString()).toString();
    }

    return properties;
}



// Method called by Qt scripts to create a new web window in the overlay
ScriptValuePointer QmlWindowClass::internal_constructor(ScriptContext* context, ScriptEngine* engine, bool restricted) {
    auto properties = parseArguments(context);
    QmlWindowClass* retVal = new QmlWindowClass(restricted);
    Q_ASSERT(retVal);
    if (QThread::currentThread() != qApp->thread()) {
        retVal->moveToThread(qApp->thread());
        BLOCKING_INVOKE_METHOD(retVal, "initQml", Q_ARG(QVariantMap, properties));
    } else {
        retVal->initQml(properties);
    }
    auto manager = engine->manager();
    connect(manager, &ScriptManager::destroyed, retVal, &QmlWindowClass::deleteLater);
    return engine->newQObject(retVal);
}

QmlWindowClass::QmlWindowClass(bool restricted) : _restricted(restricted) {

}

/*@jsdoc
 * Properties used to initialize an {@link OverlayWindow} or {@link OverlayWebWindow}.
 * @typedef {object} OverlayWindow.Properties
 * @property {string} [title="WebWindow] - The window title.
 * @property {string} [source] - The source of the QML or HTML to display.
 * @property {number} [width=0] - The width of the window interior, in pixels.
 * @property {number} [height=0] - The height of the window interior, in pixels.
 * @property {boolean} [visible=true] - <code>true</codE> if the window should be visible, <code>false</code> if it shouldn't.
 */
void QmlWindowClass::initQml(QVariantMap properties) {
#ifndef DISABLE_QML
    auto offscreenUi = DependencyManager::get<OffscreenUi>();
    _source = properties[SOURCE_PROPERTY].toString();

    auto objectInitLambda = [&](QQmlContext* context, QObject* object) {
        _qmlWindow = object;
        context->setContextProperty(EVENT_BRIDGE_PROPERTY, this);
        context->engine()->setObjectOwnership(this, QQmlEngine::CppOwnership);
        context->engine()->setObjectOwnership(object, QQmlEngine::CppOwnership);
        if (properties.contains(TITLE_PROPERTY)) {
            object->setProperty(TITLE_PROPERTY, properties[TITLE_PROPERTY].toString());
        }
        if (properties.contains(HEIGHT_PROPERTY) && properties.contains(WIDTH_PROPERTY)) {
            uvec2 requestedSize { properties[WIDTH_PROPERTY].toUInt(), properties[HEIGHT_PROPERTY].toUInt() };
            requestedSize = glm::clamp(requestedSize, MIN_QML_WINDOW_SIZE, MAX_QML_WINDOW_SIZE);
            asQuickItem()->setSize(QSize(requestedSize.x, requestedSize.y));
        }

        bool visible = !properties.contains(VISIBILE_PROPERTY) || properties[VISIBILE_PROPERTY].toBool();
        object->setProperty(OFFSCREEN_VISIBILITY_PROPERTY, visible);
        object->setProperty(SOURCE_PROPERTY, _source);

        const QMetaObject *metaObject = _qmlWindow->metaObject();
        // Forward messages received from QML on to the script
        connect(_qmlWindow, SIGNAL(sendToScript(QVariant)), this, SLOT(qmlToScript(const QVariant&)), Qt::QueuedConnection);
        connect(_qmlWindow, SIGNAL(visibleChanged()), this, SIGNAL(visibleChanged()), Qt::QueuedConnection);

        if (metaObject->indexOfSignal("resized") >= 0)
            connect(_qmlWindow, SIGNAL(resized(QSizeF)), this, SIGNAL(resized(QSizeF)), Qt::QueuedConnection);
        if (metaObject->indexOfSignal("moved") >= 0)
            connect(_qmlWindow, SIGNAL(moved(QVector2D)), this, SLOT(hasMoved(QVector2D)), Qt::QueuedConnection);
        connect(_qmlWindow, SIGNAL(windowClosed()), this, SLOT(hasClosed()), Qt::QueuedConnection);
    };

    auto contextInitLambda = [&](QQmlContext* context) {
#if !defined(Q_OS_ANDROID)
        // If the restricted flag is on, override the FileTypeProfile and HFWebEngineProfile objects in the 
        // QML surface root context with local ones
        ContextAwareProfile::restrictContext(context, _restricted);
        FileTypeProfile::registerWithContext(context);
        HFWebEngineProfile::registerWithContext(context);
#endif
    };

    // Build the event bridge and wrapper on the main thread
    offscreenUi->loadInNewContext(qmlSource(), objectInitLambda, contextInitLambda);

    Q_ASSERT(_qmlWindow);
    Q_ASSERT(dynamic_cast<const QQuickItem*>(_qmlWindow.data()));
#endif
}

void QmlWindowClass::qmlToScript(const QVariant& message) {
    if (message.canConvert<QJSValue>()) {
        emit fromQml(qvariant_cast<QJSValue>(message).toVariant());
    } else if (message.canConvert<QString>()) {
        emit fromQml(message.toString());
    } else {
        qWarning() << "Unsupported message type " << message;
    }
}

void QmlWindowClass::sendToQml(const QVariant& message) {
    // Forward messages received from the script on to QML
    QMetaObject::invokeMethod(asQuickItem(), "fromScript", Qt::QueuedConnection, Q_ARG(QVariant, message));
}

void QmlWindowClass::clearDebugWindow() {
    QMetaObject::invokeMethod(asQuickItem(), "clearDebugWindow", Qt::QueuedConnection);
}

void QmlWindowClass::emitScriptEvent(const QVariant& scriptMessage) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "emitScriptEvent", Qt::QueuedConnection, Q_ARG(QVariant, scriptMessage));
    } else {
        emit scriptEventReceived(scriptMessage);
    }
}

void QmlWindowClass::emitWebEvent(const QVariant& webMessage) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "emitWebEvent", Qt::QueuedConnection, Q_ARG(QVariant, webMessage));
    } else {
        // Special case to handle raising and lowering the virtual keyboard.
        const QString RAISE_KEYBOARD = "_RAISE_KEYBOARD";
        const QString RAISE_KEYBOARD_NUMERIC = "_RAISE_KEYBOARD_NUMERIC";
        const QString LOWER_KEYBOARD = "_LOWER_KEYBOARD";
        QString messageString = webMessage.type() == QVariant::String ? webMessage.toString() : "";
        if (messageString.left(RAISE_KEYBOARD.length()) == RAISE_KEYBOARD) {
            setKeyboardRaised(asQuickItem(), true, messageString == RAISE_KEYBOARD_NUMERIC);
        } else if (messageString == LOWER_KEYBOARD) {
            setKeyboardRaised(asQuickItem(), false);
        } else {
            emit webEventReceived(webMessage);
        }
    }
}

void QmlWindowClass::setKeyboardRaised(QObject* object, bool raised, bool numeric) {
    if (!object) {
        return;
    }

    QQuickItem* item = dynamic_cast<QQuickItem*>(object);
    while (item) {
        if (item->property("keyboardRaised").isValid()) {
            if (item->property("punctuationMode").isValid()) {
                item->setProperty("punctuationMode", QVariant(numeric));
            }
            item->setProperty("keyboardRaised", QVariant(raised));
            return;
        }
        item = dynamic_cast<QQuickItem*>(item->parentItem());
    }
}

QmlWindowClass::~QmlWindowClass() {
    close();
}

QQuickItem* QmlWindowClass::asQuickItem() const {
    return _qmlWindow.isNull() ? nullptr : dynamic_cast<QQuickItem*>(_qmlWindow.data());
}

void QmlWindowClass::setVisible(bool visible) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setVisible", Q_ARG(bool, visible));
        return;
    }

    QQuickItem* targetWindow = asQuickItem();
    targetWindow->setProperty(OFFSCREEN_VISIBILITY_PROPERTY, visible);
}

bool QmlWindowClass::isVisible() {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "isVisible", Q_RETURN_ARG(bool, result));
        return result;
    }

    // The tool window itself has special logic based on whether any tabs are enabled
    if (_qmlWindow.isNull()) {
        return false;
    }

    return asQuickItem()->isVisible();
}

glm::vec2 QmlWindowClass::getPosition() {
    if (QThread::currentThread() != thread()) {
        vec2 result;
        BLOCKING_INVOKE_METHOD(this, "getPosition", Q_RETURN_ARG(glm::vec2, result));
        return result;
    }

    if (_qmlWindow.isNull()) {
        return {};
    }

    return toGlm(asQuickItem()->position());
}


void QmlWindowClass::setPosition(const glm::vec2& position) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setPosition", Q_ARG(const glm::vec2&, position));
        return;
    }

    if (!_qmlWindow.isNull()) {
        asQuickItem()->setPosition(QPointF(position.x, position.y));
    }
}

void QmlWindowClass::setPosition(int x, int y) {
    setPosition(glm::vec2(x, y));
}

// FIXME move to GLM helpers
glm::vec2 toGlm(const QSizeF& size) {
    return glm::vec2(size.width(), size.height());
}

glm::vec2 QmlWindowClass::getSize() {
    if (QThread::currentThread() != thread()) {
        vec2 result;
        BLOCKING_INVOKE_METHOD(this, "getSize", Q_RETURN_ARG(glm::vec2, result));
        return result;
    }

    if (_qmlWindow.isNull()) {
        return {};
    }
    QQuickItem* targetWindow = asQuickItem();
    return vec2(targetWindow->width(), targetWindow->height());
}

void QmlWindowClass::setSize(const glm::vec2& size) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setSize", Q_ARG(const glm::vec2&, size));
        return;
    }

    if (!_qmlWindow.isNull()) {
        asQuickItem()->setSize(QSizeF(size.x, size.y));
    }
}

void QmlWindowClass::setSize(int width, int height) {
    setSize(glm::vec2(width, height));
}

void QmlWindowClass::setTitle(const QString& title) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setTitle", Q_ARG(QString, title));
        return;
    }

    if (!_qmlWindow.isNull()) {
        asQuickItem()->setProperty(TITLE_PROPERTY, title);
    }
}

void QmlWindowClass::close() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "close");
        return;
    }

    if (_qmlWindow) {
        _qmlWindow->deleteLater();
    }
    _qmlWindow = nullptr;
}

void QmlWindowClass::hasMoved(QVector2D position) {
    emit moved(glm::vec2(position.x(), position.y()));
}

void QmlWindowClass::hasClosed() {
    emit closed();
}

void QmlWindowClass::raise() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "raise");
        return;
    }

    if (_qmlWindow) {
        QMetaObject::invokeMethod(asQuickItem(), "raise", Qt::DirectConnection);
    }
}
