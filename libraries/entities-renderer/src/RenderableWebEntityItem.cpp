//
//  Created by Bradley Austin Davis on 2015/05/12
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableWebEntityItem.h"
#include <atomic>

#include <QtCore/QTimer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QTouchDevice>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlContext>


#include <GeometryCache.h>
#include <PathUtils.h>
#include <PointerEvent.h>
#include <gl/GLHelpers.h>
#include <ui/OffscreenQmlSurface.h>
#include <ui/TabletScriptingInterface.h>
#include <EntityScriptingInterface.h>
#include <shared/LocalFileAccessGate.h>

#include "EntitiesRendererLogging.h"
#include <NetworkingConstants.h>
#include <MetaverseAPI.h>

using namespace render;
using namespace render::entities;

const QString WebEntityRenderer::QML = "Web3DSurface.qml";
const char* WebEntityRenderer::URL_PROPERTY = "url";
const char* WebEntityRenderer::SCRIPT_URL_PROPERTY = "scriptURL";
const char* WebEntityRenderer::GLOBAL_POSITION_PROPERTY = "globalPosition";
const char* WebEntityRenderer::USE_BACKGROUND_PROPERTY = "useBackground";
const char* WebEntityRenderer::USER_AGENT_PROPERTY = "userAgent";

std::function<void(QString, bool, QSharedPointer<OffscreenQmlSurface>&, bool&)> WebEntityRenderer::_acquireWebSurfaceOperator = nullptr;
std::function<void(QSharedPointer<OffscreenQmlSurface>&, bool&, std::vector<QMetaObject::Connection>&)> WebEntityRenderer::_releaseWebSurfaceOperator = nullptr;

static int MAX_WINDOW_SIZE = 4096;
const float METERS_TO_INCHES = 39.3701f;
static float OPAQUE_ALPHA_THRESHOLD = 0.99f;

// If a web-view hasn't been rendered for 30 seconds, de-allocate the framebuffer
static uint64_t MAX_NO_RENDER_INTERVAL = 30 * USECS_PER_SECOND;

static uint8_t YOUTUBE_MAX_FPS = 30;

// Don't allow more than 20 concurrent web views
static std::atomic<uint32_t> _currentWebCount(0);
static const uint32_t MAX_CONCURRENT_WEB_VIEWS = 20;

static QTouchDevice _touchDevice;

WebEntityRenderer::ContentType WebEntityRenderer::getContentType(const QString& urlString) {
    if (urlString.isEmpty()) {
        return ContentType::NoContent;
    }

    const QUrl url(urlString);
    auto scheme = url.scheme();
    if (scheme == HIFI_URL_SCHEME_ABOUT || scheme == HIFI_URL_SCHEME_HTTP || scheme == HIFI_URL_SCHEME_HTTPS ||
        scheme == URL_SCHEME_DATA ||
        urlString.toLower().endsWith(".htm") || urlString.toLower().endsWith(".html")) {
        return ContentType::HtmlContent;
    }
    return ContentType::QmlContent;
}

WebEntityRenderer::WebEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    static std::once_flag once;
    std::call_once(once, [&]{
        _touchDevice.setCapabilities(QTouchDevice::Position);
        _touchDevice.setType(QTouchDevice::TouchScreen);
        _touchDevice.setName("WebEntityRendererTouchDevice");
        _touchDevice.setMaximumTouchPoints(4);
    });
    _geometryId = DependencyManager::get<GeometryCache>()->allocateID();

    _texture = gpu::Texture::createExternal(OffscreenQmlSurface::getDiscardLambda());
    _texture->setSource(__FUNCTION__);

    _contentType = ContentType::HtmlContent;
    buildWebSurface(entity, "");

    _timer.setInterval(MSECS_PER_SECOND);
    connect(&_timer, &QTimer::timeout, this, &WebEntityRenderer::onTimeout);
}

WebEntityRenderer::~WebEntityRenderer() {
    destroyWebSurface();

    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        geometryCache->releaseID(_geometryId);
    }
}

bool WebEntityRenderer::isTransparent() const {
    float fadeRatio = _isFading ? Interpolate::calculateFadeRatio(_fadeStartTime) : 1.0f;
    return fadeRatio < OPAQUE_ALPHA_THRESHOLD || _alpha < 1.0f || _pulseProperties.getAlphaMode() != PulseMode::NONE || !_useBackground;
}

bool WebEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (resultWithReadLock<bool>([&] {
        if (_webSurface && uvec2(getWindowSize(entity)) != toGlm(_webSurface->size())) {
            return true;
        }

        if (_contextPosition != entity->getWorldPosition()) {
            return true;
        }

        return false;
    })) {
        return true;
    }

    return false;
}

void WebEntityRenderer::onTimeout() {
    uint64_t lastRenderTime;
    if (!resultWithReadLock<bool>([&] {
        lastRenderTime = _lastRenderTime;
        return (_lastRenderTime != 0 && (bool)_webSurface);
    })) {
        return;
    }

    if (usecTimestampNow() - lastRenderTime > MAX_NO_RENDER_INTERVAL) {
        destroyWebSurface();
    }
}

void WebEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    // If the content type has changed, or the old content type was QML, we need to
    // destroy the existing surface (because surfaces don't support changing the root
    // object, so subsequent loads of content just overlap the existing content
    bool urlChanged = false;
    auto newSourceURL = entity->getSourceUrl();
    {
        auto newContentType = getContentType(newSourceURL);
        ContentType currentContentType;
        withReadLock([&] {
            urlChanged = newSourceURL.isEmpty() || newSourceURL != _tryingToBuildURL;
        });
        currentContentType = _contentType;

        if (urlChanged) {
            if (newContentType != ContentType::HtmlContent || currentContentType != ContentType::HtmlContent) {
                destroyWebSurface();
            }
            _contentType = newContentType;
        }
    }

    withWriteLock([&] {
        _inputMode = entity->getInputMode();
        _dpi = entity->getDPI();
        _color = entity->getColor();
        _alpha = entity->getAlpha();
        _pulseProperties = entity->getPulseProperties();

        if (_contentType == ContentType::NoContent) {
            _tryingToBuildURL = newSourceURL;
            _sourceURL = newSourceURL;
            return;
        }

        // This work must be done on the main thread
        bool localSafeContext = entity->getLocalSafeContext();
        if (!_webSurface) {
            if (localSafeContext) {
                ::hifi::scripting::setLocalAccessSafeThread(true);
            }
            buildWebSurface(entity, newSourceURL);
            ::hifi::scripting::setLocalAccessSafeThread(false);
        }

        if (_webSurface) {
            if (_webSurface->getRootItem()) {
                if (_contentType == ContentType::HtmlContent && _sourceURL != newSourceURL) {            
                    if (localSafeContext) {
                        ::hifi::scripting::setLocalAccessSafeThread(true);
                    }
                    _webSurface->getRootItem()->setProperty(URL_PROPERTY, newSourceURL);
                    _webSurface->getRootItem()->setProperty(SCRIPT_URL_PROPERTY, _scriptURL);
                    _webSurface->getRootItem()->setProperty(USE_BACKGROUND_PROPERTY, _useBackground);
                    _webSurface->getRootItem()->setProperty(USER_AGENT_PROPERTY, _userAgent);
                    _webSurface->getSurfaceContext()->setContextProperty(GLOBAL_POSITION_PROPERTY, vec3toVariant(_contextPosition));
                    _webSurface->setMaxFps((QUrl(newSourceURL).host().endsWith("youtube.com", Qt::CaseInsensitive)) ? YOUTUBE_MAX_FPS : _maxFPS);
                    ::hifi::scripting::setLocalAccessSafeThread(false);
                    _sourceURL = newSourceURL;
                } else if (_contentType != ContentType::HtmlContent) {
                    _sourceURL = newSourceURL;
                }

                {
                    auto scriptURL = entity->getScriptURL();
                    if (_scriptURL != scriptURL) {
                        _webSurface->getRootItem()->setProperty(SCRIPT_URL_PROPERTY, scriptURL);
                        _scriptURL = scriptURL;
                    }
                }

                {
                    auto maxFPS = entity->getMaxFPS();
                    if (_maxFPS != maxFPS) {
                        // We special case YouTube URLs since we know they are videos that we should play with at least 30 FPS.
                        // FIXME this doesn't handle redirects or shortened URLs, consider using a signaling method from the web entity
                        if (QUrl(_sourceURL).host().endsWith("youtube.com", Qt::CaseInsensitive)) {
                            _webSurface->setMaxFps(YOUTUBE_MAX_FPS);
                        } else {
                            _webSurface->setMaxFps(maxFPS);
                        }
                        _maxFPS = maxFPS;
                    }
                }

                { 
                    auto useBackground = entity->getUseBackground();
                    if (_useBackground != useBackground) {
                        _webSurface->getRootItem()->setProperty(USE_BACKGROUND_PROPERTY, useBackground);
                        _useBackground = useBackground;
                    }
                }
                
                { 
                    auto userAgent = entity->getUserAgent();
                    if (_userAgent != userAgent) {
                        _webSurface->getRootItem()->setProperty(USER_AGENT_PROPERTY, userAgent);
                        _userAgent = userAgent;
                    }
                }

                {
                    auto contextPosition = entity->getWorldPosition();
                    if (_contextPosition != contextPosition) {
                        _webSurface->getSurfaceContext()->setContextProperty(GLOBAL_POSITION_PROPERTY, vec3toVariant(contextPosition));
                        _contextPosition = contextPosition;
                    }
                }
            }

            void* key = (void*)this;
            AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this, entity] {
                withWriteLock([&] {
                    glm::vec2 windowSize = getWindowSize(entity);
                    _webSurface->resize(QSize(windowSize.x, windowSize.y));
                    _renderTransform = getModelTransform();
                    _renderTransform.setScale(1.0f);
                    _renderTransform.postScale(entity->getScaledDimensions());
                });
            });
        } else {
            emit requestRenderUpdate();
        }
    });
}

void WebEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("WebEntityRenderer::render");
    withWriteLock([&] {
        _lastRenderTime = usecTimestampNow();
    });

    // Try to update the texture
    OffscreenQmlSurface::TextureAndFence newTextureAndFence;
    bool newTextureAvailable = false;
    if (!resultWithReadLock<bool>([&] {
        if (!_webSurface) {
            return false;
        }

        newTextureAvailable = _webSurface->fetchTexture(newTextureAndFence);
        return true;
    })) {
        return;
    }

    if (newTextureAvailable) {
        _texture->setExternalTexture(newTextureAndFence.first, newTextureAndFence.second);
    }

    static const glm::vec2 texMin(0.0f), texMax(1.0f), topLeft(-0.5f), bottomRight(0.5f);

    gpu::Batch& batch = *args->_batch;
    glm::vec4 color;
    Transform transform;
    bool transparent;
    withReadLock([&] {
        float fadeRatio = _isFading ? Interpolate::calculateFadeRatio(_fadeStartTime) : 1.0f;
        color = glm::vec4(toGlm(_color), _alpha * fadeRatio);
        color = EntityRenderer::calculatePulseColor(color, _pulseProperties, _created);
        transform = _renderTransform;
        transparent = isTransparent();
    });

    if (color.a == 0.0f) {
        return;
    }

    bool forward = _renderLayer != RenderLayer::WORLD || args->_renderMethod == render::Args::FORWARD;

    batch.setResourceTexture(0, _texture);

    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    batch.setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }

    // Turn off jitter for these entities
    batch.pushProjectionJitterEnabled(false);
    DependencyManager::get<GeometryCache>()->bindWebBrowserProgram(batch, transparent, forward);
    DependencyManager::get<GeometryCache>()->renderQuad(batch, topLeft, bottomRight, texMin, texMax, color, _geometryId);
    batch.popProjectionJitterEnabled();
    batch.setResourceTexture(0, nullptr);
}

void WebEntityRenderer::buildWebSurface(const EntityItemPointer& entity, const QString& newSourceURL) {
    if (_currentWebCount >= MAX_CONCURRENT_WEB_VIEWS) {
        qWarning() << "Too many concurrent web views to create new view";
        return;
    }

    bool isHTML = _contentType == ContentType::HtmlContent;
    if (isHTML) {
        ++_currentWebCount;
    }
    WebEntityRenderer::acquireWebSurface(newSourceURL, isHTML, _webSurface, _cachedWebSurface);
    _fadeStartTime = usecTimestampNow();
    _webSurface->resume();

    _connections.push_back(QObject::connect(this, &WebEntityRenderer::scriptEventReceived, _webSurface.data(), &OffscreenQmlSurface::emitScriptEvent));
    _connections.push_back(QObject::connect(_webSurface.data(), &OffscreenQmlSurface::webEventReceived, this, &WebEntityRenderer::webEventReceived));
    const EntityItemID entityItemID = entity->getID();
    _connections.push_back(QObject::connect(_webSurface.data(), &OffscreenQmlSurface::webEventReceived, this, [entityItemID](const QVariant& message) {
        emit DependencyManager::get<EntityScriptingInterface>()->webEventReceived(entityItemID, message);
    }));

    _tryingToBuildURL = newSourceURL;
}

void WebEntityRenderer::destroyWebSurface() {
    QSharedPointer<OffscreenQmlSurface> webSurface;
    withWriteLock([&] {
        webSurface.swap(_webSurface);

        if (webSurface) {
            if (_contentType == ContentType::HtmlContent) {
                --_currentWebCount;
            }
            WebEntityRenderer::releaseWebSurface(webSurface, _cachedWebSurface, _connections);
        }

        _contentType = ContentType::NoContent;
    });
}

glm::vec2 WebEntityRenderer::getWindowSize(const TypedEntityPointer& entity) const {
    glm::vec2 dims = glm::vec2(entity->getScaledDimensions());
    dims *= METERS_TO_INCHES * _dpi;

    // ensure no side is never larger then MAX_WINDOW_SIZE
    float max = (dims.x > dims.y) ? dims.x : dims.y;
    if (max > MAX_WINDOW_SIZE) {
        dims *= MAX_WINDOW_SIZE / max;
    }

    return dims;
}

void WebEntityRenderer::hoverEnterEntity(const PointerEvent& event) {
    if (_inputMode == WebInputMode::MOUSE) {
        handlePointerEvent(event);
        return;
    }

    withReadLock([&] {
        if (_webSurface) {
            PointerEvent webEvent = event;
            webEvent.setPos2D(event.getPos2D() * (METERS_TO_INCHES * _dpi));
            _webSurface->hoverBeginEvent(webEvent, _touchDevice);
        }
    });
}

void WebEntityRenderer::hoverLeaveEntity(const PointerEvent& event) {
    if (_inputMode == WebInputMode::MOUSE) {
        PointerEvent endEvent(PointerEvent::Release, event.getID(), event.getPos2D(), event.getPos3D(), event.getNormal(), event.getDirection(),
            event.getButton(), event.getButtons(), event.getKeyboardModifiers());
        handlePointerEvent(endEvent);
        // QML onReleased is only triggered if a click has happened first.  We need to send this "fake" mouse move event to properly trigger an onExited.
        PointerEvent endMoveEvent(PointerEvent::Move, event.getID());
        handlePointerEvent(endMoveEvent);
        return;
    }

    withReadLock([&] {
        if (_webSurface) {
            PointerEvent webEvent = event;
            webEvent.setPos2D(event.getPos2D() * (METERS_TO_INCHES * _dpi));
            _webSurface->hoverEndEvent(webEvent, _touchDevice);
        }
    });
}

void WebEntityRenderer::handlePointerEvent(const PointerEvent& event) {
    withReadLock([&] {
        if (!_webSurface) {
            return;
        }

        if (_inputMode == WebInputMode::TOUCH) {
            handlePointerEventAsTouch(event);
        } else {
            handlePointerEventAsMouse(event);
        }
    });
}

void WebEntityRenderer::handlePointerEventAsTouch(const PointerEvent& event) {
    PointerEvent webEvent = event;
    webEvent.setPos2D(event.getPos2D() * (METERS_TO_INCHES * _dpi));
    _webSurface->handlePointerEvent(webEvent, _touchDevice);
}

void WebEntityRenderer::handlePointerEventAsMouse(const PointerEvent& event) {
    glm::vec2 windowPos = event.getPos2D() * (METERS_TO_INCHES * _dpi);
    QPointF windowPoint(windowPos.x, windowPos.y);

    Qt::MouseButtons buttons = Qt::NoButton;
    if (event.getButtons() & PointerEvent::PrimaryButton) {
        buttons |= Qt::LeftButton;
    }

    Qt::MouseButton button = Qt::NoButton;
    if (event.getButton() == PointerEvent::PrimaryButton) {
        button = Qt::LeftButton;
    }

    QEvent::Type type;
    switch (event.getType()) {
        case PointerEvent::Press:
            type = QEvent::MouseButtonPress;
            break;
        case PointerEvent::Release:
            type = QEvent::MouseButtonRelease;
            break;
        case PointerEvent::Move:
            type = QEvent::MouseMove;
            break;
        default:
            return;
    }

    QMouseEvent mouseEvent(type, windowPoint, windowPoint, windowPoint, button, buttons, event.getKeyboardModifiers());
    QCoreApplication::sendEvent(_webSurface->getWindow(), &mouseEvent);
}

void WebEntityRenderer::setProxyWindow(QWindow* proxyWindow) {
    withReadLock([&] {
        if (_webSurface) {
            _webSurface->setProxyWindow(proxyWindow);
        }
    });
}

QObject* WebEntityRenderer::getEventHandler() {
    return resultWithReadLock<QObject*>([&]() -> QObject* {
        if (!_webSurface) {
            return nullptr;
        }
        return _webSurface->getEventHandler();
    });
}

void WebEntityRenderer::emitScriptEvent(const QVariant& message) {
    emit scriptEventReceived(message);
}
