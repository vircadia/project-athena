//
//  Created by Bradley Austin Davis on 2015-05-13
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "OffscreenQmlSurface.h"

#include <unordered_set>
#include <unordered_map>

#include <gl/Config.h>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions_4_1_Core>
#include <QtWidgets/QWidget>
#include <QtQml/QtQml>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickRenderControl>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtMultimedia/QMediaService>
#include <QtMultimedia/QAudioOutputSelectorControl>
#include <QtMultimedia/QMediaPlayer>
#include <QtGui/QInputMethodEvent>
#include <shared/NsightHelpers.h>
#include <shared/GlobalAppProperties.h>
#include <shared/QtHelpers.h>
#include <PerfStat.h>
#include <DependencyManager.h>
#include <NumericalConstants.h>
#include <Finally.h>
#include <PathUtils.h>
#include <AbstractUriHandler.h>
#include <AccountManager.h>
#include <NetworkAccessManager.h>
#include <GLMHelpers.h>
#include <AudioClient.h>
#include <shared/LocalFileAccessGate.h>

#include <gl/OffscreenGLCanvas.h>
#include <gl/GLHelpers.h>
#include <gl/Context.h>
#include <shared/ReadWriteLockable.h>

#include "shared/FileUtils.h"
#include "types/FileTypeProfile.h"
#include "types/HFWebEngineProfile.h"
#include "types/SoundEffect.h"

#include "TabletScriptingInterface.h"
#include "ToolbarScriptingInterface.h"
#include "Logging.h"

namespace hifi { namespace qml { namespace offscreen {

class OffscreenQmlWhitelist : public Dependency, private ReadWriteLockable {
    SINGLETON_DEPENDENCY

public:
    void addWhitelistContextHandler(const std::initializer_list<QUrl>& urls, const QmlContextCallback& callback) {
        withWriteLock([&] {
            for (auto url : urls) {
                if (url.isRelative()) {
                    url = PathUtils::qmlUrl(url.toString());
                }
                _callbacks[url].push_back(callback);
            }
        });
    }

    QList<QmlContextCallback> getCallbacksForUrl(const QUrl& url) const {
        return resultWithReadLock<QList<QmlContextCallback>>([&] {
            QList<QmlContextCallback> result;
            auto itr = _callbacks.find(url);
            if (_callbacks.end() != itr) {
                result = *itr;
            }
            return result;
        });
    }

private:
    QHash<QUrl, QList<QmlContextCallback>> _callbacks;
};

QSharedPointer<OffscreenQmlWhitelist> getQmlWhitelist() {
    static std::once_flag once;
    std::call_once(once, [&] { DependencyManager::set<OffscreenQmlWhitelist>(); });

    return DependencyManager::get<OffscreenQmlWhitelist>();
}

// Class to handle changing QML audio output device using another thread
class AudioHandler : public QObject, QRunnable {
    Q_OBJECT
public:
    AudioHandler(OffscreenQmlSurface* surface, const QString& deviceName, QObject* parent = nullptr);

    virtual ~AudioHandler() { }

    void run() override;

private:
    QString _newTargetDevice;
    QSharedPointer<OffscreenQmlSurface> _surface;
    std::vector<QMediaPlayer*> _players;
};

class UrlHandler : public QObject {
    Q_OBJECT
public:
    UrlHandler(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE bool canHandleUrl(const QString& url) {
        static auto handler = dynamic_cast<AbstractUriHandler*>(qApp);
        return handler && handler->canAcceptURL(url);
    }

    Q_INVOKABLE bool handleUrl(const QString& url) {
        static auto handler = dynamic_cast<AbstractUriHandler*>(qApp);
        return handler && handler->acceptURL(url);
    }
};

class QmlNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory {
public:
    class QmlNetworkAccessManager : public NetworkAccessManager {
    public:
        QmlNetworkAccessManager(QObject* parent)
            : NetworkAccessManager(parent){};
    };
    QNetworkAccessManager* create(QObject* parent) override { return new QmlNetworkAccessManager(parent); }
};

QString getEventBridgeJavascript() {
    // FIXME: Refactor with similar code in RenderableWebEntityItem
    QString javaScriptToInject;
    QFile webChannelFile(":qtwebchannel/qwebchannel.js");
    QFile createGlobalEventBridgeFile(PathUtils::resourcesPath() + "/html/createGlobalEventBridge.js");
    if (webChannelFile.open(QFile::ReadOnly | QFile::Text) && createGlobalEventBridgeFile.open(QFile::ReadOnly | QFile::Text)) {
        QString webChannelStr = QTextStream(&webChannelFile).readAll();
        QString createGlobalEventBridgeStr = QTextStream(&createGlobalEventBridgeFile).readAll();
        javaScriptToInject = webChannelStr + createGlobalEventBridgeStr;
    } else {
        qCWarning(uiLogging) << "Unable to find qwebchannel.js or createGlobalEventBridge.js";
    }
    return javaScriptToInject;
}

class EventBridgeWrapper : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* eventBridge READ getEventBridge CONSTANT);

public:
    EventBridgeWrapper(QObject* eventBridge, QObject* parent = nullptr)
        : QObject(parent)
        , _eventBridge(eventBridge) {}

    QObject* getEventBridge() { return _eventBridge; }

private:
    QObject* _eventBridge;
};

}}}  // namespace hifi::qml::offscreen

using namespace hifi::qml::offscreen;

AudioHandler::AudioHandler(OffscreenQmlSurface* surface, const QString& deviceName, QObject* parent)
    : QObject(parent) {
    setAutoDelete(true);
    _newTargetDevice = deviceName;
    auto rootItem = surface->getRootItem();
    if (rootItem) {
        for (auto player : rootItem->findChildren<QMediaPlayer*>()) {
            _players.push_back(player);
        }
    }

    if (!_newTargetDevice.isEmpty() && !_players.empty()) {
        QThreadPool::globalInstance()->start(this);
    } else {
        deleteLater();
    }
}

void AudioHandler::run() {
    for (auto player : _players) {
        auto mediaState = player->state();
        QMediaService* svc = player->service();
        if (nullptr == svc) {
            continue;
        }
        QAudioOutputSelectorControl* out =
            qobject_cast<QAudioOutputSelectorControl*>(svc->requestControl(QAudioOutputSelectorControl_iid));
        if (nullptr == out) {
            continue;
        }
        QString deviceOuput;
        auto outputs = out->availableOutputs();
        for (int i = 0; i < outputs.size(); i++) {
            QString output = outputs[i];
            QString description = out->outputDescription(output);
            if (description == _newTargetDevice) {
                deviceOuput = output;
                break;
            }
        }
        out->setActiveOutput(deviceOuput);
        svc->releaseControl(out);
        // if multimedia was paused, it will start playing automatically after changing audio device
        // this will reset it back to a paused state
        if (mediaState == QMediaPlayer::State::PausedState) {
            player->pause();
        } else if (mediaState == QMediaPlayer::State::StoppedState) {
            player->stop();
        }
    }
    qDebug() << "QML Audio changed to " << _newTargetDevice;
}

OffscreenQmlSurface::~OffscreenQmlSurface() {
    clearFocusItem();
}

void OffscreenQmlSurface::clearFocusItem() {
    if (_currentFocusItem) {
        disconnect(_currentFocusItem, &QObject::destroyed, this, &OffscreenQmlSurface::focusDestroyed);
    }
    _currentFocusItem = nullptr;
}

void OffscreenQmlSurface::initializeEngine(QQmlEngine* engine) {
    Parent::initializeEngine(engine);
    auto fileSelector = QQmlFileSelector::get(engine);
    if (!fileSelector) {
        fileSelector = new QQmlFileSelector(engine);
    }
    fileSelector->setExtraSelectors(FileUtils::getFileSelectors());

    static std::once_flag once;
    std::call_once(once, [] { 
        qRegisterMetaType<TabletProxy*>();
        qRegisterMetaType<TabletButtonProxy*>();
        qmlRegisterType<SoundEffect>("Hifi", 1, 0, "SoundEffect");
    });

    engine->setNetworkAccessManagerFactory(new QmlNetworkAccessManagerFactory);
    auto importList = engine->importPathList();
    importList.insert(importList.begin(), PathUtils::resourcesPath() + "qml/");
    importList.insert(importList.begin(), PathUtils::resourcesPath());
    engine->setImportPathList(importList);
    for (const auto& path : importList) {
        qDebug() << path;
    }

    auto rootContext = engine->rootContext();

    static QJsonObject QML_GL_INFO;
    static std::once_flag once_gl_info;
    std::call_once(once_gl_info, [] {
        const auto& contextInfo = gl::ContextInfo::get();
        QML_GL_INFO = QJsonObject {
            { "version", contextInfo.version.c_str() },
            { "sl_version", contextInfo.shadingLanguageVersion.c_str() },
            { "vendor", contextInfo.vendor.c_str() },
            { "renderer", contextInfo.renderer.c_str() },
        };
    });
    rootContext->setContextProperty("GL", QML_GL_INFO);
    rootContext->setContextProperty("urlHandler", new UrlHandler(rootContext));
    rootContext->setContextProperty("resourceDirectoryUrl", QUrl::fromLocalFile(PathUtils::resourcesPath()));
    rootContext->setContextProperty("ApplicationInterface", qApp);
    auto javaScriptToInject = getEventBridgeJavascript();
    if (!javaScriptToInject.isEmpty()) {
        rootContext->setContextProperty("eventBridgeJavaScriptToInject", QVariant(javaScriptToInject));
    }
    rootContext->setContextProperty("Paths", DependencyManager::get<PathUtils>().data());
    rootContext->setContextProperty("Tablet", DependencyManager::get<TabletScriptingInterface>().data());
    rootContext->setContextProperty("Toolbars", DependencyManager::get<ToolbarScriptingInterface>().data());
    TabletProxy* tablet =
        DependencyManager::get<TabletScriptingInterface>()->getTablet("com.highfidelity.interface.tablet.system");
    engine->setObjectOwnership(tablet, QQmlEngine::CppOwnership);
}

void OffscreenQmlSurface::addWhitelistContextHandler(const std::initializer_list<QUrl>& urls,
                                                     const QmlContextCallback& callback) {
    getQmlWhitelist()->addWhitelistContextHandler(urls, callback);
}

void OffscreenQmlSurface::onRootContextCreated(QQmlContext* qmlContext) {
    OffscreenSurface::onRootContextCreated(qmlContext);
    qmlContext->setBaseUrl(PathUtils::qmlBaseUrl());
    qmlContext->setContextProperty("eventBridge", this);
    qmlContext->setContextProperty("webEntity", this);
    qmlContext->setContextProperty("QmlSurface", this);
    // FIXME Compatibility mechanism for existing HTML and JS that uses eventBridgeWrapper
    // Find a way to flag older scripts using this mechanism and wanr that this is deprecated
    qmlContext->setContextProperty("eventBridgeWrapper", new EventBridgeWrapper(this, qmlContext));
#if !defined(Q_OS_ANDROID)
    {
        PROFILE_RANGE(startup, "FileTypeProfile");
        FileTypeProfile::registerWithContext(qmlContext);
    }
    {
        PROFILE_RANGE(startup, "HFWebEngineProfile");
        HFWebEngineProfile::registerWithContext(qmlContext);

    }
#endif
}

void OffscreenQmlSurface::applyWhiteList(const QUrl& url, QQmlContext* context) {
    QList<QmlContextCallback> callbacks = getQmlWhitelist()->getCallbacksForUrl(url);
    for(const auto& callback : callbacks){
        callback(context);
    }
}

QQmlContext* OffscreenQmlSurface::contextForUrl(const QUrl& qmlSource, QQuickItem* parent, bool forceNewContext) {
    // Get any whitelist functionality
    QList<QmlContextCallback> callbacks = getQmlWhitelist()->getCallbacksForUrl(qmlSource);
    // If we have whitelisted content, we must load a new context
    forceNewContext |= !callbacks.empty();

    QQmlContext* targetContext = Parent::contextForUrl(qmlSource, parent, forceNewContext);

    for (const auto& callback : callbacks) {
        callback(targetContext);
    }

    return targetContext;
}

void OffscreenQmlSurface::onItemCreated(QQmlContext* qmlContext, QQuickItem* newItem) {
    QObject* eventBridge = qmlContext->contextProperty("eventBridge").value<QObject*>();
    if (qmlContext != getSurfaceContext() && eventBridge && eventBridge != this) {
        // FIXME Compatibility mechanism for existing HTML and JS that uses eventBridgeWrapper
        // Find a way to flag older scripts using this mechanism and wanr that this is deprecated
        qmlContext->setContextProperty("eventBridgeWrapper", new EventBridgeWrapper(eventBridge, qmlContext));
    }

}

void OffscreenQmlSurface::onRootCreated() {
    getSurfaceContext()->setContextProperty("offscreenWindow", QVariant::fromValue(getWindow()));

    // Connect with the audio client and listen for audio device changes
    connect(DependencyManager::get<AudioClient>().data(), &AudioClient::deviceChanged, this, [this](QAudio::Mode mode, const HifiAudioDeviceInfo& device) {
        if (mode == QAudio::Mode::AudioOutput) {
            QMetaObject::invokeMethod(this, "changeAudioOutputDevice", Qt::QueuedConnection, Q_ARG(QString, device.deviceName()));
        }
    });

#if !defined(Q_OS_ANDROID)
    // Setup the update of the QML media components with the current audio output device
    QObject::connect(&_audioOutputUpdateTimer, &QTimer::timeout, this, [this]() {
        if (_currentAudioOutputDevice.size() > 0) {
            new AudioHandler(this, _currentAudioOutputDevice);
        }
    });
    int waitForAudioQmlMs = 200;
    _audioOutputUpdateTimer.setInterval(waitForAudioQmlMs);
    _audioOutputUpdateTimer.setSingleShot(true);
#endif

    if (getRootItem()->objectName() == "tabletRoot") {
        getSurfaceContext()->setContextProperty("tabletRoot", QVariant::fromValue(getRootItem()));
        auto tabletScriptingInterface = DependencyManager::get<TabletScriptingInterface>();
        tabletScriptingInterface->setQmlTabletRoot("com.highfidelity.interface.tablet.system", this);
        QObject* tablet = tabletScriptingInterface->getTablet("com.highfidelity.interface.tablet.system");
        getSurfaceContext()->engine()->setObjectOwnership(tablet, QQmlEngine::CppOwnership);
    }
    QMetaObject::invokeMethod(this, "forceQmlAudioOutputDeviceUpdate", Qt::QueuedConnection);
}

QPointF OffscreenQmlSurface::mapWindowToUi(const QPointF& sourcePosition, QObject* sourceObject) {
    vec2 sourceSize;
    if (dynamic_cast<QWidget*>(sourceObject)) {
        sourceSize = toGlm(((QWidget*)sourceObject)->size());
    } else if (dynamic_cast<QWindow*>(sourceObject)) {
        sourceSize = toGlm(((QWindow*)sourceObject)->size());
    }
    vec2 offscreenPosition = toGlm(sourcePosition);
    offscreenPosition /= sourceSize;
    offscreenPosition *= vec2(toGlm(getWindow()->size()));
    return QPointF(offscreenPosition.x, offscreenPosition.y);
}

///////////////////////////////////////////////////////
//
// Event handling customization
//

bool OffscreenQmlSurface::eventFilter(QObject* originalDestination, QEvent* event) {
    if (!filterEnabled(originalDestination, event)) {
        return false;
    }

    if (event->type() == QEvent::Resize) {
        QResizeEvent* resizeEvent = static_cast<QResizeEvent*>(event);
        QWidget* widget = static_cast<QWidget*>(originalDestination);
        if (widget) {
            this->resize(resizeEvent->size());
        }
    }

    return Parent::eventFilter(originalDestination, event);
}

unsigned int OffscreenQmlSurface::deviceIdByTouchPoint(qreal x, qreal y) {
    if (!getRootItem()) {
        return PointerEvent::INVALID_POINTER_ID;
    }

    auto mapped = getRootItem()->mapFromGlobal(QPoint(x, y));
    for (auto pair : _activeTouchPoints) {
        if (mapped.x() == (int)pair.second.touchPoint.pos().x() && mapped.y() == (int)pair.second.touchPoint.pos().y()) {
            return pair.first;
        }
    }

    return PointerEvent::INVALID_POINTER_ID;
}

PointerEvent::EventType OffscreenQmlSurface::choosePointerEventType(QEvent::Type type) {
    switch (type) {
        case QEvent::MouseButtonDblClick:
            return PointerEvent::DoublePress;
        case QEvent::MouseButtonPress:
            return PointerEvent::Press;
        case QEvent::MouseButtonRelease:
            return PointerEvent::Release;
        case QEvent::MouseMove:
            return PointerEvent::Move;
        default:
            return PointerEvent::Move;
    }
}

void OffscreenQmlSurface::hoverBeginEvent(const PointerEvent& event, class QTouchDevice& device) {
#if defined(DISABLE_QML)
    return;
#endif
    handlePointerEvent(event, device);
    _activeTouchPoints[event.getID()].hovering = true;
}

void OffscreenQmlSurface::hoverEndEvent(const PointerEvent& event, class QTouchDevice& device) {
#if defined(DISABLE_QML)
    return;
#endif
    _activeTouchPoints[event.getID()].hovering = false;
    // Send a fake mouse move event if
    // - the event told us to
    // - we aren't pressing with this ID
    if (event.sendMoveOnHoverLeave() || !_activeTouchPoints[event.getID()].pressed) {
        // QML onReleased is only triggered if a click has happened first.  We need to send this "fake" mouse move event to properly trigger an onExited.
        PointerEvent endMoveEvent(PointerEvent::Move, event.getID());
        // If we aren't pressing, we want to release this TouchPoint
        handlePointerEvent(endMoveEvent, device, !_activeTouchPoints[event.getID()].pressed);
    }
}

bool OffscreenQmlSurface::handlePointerEvent(const PointerEvent& event, class QTouchDevice& device, bool release) {
#if defined(DISABLE_QML)
    return false;
#endif

    // Ignore mouse interaction if we're paused
    if (!getRootItem() || isPaused()) {
        return false;
    }

    QPointF windowPoint(event.getPos2D().x, event.getPos2D().y);

    Qt::TouchPointState state = Qt::TouchPointStationary;
    if (event.getType() == PointerEvent::Press && event.getButton() == PointerEvent::PrimaryButton) {
        state = Qt::TouchPointPressed;
    } else if (event.getType() == PointerEvent::Release && event.getButton() == PointerEvent::PrimaryButton) {
        state = Qt::TouchPointReleased;
    } else if (_activeTouchPoints.count(event.getID()) && windowPoint != _activeTouchPoints[event.getID()].touchPoint.pos()) {
        state = Qt::TouchPointMoved;
    }

    // Remove the touch point if:
    // - this was a hover end event and the mouse wasn't pressed
    // - this was a release event and we aren't still hovering
    auto touchPoint = _activeTouchPoints.find(event.getID());
    bool removeTouchPoint =
        release || (touchPoint != _activeTouchPoints.end() && !touchPoint->second.hovering && state == Qt::TouchPointReleased);
    QEvent::Type touchType = QEvent::TouchUpdate;
    if (_activeTouchPoints.empty()) {
        // If the first active touch point is being created, send a begin
        touchType = QEvent::TouchBegin;
    } else if (removeTouchPoint && _activeTouchPoints.size() == 1 && _activeTouchPoints.count(event.getID())) {
        // If the last active touch point is being released, send an end
        touchType = QEvent::TouchEnd;
    }

    {
        QTouchEvent::TouchPoint point;
        point.setId(event.getID());
        point.setState(state);
        point.setPos(windowPoint);
        point.setScreenPos(windowPoint);
        _activeTouchPoints[event.getID()].touchPoint = point;
        if (state == Qt::TouchPointPressed) {
            _activeTouchPoints[event.getID()].pressed = true;
        } else if (state == Qt::TouchPointReleased) {
            _activeTouchPoints[event.getID()].pressed = false;
        }
    }

    QTouchEvent touchEvent(touchType, &device, event.getKeyboardModifiers());
    {
        QList<QTouchEvent::TouchPoint> touchPoints;
        Qt::TouchPointStates touchPointStates;
        for (const auto& entry : _activeTouchPoints) {
            touchPointStates |= entry.second.touchPoint.state();
            touchPoints.push_back(entry.second.touchPoint);
        }

        touchEvent.setDevice(&device);
        touchEvent.setWindow(getWindow());
        touchEvent.setTarget(getRootItem());
        touchEvent.setTouchPoints(touchPoints);
        touchEvent.setTouchPointStates(touchPointStates);
        touchEvent.setTimestamp((ulong)QDateTime::currentMSecsSinceEpoch());
        touchEvent.ignore();
    }

    // Send mouse events to the surface so that HTML dialog elements work with mouse press and hover.
    //
    // In Qt 5.9 mouse events must be sent before touch events to make sure some QtQuick components will
    // receive mouse events
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    if (event.getButton() == PointerEvent::PrimaryButton) {
        button = Qt::LeftButton;
    }
    if (event.getButtons() & PointerEvent::PrimaryButton) {
        buttons |= Qt::LeftButton;
    }

    bool eventSent = false;
    bool eventsAccepted = true;

    if (event.getType() == PointerEvent::Move) {
        QMouseEvent mouseEvent(QEvent::MouseMove, windowPoint, windowPoint, windowPoint, button, buttons,
                               event.getKeyboardModifiers());
        // TODO - this line necessary for the QML Tooltop to work (which is not currently being used), but it causes interface to crash on launch on a fresh install
        // need to investigate into why this crash is happening.
        //_qmlContext->setContextProperty("lastMousePosition", windowPoint);
        mouseEvent.ignore();
        if (QCoreApplication::sendEvent(getWindow(), &mouseEvent)) {
            eventSent = true;
            eventsAccepted &= mouseEvent.isAccepted();
        }
    }

    if (touchType == QEvent::TouchBegin) {
        _touchBeginAccepted = QCoreApplication::sendEvent(getWindow(), &touchEvent);
        if (_touchBeginAccepted) {
            eventSent = true;
            eventsAccepted &= touchEvent.isAccepted();
        }
    } else if (_touchBeginAccepted) {
        if (QCoreApplication::sendEvent(getWindow(), &touchEvent)) {
            eventSent = true;
            eventsAccepted &= touchEvent.isAccepted();
        }
    }

    if (removeTouchPoint) {
        _activeTouchPoints.erase(event.getID());
    }

    return eventSent && eventsAccepted;
}

void OffscreenQmlSurface::focusDestroyed(QObject* obj) {
    clearFocusItem();
}

void OffscreenQmlSurface::onFocusObjectChanged(QObject* object) {
    clearFocusItem();

    QQuickItem* item = static_cast<QQuickItem*>(object);
    if (!item) {
        setFocusText(false);
        return;
    }

    QInputMethodQueryEvent query(Qt::ImEnabled);
    qApp->sendEvent(object, &query);
    setFocusText(query.value(Qt::ImEnabled).toBool());

    // Raise and lower keyboard for QML text fields.
    // HTML text fields are handled in emitWebEvent() methods - testing READ_ONLY_PROPERTY prevents action for HTML files.
    const char* READ_ONLY_PROPERTY = "readOnly";
    bool raiseKeyboard = item->hasActiveFocus() && item->property(READ_ONLY_PROPERTY) == false;
    if (_currentFocusItem && !raiseKeyboard) {
        setKeyboardRaised(_currentFocusItem, false);
    }
    setKeyboardRaised(item, raiseKeyboard);  // Always set focus so that alphabetic / numeric setting is updated.

    _currentFocusItem = item;
    connect(_currentFocusItem, &QObject::destroyed, this, &OffscreenQmlSurface::focusDestroyed);
}

void OffscreenQmlSurface::setFocusText(bool newFocusText) {
    if (newFocusText != _focusText) {
        _focusText = newFocusText;
        emit focusTextChanged(_focusText);
    }
}

// UTF-8 encoded symbols
static const uint8_t SHIFT_ARROW[] = { 0xE2, 0x87, 0xAA, 0x00 };
static const uint8_t NUMERIC_SHIFT_ARROW[] = { 0xE2, 0x87, 0xA8, 0x00 };
static const uint8_t BACKSPACE_SYMBOL[] = { 0xE2, 0x86, 0x90, 0x00 };
static const uint8_t LEFT_ARROW[] = { 0xE2, 0x9D, 0xAC, 0x00 };
static const uint8_t RIGHT_ARROW[] = { 0xE2, 0x9D, 0xAD, 0x00 };
static const uint8_t RETURN_SYMBOL[] = { 0xE2, 0x8F, 0x8E, 0x00 };
static const uint8_t COLLAPSE_KEYBOARD[] = { 0xEE, 0x80, 0xAB, 0x00 };
static const char PUNCTUATION_STRING[] = "123";
static const char ALPHABET_STRING[] = "abc";

static bool equals(const QByteArray& byteArray, const uint8_t* ptr) {
    int i;
    for (i = 0; i < byteArray.size(); i++) {
        if ((char)ptr[i] != byteArray[i]) {
            return false;
        }
    }
    return ptr[i] == 0x00;
}

void OffscreenQmlSurface::synthesizeKeyPress(QString key, QObject* targetOverride) {
    auto eventHandler = targetOverride ? targetOverride : getEventHandler();
    if (eventHandler) {
        auto utf8Key = key.toUtf8();

        int scanCode = (int)utf8Key[0];
        QString keyString = key;
        if (equals(utf8Key, SHIFT_ARROW) || equals(utf8Key, NUMERIC_SHIFT_ARROW) ||
            equals(utf8Key, (uint8_t*)PUNCTUATION_STRING) || equals(utf8Key, (uint8_t*)ALPHABET_STRING)) {
            return;  // ignore
        } else if (equals(utf8Key, COLLAPSE_KEYBOARD)) {
            lowerKeyboard();
            return;
        } else if (equals(utf8Key, BACKSPACE_SYMBOL)) {
            scanCode = Qt::Key_Backspace;
            keyString = "\x08";
        } else if (equals(utf8Key, RETURN_SYMBOL)) {
            scanCode = Qt::Key_Return;
            keyString = "\x0d";
        } else if (equals(utf8Key, LEFT_ARROW)) {
            scanCode = Qt::Key_Left;
            keyString = "";
        } else if (equals(utf8Key, RIGHT_ARROW)) {
            scanCode = Qt::Key_Right;
            keyString = "";
        }

        QKeyEvent* pressEvent = new QKeyEvent(QEvent::KeyPress, scanCode, Qt::NoModifier, keyString);
        QKeyEvent* releaseEvent = new QKeyEvent(QEvent::KeyRelease, scanCode, Qt::NoModifier, keyString);
        QCoreApplication::postEvent(eventHandler, pressEvent);
        QCoreApplication::postEvent(eventHandler, releaseEvent);
    }
}

void OffscreenQmlSurface::lowerKeyboard() {
    QSignalBlocker blocker(getWindow());

    if (_currentFocusItem) {
        _currentFocusItem->setFocus(false);
        setKeyboardRaised(_currentFocusItem, false);
    }
}

void OffscreenQmlSurface::setKeyboardRaised(QObject* object, bool raised, bool numeric, bool passwordField) {
    qCDebug(uiLogging) << "setKeyboardRaised: " << object << ", raised: " << raised << ", numeric: " << numeric
                       << ", password: " << passwordField;

    if (!object) {
        return;
    }

    bool android = false;
#if defined(Q_OS_ANDROID)
    android = true;
#endif

    bool hmd = qApp->property(hifi::properties::HMD).toBool();

    if (!android || hmd) {
        // if HMD is being worn, allow keyboard to open.  allow it to close, HMD or not.
        if (!raised || hmd) {
            QQuickItem* item = dynamic_cast<QQuickItem*>(object);
            if (!item) {
                return;
            }

            // for future probably makes sense to consider one of the following:
            // 1. make keyboard a singleton, which will be dynamically re-parented before showing
            // 2. track currently visible keyboard somewhere, allow to subscribe for this signal
            // any of above should also eliminate need in duplicated properties and code below

            while (item) {
                // Numeric value may be set in parameter from HTML UI; for QML UI, detect numeric fields here.
                numeric = numeric || QString(item->metaObject()->className()).left(7) == "SpinBox";

                if (item->property("keyboardRaised").isValid()) {

                    if (item->property("punctuationMode").isValid()) {
                        item->setProperty("punctuationMode", QVariant(numeric));
                    }
                    if (item->property("passwordField").isValid()) {
                        item->setProperty("passwordField", QVariant(passwordField));
                    }

                    if (hmd && item->property("keyboardEnabled").isValid()) {
                        item->setProperty("keyboardEnabled", true);
                    }

                    item->setProperty("keyboardRaised", QVariant(raised));

                    return;
                }
                item = dynamic_cast<QQuickItem*>(item->parentItem());
            }
        }
    }

}

void OffscreenQmlSurface::emitScriptEvent(const QVariant& message) {
    emit scriptEventReceived(message);
}

void OffscreenQmlSurface::emitWebEvent(const QVariant& message) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "emitWebEvent", Qt::QueuedConnection, Q_ARG(QVariant, message));
    } else {
        // Special case to handle raising and lowering the virtual keyboard.
        const QString RAISE_KEYBOARD = "_RAISE_KEYBOARD";
        const QString RAISE_KEYBOARD_NUMERIC = "_RAISE_KEYBOARD_NUMERIC";
        const QString LOWER_KEYBOARD = "_LOWER_KEYBOARD";
        const QString RAISE_KEYBOARD_NUMERIC_PASSWORD = "_RAISE_KEYBOARD_NUMERIC_PASSWORD";
        const QString RAISE_KEYBOARD_PASSWORD = "_RAISE_KEYBOARD_PASSWORD";
        QString messageString = message.type() == QVariant::String ? message.toString() : "";
        if (messageString.left(RAISE_KEYBOARD.length()) == RAISE_KEYBOARD) {
            bool numeric = (messageString == RAISE_KEYBOARD_NUMERIC || messageString == RAISE_KEYBOARD_NUMERIC_PASSWORD);
            bool passwordField = (messageString == RAISE_KEYBOARD_PASSWORD || messageString == RAISE_KEYBOARD_NUMERIC_PASSWORD);
            setKeyboardRaised(_currentFocusItem, true, numeric, passwordField);
        } else if (messageString == LOWER_KEYBOARD) {
            setKeyboardRaised(_currentFocusItem, false);
        } else {
            emit webEventReceived(message);
        }
    }
}

void OffscreenQmlSurface::sendToQml(const QVariant& message) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "emitQmlEvent", Qt::QueuedConnection, Q_ARG(QVariant, message));
    } else if (getRootItem()) {
        // call fromScript method on qml root
        QMetaObject::invokeMethod(getRootItem(), "fromScript", Qt::QueuedConnection, Q_ARG(QVariant, message));
    }
}

void OffscreenQmlSurface::changeAudioOutputDevice(const QString& deviceName, bool isHtmlUpdate) {
#if !defined(Q_OS_ANDROID)
    _currentAudioOutputDevice = deviceName;
    if (getRootItem() && !isHtmlUpdate) {
        QMetaObject::invokeMethod(this, "forceQmlAudioOutputDeviceUpdate", Qt::QueuedConnection);
    }
    emit audioOutputDeviceChanged(deviceName);
#endif
}

void OffscreenQmlSurface::forceHtmlAudioOutputDeviceUpdate() {
#if !defined(Q_OS_ANDROID)
    if (_currentAudioOutputDevice.size() > 0) {
        QMetaObject::invokeMethod(this, "changeAudioOutputDevice", Qt::QueuedConnection,
            Q_ARG(QString, _currentAudioOutputDevice), Q_ARG(bool, true));
    }
#endif
}

void OffscreenQmlSurface::forceQmlAudioOutputDeviceUpdate() {
#if !defined(Q_OS_ANDROID)
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, "forceQmlAudioOutputDeviceUpdate", Qt::QueuedConnection);
    } else {
        if (_audioOutputUpdateTimer.isActive()) {
            _audioOutputUpdateTimer.stop();
        }
        _audioOutputUpdateTimer.start();
    }
#endif
}

void OffscreenQmlSurface::loadFromQml(const QUrl& qmlSource, QQuickItem* parent, const QJSValue& callback) {
    auto objectCallback = [callback](QQmlContext* context, QQuickItem* newItem) {
        QJSValue(callback).call(QJSValueList() << context->engine()->newQObject(newItem));
    };

    if (hifi::scripting::isLocalAccessSafeThread()) {
        // If this is a 
        auto contextCallback = [callback](QQmlContext* context) {
            ContextAwareProfile::restrictContext(context, false);
#if !defined(Q_OS_ANDROID)
            FileTypeProfile::registerWithContext(context);
            HFWebEngineProfile::registerWithContext(context);
#endif
        };
        loadInternal(qmlSource, true, parent, objectCallback, contextCallback);
    } else {
        loadInternal(qmlSource, false, parent, objectCallback);
    }
}

#include "OffscreenQmlSurface.moc"
