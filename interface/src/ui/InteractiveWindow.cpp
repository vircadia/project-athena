//
//  InteractiveWindow.cpp
//  libraries/ui/src
//
//  Created by Thijs Wenker on 2018-06-25
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InteractiveWindow.h"

#include "Application.h"
#include <QtQml/QQmlContext>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QQuickView>

#include <ui/types/ContextAwareProfile.h>
#include <ui/types/HFWebEngineProfile.h>
#include <ui/types/FileTypeProfile.h>
#include <DependencyManager.h>
#include <DockWidget.h>
#include <RegisteredMetaTypes.h>
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>

#include "OffscreenUi.h"
#include "shared/QtHelpers.h"
#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <WinUser.h>
#endif

static auto CONTENT_WINDOW_QML = QUrl("InteractiveWindow.qml");

static const char* const ADDITIONAL_FLAGS_PROPERTY = "additionalFlags";
static const char* const OVERRIDE_FLAGS_PROPERTY = "overrideFlags";
static const char* const SOURCE_PROPERTY = "source";
static const char* const TITLE_PROPERTY = "title";
static const char* const RELATIVE_POSITION_ANCHOR_PROPERTY = "relativePositionAnchor";
static const char* const RELATIVE_POSITION_PROPERTY = "relativePosition";
static const char* const IS_FULL_SCREEN_WINDOW = "isFullScreenWindow";
static const char* const POSITION_PROPERTY = "position";
static const char* const INTERACTIVE_WINDOW_POSITION_PROPERTY = "interactiveWindowPosition";
static const char* const SIZE_PROPERTY = "size";
static const char* const INTERACTIVE_WINDOW_SIZE_PROPERTY = "interactiveWindowSize";
static const char* const VISIBLE_PROPERTY = "visible";
static const char* const INTERACTIVE_WINDOW_VISIBLE_PROPERTY = "interactiveWindowVisible";
static const char* const EVENT_BRIDGE_PROPERTY = "eventBridge";
static const char* const PRESENTATION_MODE_PROPERTY = "presentationMode";
static const char* const DOCKED_PROPERTY = "presentationWindowInfo";
static const char* const DOCK_AREA_PROPERTY = "dockArea";

static const QStringList KNOWN_SCHEMES = QStringList() << "http" << "https" << "file" << "about" << "atp" << "qrc";

static const int DEFAULT_HEIGHT = 60;

QmlWindowProxy::QmlWindowProxy(QObject* qmlObject, QObject* parent) : QmlWrapper(qmlObject, parent) {
    _qmlWindow = qmlObject;
}

void QmlWindowProxy::parentNativeWindowToMainWindow() {
#ifdef Q_OS_WIN
    if (!_qmlWindow) {
        return;
    }
    const auto nativeWindowProperty = _qmlWindow->property("nativeWindow");
    if (nativeWindowProperty.isNull() || !nativeWindowProperty.isValid()) {
        return;
    }
    const auto nativeWindow = qvariant_cast<QQuickWindow*>(nativeWindowProperty);
    SetWindowLongPtr((HWND)nativeWindow->winId(), GWLP_HWNDPARENT, (LONG)MainWindow::findMainWindow()->winId());
#endif
}

void InteractiveWindowProxy::emitScriptEvent(const QVariant& scriptMessage){
    emit scriptEventReceived(scriptMessage);
}

void InteractiveWindowProxy::emitWebEvent(const QVariant& webMessage) {
    emit webEventReceived(webMessage);
}

static void qmlWindowProxyDeleter(QmlWindowProxy* qmlWindowProxy) {
    qmlWindowProxy->deleteLater();
}

static void dockWidgetDeleter(DockWidget* dockWidget) {
    dockWidget->deleteLater();
}

void registerInteractiveWindowMetaType(ScriptEngine* engine) {
    scriptRegisterMetaType(engine, interactiveWindowPointerToScriptValue, interactiveWindowPointerFromScriptValue);
}

ScriptValuePointer interactiveWindowPointerToScriptValue(ScriptEngine* engine, const InteractiveWindowPointer& in) {
    return engine->newQObject(in, ScriptEngine::ScriptOwnership);
}

void interactiveWindowPointerFromScriptValue(const ScriptValuePointer& object, InteractiveWindowPointer& out) {
    if (const auto interactiveWindow = qobject_cast<InteractiveWindowPointer>(object->toQObject())) {
        out = interactiveWindow;
    }
}

void InteractiveWindow::forwardKeyPressEvent(int key, int modifiers) {
    QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, static_cast<Qt::KeyboardModifiers>(modifiers));
    QCoreApplication::postEvent(QCoreApplication::instance(), event);
}

void InteractiveWindow::forwardKeyReleaseEvent(int key, int modifiers) {
    QKeyEvent* event = new QKeyEvent(QEvent::KeyRelease, key, static_cast<Qt::KeyboardModifiers>(modifiers));
    QCoreApplication::postEvent(QCoreApplication::instance(), event);
}

void InteractiveWindow::onMainWindowGeometryChanged(QRect geometry) {
    // This handler is only connected `if (_isFullScreenWindow || _relativePositionAnchor != RelativePositionAnchor::NONE)`.
    if (_isFullScreenWindow) {
        repositionAndResizeFullScreenWindow();
    } else if (_relativePositionAnchor != RelativePositionAnchor::NO_ANCHOR) {
        setPositionUsingRelativePositionAndAnchor(geometry);
    }
}

void InteractiveWindow::emitMainWindowResizeEvent() {
    emit qApp->getWindow()->windowGeometryChanged(qApp->getWindow()->geometry());
}

/*@jsdoc
 * Property values used when creating an <code>InteractiveWindow</code>.
 * @typedef {object} InteractiveWindow.WindowProperties
 * @property {string} [title="InteractiveWindow] - The title of the window.
 * @property {Vec2} [position] - The initial position of the window, in pixels.
 * @property {Vec2} [size] - The initial size of the window, in pixels
 * @property {boolean} [visible=true] - <code>true</code> to make the window visible when created, <code>false</code> to make
 *     it invisible.
 * @property {InteractiveWindow.PresentationMode} [presentationMode=Desktop.PresentationMode.VIRTUAL] -
 *     <code>Desktop.PresentationMode.VIRTUAL</code> to display the window inside Interface, <code>.NATIVE</code> to display it
 *     as its own separate window.
 * @property {InteractiveWindow.PresentationWindowInfo} [presentationWindowInfo] - Controls how a <code>NATIVE</code> window is
 *     displayed. If used, the window is docked to the specified edge of the Interface window, otherwise the window is
 *     displayed as its own separate window.
 * @property {InteractiveWindow.Flags} [additionalFlags=0] - Customizes window behavior.
 * @property {InteractiveWindow.OverrideFlags} [overrideFlags=0] - Customizes window controls.

  * @property {InteractiveWindow.RelativePositionAnchor} [relativePositionAnchor] - The anchor for the 
  *     <code>relativePosition</code>, if used.
  * @property {Vec2} [relativePosition] - The position of the window, relative to the <code>relativePositionAnchor</code>, in 
  *     pixels. Excludes the window frame.
  * @property {boolean} [isFullScreenWindow] - <code>true</code> to make the window full screen.
 */
/*@jsdoc
 * <p>A set of flags customizing <code>InteractiveWindow</code> controls. The value is constructed by using the <code>|</code> 
 * (bitwise OR) operator on the individual flag values.</code>.</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0x00000001</code></td><td>Window</td><td>Displays the window as a window rather than a dialog.</td></tr>
 *     <tr><td><code>0x00001000</code></td><td>WindowTitleHint</td><td>Adds a title bar.</td><td>
 *     <tr><td><code>0x00002000</code></td><td>WindowSystemMenuHint</td><td>Adds a window system menu.</td><td>
 *     <tr><td><code>0x00004000</code></td><td>WindowMinimizeButtonHint</td><td>Adds a minimize button.</td><td>
 *     <tr><td><code>0x00008000</code></td><td>WindowMaximizeButtonHint</td><td>Adds a maximize button.</td><td>
 *     <tr><td><code>0x00040000</code></td><td>WindowStaysOnTopHint</td><td>The window stays on top of other windows.
 *       <em>Not used on Windows.</em>
 *     <tr><td><code>0x08000000</code></td><td>WindowCloseButtonHint</td><td>Adds a close button.</td><td>
 *   <tbody>
 * </table>
 * @typedef {number} InteractiveWindow.OverrideFlags
 */
// OverrideFlags is per InteractiveWindow.qml.
InteractiveWindow::InteractiveWindow(const QString& sourceUrl, const QVariantMap& properties, bool restricted) {
    InteractiveWindowPresentationMode presentationMode = InteractiveWindowPresentationMode::Native;

    if (properties.contains(PRESENTATION_MODE_PROPERTY)) {
        presentationMode = (InteractiveWindowPresentationMode) properties[PRESENTATION_MODE_PROPERTY].toInt();
    }
    
   _interactiveWindowProxy = std::unique_ptr<InteractiveWindowProxy, 
       std::function<void(InteractiveWindowProxy*)>>(new InteractiveWindowProxy, [](InteractiveWindowProxy *p) {
            p->deleteLater();
   });

    connect(_interactiveWindowProxy.get(), &InteractiveWindowProxy::webEventReceived, 
        this, &InteractiveWindow::emitWebEvent, Qt::QueuedConnection);
    connect(this, &InteractiveWindow::scriptEventReceived, _interactiveWindowProxy.get(), 
        &InteractiveWindowProxy::emitScriptEvent, Qt::QueuedConnection);

    if (properties.contains(DOCKED_PROPERTY) && presentationMode == InteractiveWindowPresentationMode::Native) {
        QVariantMap nativeWindowInfo = properties[DOCKED_PROPERTY].toMap();
        Qt::DockWidgetArea dockArea = Qt::TopDockWidgetArea;
        QString title;
        QSize windowSize(DEFAULT_HEIGHT, DEFAULT_HEIGHT);

        if (properties.contains(TITLE_PROPERTY)) {
            title = properties[TITLE_PROPERTY].toString();
        }
        if (properties.contains(SIZE_PROPERTY)) {
            const auto size = vec2FromVariant(properties[SIZE_PROPERTY]);
            windowSize = QSize(size.x, size.y);
        }

        auto mainWindow = qApp->getWindow();
        _dockWidget = std::shared_ptr<DockWidget>(new DockWidget(title, mainWindow), dockWidgetDeleter);
        auto quickView = _dockWidget->getQuickView();

        Application::setupQmlSurface(quickView->rootContext(), true);

        //add any whitelisted callbacks
        OffscreenUi::applyWhiteList(sourceUrl, quickView->rootContext());

        /*@jsdoc
         * Configures how a <code>NATIVE</code> window is displayed.
         * @typedef {object} InteractiveWindow.PresentationWindowInfo
         * @property {InteractiveWindow.DockArea} dockArea - The edge of the Interface window to dock to.
         */
        if (nativeWindowInfo.contains(DOCK_AREA_PROPERTY)) {
            DockArea dockedArea = (DockArea) nativeWindowInfo[DOCK_AREA_PROPERTY].toInt();
            int tempWidth = 0;
            int tempHeight = 0;
            switch (dockedArea) {
                case DockArea::TOP:
                    dockArea = Qt::TopDockWidgetArea;
                    tempHeight = windowSize.height();
                    _dockWidget->setFixedHeight(tempHeight);
                    qApp->getWindow()->setDockedWidgetRelativePositionOffset(QSize(0, -tempHeight));
                    break;
                case DockArea::BOTTOM:
                    dockArea = Qt::BottomDockWidgetArea;
                    tempHeight = windowSize.height();
                    _dockWidget->setFixedHeight(tempHeight);
                    qApp->getWindow()->setDockedWidgetRelativePositionOffset(QSize(0, tempHeight));
                    break;
                case DockArea::LEFT:
                    dockArea = Qt::LeftDockWidgetArea;
                    tempWidth = windowSize.width();
                    _dockWidget->setFixedWidth(tempWidth);
                    qApp->getWindow()->setDockedWidgetRelativePositionOffset(QSize(-tempWidth, 0));
                    break;
                case DockArea::RIGHT:
                    dockArea = Qt::RightDockWidgetArea;
                    tempWidth = windowSize.width();
                    _dockWidget->setFixedWidth(tempWidth);
                    qApp->getWindow()->setDockedWidgetRelativePositionOffset(QSize(tempWidth, 0));
                    break;

                default:
                    _dockWidget->setFixedHeight(DEFAULT_HEIGHT);
                    break;
            }
        }
      
        
        QObject::connect(quickView.get(), &QQuickView::statusChanged, [&, this] (QQuickView::Status status) {
            if (status == QQuickView::Ready) {
                QQuickItem* rootItem = _dockWidget->getRootItem();
                _dockWidget->getQuickView()->rootContext()->setContextProperty(EVENT_BRIDGE_PROPERTY, _interactiveWindowProxy.get());
                // The qmlToScript method handles the thread-safety of this call. Because the QVariant argument
                // passed to the sendToScript signal may wrap an externally managed and thread-unsafe QJSValue,
                // qmlToScript needs to be called directly, so the QJSValue can be immediately converted to a plain QVariant.
                QObject::connect(rootItem, SIGNAL(sendToScript(QVariant)), this, SLOT(qmlToScript(const QVariant&)),
                                 Qt::DirectConnection);
                QObject::connect(rootItem, SIGNAL(keyPressEvent(int, int)), this, SLOT(forwardKeyPressEvent(int, int)),
                                 Qt::QueuedConnection);
                QObject::connect(rootItem, SIGNAL(keyReleaseEvent(int, int)), this, SLOT(forwardKeyReleaseEvent(int, int)),
                                 Qt::QueuedConnection);
            }
        });

        QObject::connect(_dockWidget.get(), SIGNAL(onResizeEvent()), this, SLOT(emitMainWindowResizeEvent()));

        _dockWidget->setSource(QUrl(sourceUrl));
        _dockWidget->setObjectName("DockedWidget");
        mainWindow->addDockWidget(dockArea, _dockWidget.get());
    } else {
        auto contextInitLambda = [&](QQmlContext* context) {
            // If the restricted flag is on, the web content will not be able to access local files
            ContextAwareProfile::restrictContext(context, restricted);
#if !defined(Q_OS_ANDROID)
            FileTypeProfile::registerWithContext(context);
            HFWebEngineProfile::registerWithContext(context);
#endif
        };

        auto objectInitLambda = [&](QQmlContext* context, QObject* object) {
            _qmlWindowProxy = std::shared_ptr<QmlWindowProxy>(new QmlWindowProxy(object, nullptr), qmlWindowProxyDeleter);
            context->setContextProperty(EVENT_BRIDGE_PROPERTY, _interactiveWindowProxy.get());
            if (properties.contains(ADDITIONAL_FLAGS_PROPERTY)) {
                object->setProperty(ADDITIONAL_FLAGS_PROPERTY, properties[ADDITIONAL_FLAGS_PROPERTY].toUInt());
            }
            if (properties.contains(OVERRIDE_FLAGS_PROPERTY)) {
                object->setProperty(OVERRIDE_FLAGS_PROPERTY, properties[OVERRIDE_FLAGS_PROPERTY].toUInt());
            }
            if (properties.contains(PRESENTATION_MODE_PROPERTY)) {
                object->setProperty(PRESENTATION_MODE_PROPERTY, properties[PRESENTATION_MODE_PROPERTY].toInt());
            }
            if (properties.contains(TITLE_PROPERTY)) {
                object->setProperty(TITLE_PROPERTY, properties[TITLE_PROPERTY].toString());
            }
            if (properties.contains(VISIBLE_PROPERTY)) {
                object->setProperty(VISIBLE_PROPERTY, properties[INTERACTIVE_WINDOW_VISIBLE_PROPERTY].toBool());
            }
            if (properties.contains(SIZE_PROPERTY)) {
                const auto size = vec2FromVariant(properties[SIZE_PROPERTY]);
                object->setProperty(INTERACTIVE_WINDOW_SIZE_PROPERTY, QSize(size.x, size.y));
            }
            if (properties.contains(POSITION_PROPERTY)) {
                const auto position = vec2FromVariant(properties[POSITION_PROPERTY]);
                object->setProperty(INTERACTIVE_WINDOW_POSITION_PROPERTY, QPointF(position.x, position.y));
            }
            if (properties.contains(RELATIVE_POSITION_ANCHOR_PROPERTY)) {
                _relativePositionAnchor = static_cast<RelativePositionAnchor>(properties[RELATIVE_POSITION_ANCHOR_PROPERTY].toInt());
            }
            if (properties.contains(RELATIVE_POSITION_PROPERTY)) {
                _relativePosition = vec2FromVariant(properties[RELATIVE_POSITION_PROPERTY]);
                setPositionUsingRelativePositionAndAnchor(qApp->getWindow()->geometry());
            }
            if (properties.contains(IS_FULL_SCREEN_WINDOW)) {
                _isFullScreenWindow = properties[IS_FULL_SCREEN_WINDOW].toBool();
            }

            if (_isFullScreenWindow) {
                QRect geo = qApp->getWindow()->geometry();
                object->setProperty(INTERACTIVE_WINDOW_POSITION_PROPERTY, QPointF(geo.x(), geo.y()));
                object->setProperty(INTERACTIVE_WINDOW_SIZE_PROPERTY, QSize(geo.width(), geo.height()));
            }

            // The qmlToScript method handles the thread-safety of this call. Because the QVariant argument
            // passed to the sendToScript signal may wrap an externally managed and thread-unsafe QJSValue,
            // qmlToScript needs to be called directly, so the QJSValue can be immediately converted to a plain QVariant.
            connect(object, SIGNAL(sendToScript(QVariant)), this, SLOT(qmlToScript(const QVariant&)), Qt::DirectConnection);
            QObject::connect(object, SIGNAL(keyPressEvent(int, int)), this, SLOT(forwardKeyPressEvent(int, int)),
                             Qt::QueuedConnection);
            QObject::connect(object, SIGNAL(keyReleaseEvent(int, int)), this, SLOT(forwardKeyReleaseEvent(int, int)),
                             Qt::QueuedConnection);
            connect(object, SIGNAL(interactiveWindowPositionChanged()), this, SIGNAL(positionChanged()), Qt::QueuedConnection);
            connect(object, SIGNAL(interactiveWindowSizeChanged()), this, SIGNAL(sizeChanged()), Qt::QueuedConnection);
            connect(object, SIGNAL(interactiveWindowVisibleChanged()), this, SIGNAL(visibleChanged()), Qt::QueuedConnection);
            connect(object, SIGNAL(presentationModeChanged()), this, SIGNAL(presentationModeChanged()), Qt::QueuedConnection);
            connect(object, SIGNAL(titleChanged()), this, SIGNAL(titleChanged()), Qt::QueuedConnection);
            connect(object, SIGNAL(windowClosed()), this, SIGNAL(closed()), Qt::QueuedConnection);
            connect(object, SIGNAL(selfDestruct()), this, SLOT(close()), Qt::QueuedConnection);

#ifdef Q_OS_WIN
            connect(object, SIGNAL(nativeWindowChanged()), this, SLOT(parentNativeWindowToMainWindow()), Qt::QueuedConnection);
            connect(object, SIGNAL(interactiveWindowVisibleChanged()), this, SLOT(parentNativeWindowToMainWindow()), Qt::QueuedConnection);
            connect(object, SIGNAL(presentationModeChanged()), this, SLOT(parentNativeWindowToMainWindow()), Qt::QueuedConnection);
#endif
            
            if (_isFullScreenWindow || _relativePositionAnchor != RelativePositionAnchor::NO_ANCHOR) {
                connect(qApp->getWindow(), &MainWindow::windowGeometryChanged, this, &InteractiveWindow::onMainWindowGeometryChanged, Qt::QueuedConnection);
            }

            QUrl sourceURL{ sourceUrl };
            // If the passed URL doesn't correspond to a known scheme, assume it's a local file path
            if (!KNOWN_SCHEMES.contains(sourceURL.scheme(), Qt::CaseInsensitive)) {
                sourceURL = QUrl::fromLocalFile(sourceURL.toString()).toString();
            }
            object->setObjectName("InteractiveWindow");
            object->setProperty(SOURCE_PROPERTY, sourceURL);
        };

        if (auto offscreenUI = DependencyManager::get<OffscreenUi>()) {
            // Build the event bridge and wrapper on the main thread
            offscreenUI->loadInNewContext(CONTENT_WINDOW_QML, objectInitLambda, contextInitLambda);
        }
    }
}

InteractiveWindow::~InteractiveWindow() {
    close();
}

void InteractiveWindow::sendToQml(const QVariant& message) {

    // Forward messages received from the script on to QML
    if (_dockWidget) {
        QQuickItem* rootItem = _dockWidget->getRootItem();
        if (rootItem) {
            QMetaObject::invokeMethod(rootItem, "fromScript", Qt::QueuedConnection, Q_ARG(QVariant, message));
        }
    } else {
        if (_qmlWindowProxy) {
            QMetaObject::invokeMethod(_qmlWindowProxy->getQmlWindow(), "fromScript", Qt::QueuedConnection, Q_ARG(QVariant, message));
        }
    }
}

void InteractiveWindow::emitScriptEvent(const QVariant& scriptMessage) {
    emit scriptEventReceived(scriptMessage);
}

void InteractiveWindow::emitWebEvent(const QVariant& webMessage) {
    emit webEventReceived(webMessage);
}

void InteractiveWindow::close() {
    if (_qmlWindowProxy) {
        QObject* qmlWindow = _qmlWindowProxy->getQmlWindow();
        if (qmlWindow) {
            qmlWindow->deleteLater();
        }
        _qmlWindowProxy->deleteLater();
    }

    if (_dockWidget) {
        auto window = qApp->getWindow();
        if (QThread::currentThread() != window->thread()) {
            BLOCKING_INVOKE_METHOD(window, "removeDockWidget", Q_ARG(QDockWidget*, _dockWidget.get()));
        } else {
            window->removeDockWidget(_dockWidget.get());
        }
    }
    _dockWidget = nullptr;
    _qmlWindowProxy = nullptr;
    _interactiveWindowProxy = nullptr;
}

void InteractiveWindow::show() {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy->getQmlWindow(), "show");
    }
}

void InteractiveWindow::raise() {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy->getQmlWindow(), "raiseWindow");
    }
}

void InteractiveWindow::qmlToScript(const QVariant& originalMessage) {
    QVariant message = originalMessage;
    if (message.canConvert<QJSValue>()) {
        message = qvariant_cast<QJSValue>(message).toVariant();
    } else if (message.canConvert<QString>()) {
        message = message.toString();
    } else {
        qWarning() << "Unsupported message type " << message;
        return;
    }

    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "fromQml", Q_ARG(const QVariant&, message));
    } else {
        emit fromQml(message);
    }
}

void InteractiveWindow::setVisible(bool visible) {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, INTERACTIVE_WINDOW_VISIBLE_PROPERTY),
                                  Q_ARG(QVariant, visible));
    }
}

bool InteractiveWindow::isVisible() const {
    if (!_qmlWindowProxy) {
        return false;
    }

    return _qmlWindowProxy->readProperty(INTERACTIVE_WINDOW_VISIBLE_PROPERTY).toBool();
}

glm::vec2 InteractiveWindow::getPosition() const {
    if (!_qmlWindowProxy) {
        return {};
    }

    return toGlm(_qmlWindowProxy->readProperty(INTERACTIVE_WINDOW_POSITION_PROPERTY).toPointF());
}

void InteractiveWindow::setPosition(const glm::vec2& position) {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, INTERACTIVE_WINDOW_POSITION_PROPERTY),
                                  Q_ARG(QVariant, QPointF(position.x, position.y)));
        QMetaObject::invokeMethod(_qmlWindowProxy->getQmlWindow(), "updateInteractiveWindowPositionForMode");
    }
}

RelativePositionAnchor InteractiveWindow::getRelativePositionAnchor() const {
    return _relativePositionAnchor;
}

void InteractiveWindow::setRelativePositionAnchor(const RelativePositionAnchor& relativePositionAnchor) {
    _relativePositionAnchor = relativePositionAnchor;
    setPositionUsingRelativePositionAndAnchor(qApp->getWindow()->geometry());
}

glm::vec2 InteractiveWindow::getRelativePosition() const {
    return _relativePosition;
}

void InteractiveWindow::setRelativePosition(const glm::vec2& relativePosition) {    
    _relativePosition = relativePosition;
    setPositionUsingRelativePositionAndAnchor(qApp->getWindow()->geometry());
}

void InteractiveWindow::setPositionUsingRelativePositionAndAnchor(const QRect& mainWindowGeometry) {
    RelativePositionAnchor relativePositionAnchor = getRelativePositionAnchor();
    glm::vec2 relativePosition = getRelativePosition();

    glm::vec2 newPosition;

    switch (relativePositionAnchor) {
        case RelativePositionAnchor::TOP_LEFT:
            newPosition.x = mainWindowGeometry.x() + relativePosition.x;
            newPosition.y = mainWindowGeometry.y() + relativePosition.y;
            break;
        case RelativePositionAnchor::TOP_RIGHT:
            newPosition.x = mainWindowGeometry.x() + mainWindowGeometry.width() - relativePosition.x;
            newPosition.y = mainWindowGeometry.y() + relativePosition.y;
            break;
        case RelativePositionAnchor::BOTTOM_RIGHT:
            newPosition.x = mainWindowGeometry.x() + mainWindowGeometry.width() - relativePosition.x;
            newPosition.y = mainWindowGeometry.y() + mainWindowGeometry.height() - relativePosition.y;
            break;
        case RelativePositionAnchor::BOTTOM_LEFT:
            newPosition.x = mainWindowGeometry.x() + relativePosition.x;
            newPosition.y = mainWindowGeometry.y() + mainWindowGeometry.height() - relativePosition.y;
            break;
        case RelativePositionAnchor::NO_ANCHOR:
            // No-op.
            break;
    }

    // Make sure we include the dimensions of the docked widget!
    QSize dockedWidgetRelativePositionOffset = qApp->getWindow()->getDockedWidgetRelativePositionOffset();
    newPosition.x = newPosition.x + dockedWidgetRelativePositionOffset.width();
    newPosition.y = newPosition.y + dockedWidgetRelativePositionOffset.height();

    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, INTERACTIVE_WINDOW_POSITION_PROPERTY),
            Q_ARG(QVariant, QPointF(newPosition.x, newPosition.y)));
    }
    setPosition(newPosition);
}

void InteractiveWindow::repositionAndResizeFullScreenWindow() {
    QRect windowGeometry = qApp->getWindow()->geometry();

    setPosition(glm::vec2(windowGeometry.x(), windowGeometry.y()));
    setSize(glm::vec2(windowGeometry.width(), windowGeometry.height()));
}

glm::vec2 InteractiveWindow::getSize() const {
    if (!_qmlWindowProxy) {
        return {};
    }

    return toGlm(_qmlWindowProxy->readProperty(INTERACTIVE_WINDOW_SIZE_PROPERTY).toSize());
}

void InteractiveWindow::setSize(const glm::vec2& size) {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, INTERACTIVE_WINDOW_SIZE_PROPERTY),
                                  Q_ARG(QVariant, QSize(size.x, size.y)));
        QMetaObject::invokeMethod(_qmlWindowProxy->getQmlWindow(), "updateInteractiveWindowSizeForMode");
    }
}

QString InteractiveWindow::getTitle() const {
    if (!_qmlWindowProxy) {
        return QString();
    }

    return _qmlWindowProxy->readProperty(TITLE_PROPERTY).toString();
}

void InteractiveWindow::setTitle(const QString& title) {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, TITLE_PROPERTY),
                                  Q_ARG(QVariant, title));
    }
}

int InteractiveWindow::getPresentationMode() const {
    if (!_qmlWindowProxy) {
        return Virtual;
    }

    return _qmlWindowProxy->readProperty(PRESENTATION_MODE_PROPERTY).toInt();
}

void InteractiveWindow::parentNativeWindowToMainWindow() {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "parentNativeWindowToMainWindow");
    }
}

void InteractiveWindow::setPresentationMode(int presentationMode) {
    if (_qmlWindowProxy) {
        QMetaObject::invokeMethod(_qmlWindowProxy.get(), "writeProperty", Q_ARG(QString, PRESENTATION_MODE_PROPERTY),
                                  Q_ARG(QVariant, presentationMode));
    }
}
