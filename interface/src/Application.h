//
//  Application.h
//  interface/src
//
//  Created by Andrzej Kapolka on 5/10/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Application_h
#define hifi_Application_h

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtQuick/QQuickItem>
#include <QtCore/QSharedPointer>

#include <QtGui/QImage>

#include <QtWidgets/QApplication>

#include <AvatarHashMap.h>
#include <ThreadHelpers.h>
#include <AbstractScriptingServicesInterface.h>
#include <AbstractViewStateInterface.h>
#include <EntityEditPacketSender.h>
#include <EntityTreeRenderer.h>
#include <FileScriptingInterface.h>
#include <input-plugins/KeyboardMouseDevice.h>
#include <input-plugins/TouchscreenDevice.h>
#include <input-plugins/TouchscreenVirtualPadDevice.h>
#include <OctreeQuery.h>
#include <PhysicalEntitySimulation.h>
#include <PhysicsEngine.h>
#include <plugins/Forward.h>
#include <ui-plugins/PluginContainer.h>
#include <ShapeManager.h>
#include <SimpleMovingAverage.h>
#include <StDev.h>
#include <ViewFrustum.h>
#include <AbstractUriHandler.h>
#include <shared/RateCounter.h>
#include <ThreadSafeValueCache.h>
#include <shared/ConicalViewFrustum.h>
#include <shared/FileLogger.h>
#include <RunningMarker.h>
#include <ModerationFlags.h>
#include <OffscreenUi.h>

#include "avatar/MyAvatar.h"
#include "FancyCamera.h"
#include "ConnectionMonitor.h"
#include "CursorManager.h"
#include "gpu/Context.h"
#include "LoginStateManager.h"
#include "Menu.h"
#include "PerformanceManager.h"
#include "RefreshRateManager.h"
#include "octree/OctreePacketProcessor.h"
#include "render/Engine.h"
#include "scripting/ControllerScriptingInterface.h"
#include "scripting/DialogsManagerScriptingInterface.h"
#include "ui/ApplicationOverlay.h"
#include "ui/EntityScriptServerLogDialog.h"
#include "ui/LodToolsDialog.h"
#include "ui/LogDialog.h"
#include "ui/OctreeStatsDialog.h"
#include "ui/OverlayConductor.h"
#include "ui/overlays/Overlays.h"

#include "workload/GameWorkload.h"
#include "graphics/GraphicsEngine.h"

#include <graphics/Skybox.h>
#include <ModelScriptingInterface.h>

#include "Sound.h"
#include "VisionSqueeze.h"

class GLCanvas;
class MainWindow;
class AssetUpload;
class CompositorHelper;
class AudioInjector;
class ScriptEngine;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;

namespace controller {
    class StateController;
}


static const QString RUNNING_MARKER_FILENAME = "Interface.running";
static const QString SCRIPTS_SWITCH = "scripts";
static const QString HIFI_NO_LOGIN_COMMAND_LINE_KEY = "no-login-suggestion";

class Application;
#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application*>(QCoreApplication::instance()))

class Application : public QApplication,
                    public AbstractViewStateInterface,
                    public AbstractScriptingServicesInterface,
                    public AbstractUriHandler,
                    public PluginContainer
{
    Q_OBJECT

    // TODO? Get rid of those
    friend class OctreePacketProcessor;

public:
    // virtual functions required for PluginContainer
    virtual ui::Menu* getPrimaryMenu() override;
    virtual void requestReset() override { resetSensors(false); }
    virtual void showDisplayPluginsTools(bool show) override;
    virtual GLWidget* getPrimaryWidget() override;
    virtual MainWindow* getPrimaryWindow() override;
    virtual QOpenGLContext* getPrimaryContext() override;
    virtual bool makeRenderingContextCurrent() override;
    virtual bool isForeground() const override;

    virtual DisplayPluginPointer getActiveDisplayPlugin() const override;

    // FIXME? Empty methods, do we still need them?
    static void initPlugins(const QStringList& arguments);
    static void shutdownPlugins();

    Application(int& argc, char** argv, QElapsedTimer& startup_time, bool runningMarkerExisted);
    ~Application();

    void postLambdaEvent(const std::function<void()>& f) override;
    void sendLambdaEvent(const std::function<void()>& f) override;

    QString getPreviousScriptLocation();
    void setPreviousScriptLocation(const QString& previousScriptLocation);

    // Return an HTTP User-Agent string with OS and device information.
    Q_INVOKABLE QString getUserAgent();

    void initializeGL();
    void initializeDisplayPlugins();
    void initializeRenderEngine();
    void initializeUi();

    void updateSecondaryCameraViewFrustum();

    void updateCamera(RenderArgs& renderArgs, float deltaTime);
    void resizeGL();

    bool notify(QObject *, QEvent *) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

    glm::uvec2 getCanvasSize() const;
    QRect getRenderingGeometry() const;

    glm::uvec2 getUiSize() const;
    QRect getRecommendedHUDRect() const;
    glm::vec2 getDeviceSize() const;
    bool hasFocus() const;
    void setFocus();
    void raise();

    void showCursor(const Cursor::Icon& cursor);

    bool isThrottleRendering() const;

    Camera& getCamera() { return _myCamera; }
    const Camera& getCamera() const { return _myCamera; }
    // Represents the current view frustum of the avatar.
    void copyViewFrustum(ViewFrustum& viewOut) const;
    // Represents the view frustum of the current rendering pass,
    // which might be different from the viewFrustum, i.e. shadowmap
    // passes, mirror window passes, etc
    void copyDisplayViewFrustum(ViewFrustum& viewOut) const;

    bool isMissingSequenceNumbers() { return _isMissingSequenceNumbers; }

    const ConicalViewFrustums& getConicalViews() const override { return _conicalViews; }

    const OctreePacketProcessor& getOctreePacketProcessor() const { return _octreeProcessor; }
    QSharedPointer<EntityTreeRenderer> getEntities() const { return DependencyManager::get<EntityTreeRenderer>(); }
    MainWindow* getWindow() const { return _window; }
    EntityTreePointer getEntityClipboard() const { return _entityClipboard; }
    EntityEditPacketSender* getEntityEditPacketSender() { return &_entityEditSender; }

    ivec2 getMouse() const;

    ApplicationOverlay& getApplicationOverlay() { return _applicationOverlay; }
    const ApplicationOverlay& getApplicationOverlay() const { return _applicationOverlay; }
    CompositorHelper& getApplicationCompositor() const;

    Overlays& getOverlays() { return _overlays; }

    PerformanceManager& getPerformanceManager() { return _performanceManager; }
    RefreshRateManager& getRefreshRateManager() { return _refreshRateManager; }

    size_t getRenderFrameCount() const { return _graphicsEngine.getRenderFrameCount(); }
    float getRenderLoopRate() const { return _graphicsEngine.getRenderLoopRate(); }
    float getNumCollisionObjects() const;
    float getTargetRenderFrameRate() const; // frames/second

    static void setupQmlSurface(QQmlContext* surfaceContext, bool setAdditionalContextProperties);

    float getFieldOfView() { return _fieldOfView.get(); }
    void setFieldOfView(float fov);

    float getHMDTabletScale() { return _hmdTabletScale.get(); }
    void setHMDTabletScale(float hmdTabletScale);
    float getDesktopTabletScale() { return _desktopTabletScale.get(); }
    void setDesktopTabletScale(float desktopTabletScale);

    bool getDesktopTabletBecomesToolbarSetting() { return _desktopTabletBecomesToolbarSetting.get(); }
    bool getLogWindowOnTopSetting() { return _keepLogWindowOnTop.get(); }
    void setLogWindowOnTopSetting(bool keepOnTop) { _keepLogWindowOnTop.set(keepOnTop); }
    void setDesktopTabletBecomesToolbarSetting(bool value);
    bool getHmdTabletBecomesToolbarSetting() { return _hmdTabletBecomesToolbarSetting.get(); }
    void setHmdTabletBecomesToolbarSetting(bool value);
    bool getPreferStylusOverLaser() { return _preferStylusOverLaserSetting.get(); }
    void setPreferStylusOverLaser(bool value);

    bool getPreferAvatarFingerOverStylus() { return _preferAvatarFingerOverStylusSetting.get(); }
    void setPreferAvatarFingerOverStylus(bool value);

    bool getMiniTabletEnabled() { return _miniTabletEnabledSetting.get(); }
    void setMiniTabletEnabled(bool enabled);

    float getSettingConstrainToolbarPosition() { return _constrainToolbarPosition.get(); }
    void setSettingConstrainToolbarPosition(bool setting);

    float getAwayStateWhenFocusLostInVREnabled() { return _awayStateWhenFocusLostInVREnabled.get(); }
    void setAwayStateWhenFocusLostInVREnabled(bool setting);

    Q_INVOKABLE void setMinimumGPUTextureMemStabilityCount(int stabilityCount) { _minimumGPUTextureMemSizeStabilityCount = stabilityCount; }

    NodeToOctreeSceneStats* getOcteeSceneStats() { return &_octreeServerSceneStats; }

    virtual controller::ScriptingInterface* getControllerScriptingInterface() { return _controllerScriptingInterface; }
    virtual void registerScriptEngineWithApplicationServices(const ScriptManagerPointer& scriptManager) override;

    virtual void copyCurrentViewFrustum(ViewFrustum& viewOut) const override { copyDisplayViewFrustum(viewOut); }
    virtual QThread* getMainThread() override { return thread(); }
    virtual PickRay computePickRay(float x, float y) const override;
    virtual glm::vec3 getAvatarPosition() const override;
    virtual qreal getDevicePixelRatio() override;

    void setActiveDisplayPlugin(const QString& pluginName);

#ifndef Q_OS_ANDROID
    FileLogger* getLogger() const { return _logger; }
#endif

    float getRenderResolutionScale() const;

    qint64 getCurrentSessionRuntime() const { return _sessionRunTimer.elapsed(); }

    bool isAboutToQuit() const { return _aboutToQuit; }
    bool isPhysicsEnabled() const { return _physicsEnabled; }
    PhysicsEnginePointer getPhysicsEngine() { return _physicsEngine; }

    // the isHMDMode is true whenever we use the interface from an HMD and not a standard flat display
    // rendering of several elements depend on that
    // TODO: carry that information on the Camera as a setting
    virtual bool isHMDMode() const override;
    glm::mat4 getHMDSensorPose() const;
    glm::mat4 getEyeOffset(int eye) const;
    glm::mat4 getEyeProjection(int eye) const;

    QRect getDesirableApplicationGeometry() const;

    virtual bool canAcceptURL(const QString& url) const override;
    virtual bool acceptURL(const QString& url, bool defaultUpload = false) override;

    void setMaxOctreePacketsPerSecond(int maxOctreePPS);
    int getMaxOctreePacketsPerSecond() const;

    render::ScenePointer getMain3DScene() override { return _graphicsEngine.getRenderScene(); }
    render::EnginePointer getRenderEngine() override { return  _graphicsEngine.getRenderEngine(); }
    gpu::ContextPointer getGPUContext() const { return _graphicsEngine.getGPUContext(); }

    const GameWorkload& getGameWorkload() const { return _gameWorkload; }

    virtual void pushPostUpdateLambda(void* key, const std::function<void()>& func) override;

    void updateMyAvatarLookAtPosition(float deltaTime);

    float getGameLoopRate() const { return _gameLoopCounter.rate(); }

    void takeSnapshot(bool notify, bool includeAnimated = false, float aspectRatio = 0.0f, const QString& filename = QString());
    void takeSecondaryCameraSnapshot(const bool& notify, const QString& filename = QString());
    void takeSecondaryCamera360Snapshot(const glm::vec3& cameraPosition,
                                        const bool& cubemapOutputFormat,
                                        const bool& notify,
                                        const QString& filename = QString());

    void shareSnapshot(const QString& filename, const QUrl& href = QUrl(""));

    QUuid getTabletScreenID() const;
    QUuid getTabletHomeButtonID() const;
    QUuid getTabletFrameID() const;
    QVector<QUuid> getTabletIDs() const;

    void setAvatarOverrideUrl(const QUrl& url, bool save);
    void clearAvatarOverrideUrl() { _avatarOverrideUrl = QUrl(); _saveAvatarOverrideUrl = false; }
    QUrl getAvatarOverrideUrl() { return _avatarOverrideUrl; }
    bool getSaveAvatarOverrideUrl() { return _saveAvatarOverrideUrl; }
    void saveNextPhysicsStats(QString filename);

    bool isServerlessMode() const;
    bool isInterstitialMode() const { return _interstitialMode; }
    bool failedToConnectToEntityServer() const { return _failedToConnectToEntityServer; }

    void replaceDomainContent(const QString& url, const QString& itemName);

    void loadAvatarScripts(const QVector<QString>& urls);
    void unloadAvatarScripts();

    Q_INVOKABLE void copyToClipboard(const QString& text);

    int getOtherAvatarsReplicaCount() { return DependencyManager::get<AvatarHashMap>()->getReplicaCount(); }
    void setOtherAvatarsReplicaCount(int count) { DependencyManager::get<AvatarHashMap>()->setReplicaCount(count); }

    void confirmConnectWithoutAvatarEntities();

    bool getLoginDialogPoppedUp() const { return _loginDialogPoppedUp; }
    void createLoginDialog();
    void updateLoginDialogPosition();

    void createAvatarInputsBar();
    void destroyAvatarInputsBar();

    // Check if a headset is connected
    bool hasRiftControllers();
    bool hasViveControllers();

#if defined(Q_OS_ANDROID)
    void beforeEnterBackground();
    void enterBackground();
    void enterForeground();
    void toggleAwayMode();
    #endif

    using SnapshotOperator = std::tuple<std::function<void(const QImage&)>, float, bool>;
    void addSnapshotOperator(const SnapshotOperator& snapshotOperator);
    bool takeSnapshotOperators(std::queue<SnapshotOperator>& snapshotOperators);

    void openDirectory(const QString& path);

    void overrideEntry();
    void forceDisplayName(const QString& displayName);
    void forceLoginWithTokens(const QString& tokens);
    void setConfigFileURL(const QString& fileUrl);

    // used by preferences and HMDScriptingInterface...
    VisionSqueeze& getVisionSqueeze() { return _visionSqueeze; }

signals:
    void svoImportRequested(const QString& url);

    void fullAvatarURLChanged(const QString& newValue, const QString& modelName);

    void beforeAboutToQuit();
    void activeDisplayPluginChanged();

    void uploadRequest(QString path);

    void interstitialModeChanged(bool isInInterstitialMode);

    void loginDialogFocusEnabled();
    void loginDialogFocusDisabled();

    void miniTabletEnabledChanged(bool enabled);
    void awayStateWhenFocusLostInVRChanged(bool enabled);

public slots:
    QVector<EntityItemID> pasteEntities(const QString& entityHostType, float x, float y, float z);
    bool exportEntities(const QString& filename, const QVector<QUuid>& entityIDs, const glm::vec3* givenOffset = nullptr);
    bool exportEntities(const QString& filename, float x, float y, float z, float scale);
    bool importEntities(const QString& url, const bool isObservable = true, const qint64 callerId = -1);
    void updateThreadPoolCount() const;
    void updateSystemTabletMode();
    void goToErrorDomainURL(QUrl errorDomainURL);

    Q_INVOKABLE void loadDialog();
    Q_INVOKABLE void loadScriptURLDialog() const;
    void toggleLogDialog();
    void recreateLogWindow(int);
    void toggleEntityScriptServerLogDialog();
    Q_INVOKABLE void showAssetServerWidget(QString filePath = "");
    Q_INVOKABLE void loadAddAvatarBookmarkDialog() const;
    Q_INVOKABLE void loadAvatarBrowser() const;
    Q_INVOKABLE SharedSoundPointer getSampleSound() const;

    void showDialog(const QUrl& widgetUrl, const QUrl& tabletUrl, const QString& name) const;

    void showLoginScreen();

    // FIXME: Move addAssetToWorld* methods to own class?
    void addAssetToWorldFromURL(QString url);
    void addAssetToWorldFromURLRequestFinished();
    void addAssetToWorld(QString filePath, QString zipFile, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldUnzipFailure(QString filePath);
    void addAssetToWorldWithNewMapping(QString filePath, QString mapping, int copy, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldUpload(QString filePath, QString mapping, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldSetMapping(QString filePath, QString mapping, QString hash, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldAddEntity(QString filePath, QString mapping);

    void handleUnzip(QString sourceFile, QStringList destinationFile, bool autoAdd, bool isZip, bool isBlocks);

    FileScriptingInterface* getFileDownloadInterface() { return _fileDownload; }

    void handleLocalServerConnection() const;
    void readArgumentsFromLocalSocket() const;

    static void packageModel();

    void resetSensors(bool andReload = false);

    void hmdVisibleChanged(bool visible);

#if (PR_BUILD || DEV_BUILD)
    void sendWrongProtocolVersionsSignature(bool checked) { ::sendWrongProtocolVersionsSignature(checked); }
#endif

    static void showHelp();
    static void gotoTutorial();

    void cycleCamera();
    void cameraModeChanged();
    void cameraMenuChanged();
    void captureMouseChanged(bool captureMouse);
    void toggleOverlays();
    void setOverlaysVisible(bool visible);
    Q_INVOKABLE void centerUI();

    void resetPhysicsReadyInformation();

    void reloadResourceCaches();

    void updateHeartbeat() const;

    static void deadlockApplication();
    static void unresponsiveApplication(); // cause main thread to be unresponsive for 35 seconds

    void rotationModeChanged() const;

    static void runTests();

    void setKeyboardFocusHighlight(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& dimensions);

    QUuid getKeyboardFocusEntity() const;  // thread-safe
    void setKeyboardFocusEntity(const QUuid& id);

    void addAssetToWorldMessageClose();

    void loadLODToolsDialog();
    void loadEntityStatisticsDialog();
    void loadDomainConnectionDialog();
    void showScriptLogs();

    const QString getPreferredCursor() const { return _preferredCursor.get(); }
    void setPreferredCursor(const QString& cursor);

    void setIsServerlessMode(bool serverlessDomain);
    std::map<QString, QString> prepareServerlessDomainContents(QUrl domainURL, QByteArray data);

    void loadServerlessDomain(QUrl domainURL);
    void loadErrorDomain(QUrl domainURL);
    void setIsInterstitialMode(bool interstitialMode);

    void updateVerboseLogging();
    
    void setCachebustRequire();

    void changeViewAsNeeded(float boomLength);

    QString getGraphicsCardType();

    bool gpuTextureMemSizeStable();
    void showUrlHandler(const QUrl& url);

    // used to test "shutdown" crash annotation.
    void crashOnShutdown();

private slots:
    void onDesktopRootItemCreated(QQuickItem* qmlContext);
    void onDesktopRootContextCreated(QQmlContext* qmlContext);
    void showDesktop();
    void clearDomainOctreeDetails(bool clearAll = true);
    void onAboutToQuit();
    void onPresent(quint32 frameCount);

    void resettingDomain();

    void activeChanged(Qt::ApplicationState state);
    void windowMinimizedChanged(bool minimized);

    void notifyPacketVersionMismatch();

    void loadSettings();
    void saveSettings() const;
    void setFailedToConnectToEntityServer();

    bool acceptSnapshot(const QString& urlString);
    bool askToSetAvatarUrl(const QString& url);
    bool askToLoadScript(const QString& scriptFilenameOrURL);

    bool askToWearAvatarAttachmentUrl(const QString& url);
    void displayAvatarAttachmentWarning(const QString& message) const;

    bool askToReplaceDomainContent(const QString& url);

    void setSessionUUID(const QUuid& sessionUUID) const;

    void domainURLChanged(QUrl domainURL);
    void updateWindowTitle() const;
    void nodeAdded(SharedNodePointer node);
    void nodeActivated(SharedNodePointer node);
    void nodeKilled(SharedNodePointer node);
    static void packetSent(quint64 length);
    static void addingEntityWithCertificate(const QString& certificateID, const QString& placeName);
    void updateDisplayMode();
    void setDisplayPlugin(DisplayPluginPointer newPlugin);
    void domainConnectionRefused(const QString& reasonMessage, int reason, const QString& extraInfo);

    void addAssetToWorldCheckModelSize();

    void onAssetToWorldMessageBoxClosed();
    void addAssetToWorldInfoTimeout();
    void addAssetToWorldErrorTimeout();

    void handleSandboxStatus(QNetworkReply* reply);
    void switchDisplayMode();

    void setShowBulletWireframe(bool value);
    void setShowBulletAABBs(bool value);
    void setShowBulletContactPoints(bool value);
    void setShowBulletConstraints(bool value);
    void setShowBulletConstraintLimits(bool value);

    void onDismissedLoginDialog();

    void setShowTrackedObjects(bool value);

private:
    void init();
    void pauseUntilLoginDetermined();
    void resumeAfterLoginDialogActionTaken();
    bool handleKeyEventForFocusedEntity(QEvent* event);
    bool handleFileOpenEvent(QFileOpenEvent* event);
    void cleanupBeforeQuit();

    void idle();
    void tryToEnablePhysics();
    void update(float deltaTime);

    // Various helper functions called during update()
    void updateLOD(float deltaTime) const;
    void updateThreads(float deltaTime);
    void updateDialogs(float deltaTime) const;

    void queryOctree(NodeType_t serverType, PacketType packetType);
    void queryAvatars();

    int sendNackPackets();

    std::shared_ptr<MyAvatar> getMyAvatar() const;

    void checkSkeleton() const;

    void initializeAcceptedFiles();

    bool importJSONFromURL(const QString& urlString);
    bool importSVOFromURL(const QString& urlString);
    bool importFromZIP(const QString& filePath);
    bool importImage(const QString& urlString);

    int processOctreeStats(ReceivedMessage& message, SharedNodePointer sendingNode);
    void trackIncomingOctreePacket(ReceivedMessage& message, SharedNodePointer sendingNode, bool wasStatsPacket);

    void resizeEvent(QResizeEvent* size);

    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);

    void focusOutEvent(QFocusEvent* event);
    void synthesizeKeyReleasEvents();
    void focusInEvent(QFocusEvent* event);

    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseDoublePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);

    void touchBeginEvent(QTouchEvent* event);
    void touchEndEvent(QTouchEvent* event);
    void touchUpdateEvent(QTouchEvent* event);
    void touchGestureEvent(QGestureEvent* event);

    void wheelEvent(QWheelEvent* event) const;
    void dropEvent(QDropEvent* event);
    static void dragEnterEvent(QDragEnterEvent* event);

    void maybeToggleMenuVisible(QMouseEvent* event) const;
    void toggleTabletUI(bool shouldOpen = false) const;
    bool shouldCaptureMouse() const;

    void userKickConfirmation(const QUuid& nodeID, unsigned int banFlags = ModerationFlags::getDefaultBanFlags());

    MainWindow* _window;
    QElapsedTimer& _sessionRunTimer;

    bool _aboutToQuit { false };

#ifndef Q_OS_ANDROID
    FileLogger* _logger { nullptr };
#endif

    bool _previousSessionCrashed;

    DisplayPluginPointer _displayPlugin;
    QMetaObject::Connection _displayPluginPresentConnection;
    mutable std::mutex _displayPluginLock;
    InputPluginList _activeInputPlugins;

    bool _activatingDisplayPlugin { false };

    // Frame Rate Measurement
    RateCounter<500> _gameLoopCounter;

    QTimer _minimizedWindowTimer;
    QElapsedTimer _timerStart;
    QElapsedTimer _lastTimeUpdated;

    int _minimumGPUTextureMemSizeStabilityCount { 30 };

    ShapeManager _shapeManager;
    PhysicalEntitySimulationPointer _entitySimulation;
    PhysicsEnginePointer _physicsEngine;

    EntityTreePointer _entityClipboard;

    mutable QMutex _viewMutex { QMutex::Recursive };
    ViewFrustum _viewFrustum; // current state of view frustum, perspective, orientation, etc.
    ViewFrustum _displayViewFrustum;

    ConicalViewFrustums _conicalViews;
    ConicalViewFrustums _lastQueriedViews; // last views used to query servers

    using SteadyClock = std::chrono::steady_clock;
    using TimePoint = SteadyClock::time_point;
    TimePoint _queryExpiry;

    OctreeQuery _octreeQuery { true }; // NodeData derived class for querying octee cells from octree servers

    std::shared_ptr<controller::StateController> _applicationStateDevice; // Default ApplicationDevice reflecting the state of different properties of the session
    std::shared_ptr<KeyboardMouseDevice> _keyboardMouseDevice;   // Default input device, the good old keyboard mouse and maybe touchpad
    std::shared_ptr<TouchscreenDevice> _touchscreenDevice;   // the good old touchscreen
    std::shared_ptr<TouchscreenVirtualPadDevice> _touchscreenVirtualPadDevice;
    SimpleMovingAverage _avatarSimsPerSecond {10};
    int _avatarSimsPerSecondReport {0};
    quint64 _lastAvatarSimsPerSecondUpdate {0};
    FancyCamera _myCamera;                            // My view onto the world

    Setting::Handle<QString> _previousScriptLocation;
    Setting::Handle<float> _fieldOfView;
    Setting::Handle<float> _hmdTabletScale;
    Setting::Handle<float> _desktopTabletScale;
    Setting::Handle<bool> _firstRun;
    Setting::Handle<bool> _desktopTabletBecomesToolbarSetting;
    Setting::Handle<bool> _hmdTabletBecomesToolbarSetting;
    Setting::Handle<bool> _preferStylusOverLaserSetting;
    Setting::Handle<bool> _preferAvatarFingerOverStylusSetting;
    Setting::Handle<bool> _constrainToolbarPosition;
    Setting::Handle<bool> _awayStateWhenFocusLostInVREnabled;
    Setting::Handle<QString> _preferredCursor;
    Setting::Handle<bool> _miniTabletEnabledSetting;
    Setting::Handle<bool> _keepLogWindowOnTop { "keepLogWindowOnTop", false };

    float _scaleMirror;
    float _mirrorYawOffset;
    float _raiseMirror;

    QHash<int, QKeyEvent> _keysPressed;

    bool _enableProcessOctreeThread;
    bool _interstitialMode { false };

    OctreePacketProcessor _octreeProcessor;
    EntityEditPacketSender _entityEditSender;

    StDev _idleLoopStdev;
    float _idleLoopMeasuredJitter;

    NodeToOctreeSceneStats _octreeServerSceneStats;
    ControllerScriptingInterface* _controllerScriptingInterface{ nullptr };
    QPointer<LogDialog> _logDialog;
    QPointer<EntityScriptServerLogDialog> _entityScriptServerLogDialog;
    QDir _defaultScriptsLocation;

    TouchEvent _lastTouchEvent;

    quint64 _lastNackTime;
    quint64 _lastSendDownstreamAudioStats;

    bool _notifiedPacketVersionMismatchThisDomain;

    ConditionalGuard _settingsGuard;

    GLCanvas* _glWidget{ nullptr };

    typedef bool (Application::* AcceptURLMethod)(const QString &);
    static const std::vector<std::pair<QString, Application::AcceptURLMethod>> _acceptedExtensions;

    glm::uvec2 _renderResolution;

    int _maxOctreePPS = DEFAULT_MAX_OCTREE_PPS;
    bool _interstitialModeEnabled{ false };

    bool _loginDialogPoppedUp{ false };
    bool _desktopRootItemCreated{ false };

    ModalDialogListener* _confirmConnectWithoutAvatarEntitiesDialog { nullptr };

    bool _developerMenuVisible{ false };
    QString _previousAvatarSkeletonModel;
    float _previousAvatarTargetScale;
    CameraMode _previousCameraMode;
    QUuid _loginDialogID;
    QUuid _avatarInputsBarID;
    LoginStateManager _loginStateManager;
    PerformanceManager _performanceManager;
    RefreshRateManager _refreshRateManager;

    GameWorkload _gameWorkload;

    GraphicsEngine _graphicsEngine;
    void updateRenderArgs(float deltaTime);

    bool _disableLoginScreen { true };

    Overlays _overlays;
    ApplicationOverlay _applicationOverlay;
    OverlayConductor _overlayConductor;

    DialogsManagerScriptingInterface* _dialogsManagerScriptingInterface = new DialogsManagerScriptingInterface();

    ThreadSafeValueCache<EntityItemID> _keyboardFocusedEntity;
    quint64 _lastAcceptedKeyPress = 0;
    bool _isForeground = true; // starts out assumed to be in foreground
    bool _isGLInitialized { false };
    bool _physicsEnabled { false };
    bool _failedToConnectToEntityServer { false };

    bool _reticleClickPressed { false };
    bool _keyboardFocusWaitingOnRenderable { false };

    int _avatarAttachmentRequest = 0;

    bool _settingsLoaded { false };

    bool _captureMouse { false };
    bool _ignoreMouseMove { false };
    QPointF _mouseCaptureTarget { NAN, NAN };

    bool _isMissingSequenceNumbers { false };

    void checkChangeCursor();
    mutable QMutex _changeCursorLock { QMutex::Recursive };
    Qt::CursorShape _desiredCursor{ Qt::BlankCursor };
    bool _cursorNeedsChanging { false };

    std::map<void*, std::function<void()>> _postUpdateLambdas;
    std::mutex _postUpdateLambdasLock;

    std::atomic<uint32_t> _fullSceneReceivedCounter { 0 }; // how many times have we received a full-scene octree stats packet
    uint32_t _fullSceneCounterAtLastPhysicsCheck { 0 }; // _fullSceneReceivedCounter last time we checked physics ready

    qint64 _gpuTextureMemSizeStabilityCount { 0 };
    qint64 _gpuTextureMemSizeAtLastCheck { 0 };

    bool _keyboardDeviceHasFocus { true };

    ConnectionMonitor _connectionMonitor;

    QTimer _addAssetToWorldResizeTimer;
    QHash<QUuid, int> _addAssetToWorldResizeList;

    void addAssetToWorldInfo(QString modelName, QString infoText);
    void addAssetToWorldInfoClear(QString modelName);
    void addAssetToWorldInfoDone(QString modelName);
    void addAssetToWorldError(QString modelName, QString errorText);

    QQuickItem* _addAssetToWorldMessageBox{ nullptr };
    QStringList _addAssetToWorldInfoKeys;  // Model name
    QStringList _addAssetToWorldInfoMessages;  // Info message
    QTimer _addAssetToWorldInfoTimer;
    QTimer _addAssetToWorldErrorTimer;
    mutable QTimer _entityServerConnectionTimer;

    FileScriptingInterface* _fileDownload;
    AudioInjectorPointer _snapshotSoundInjector;
    SharedSoundPointer _snapshotSound;
    SharedSoundPointer _sampleSound;
    std::mutex _snapshotMutex;
    std::queue<SnapshotOperator> _snapshotOperators;
    bool _hasPrimarySnapshot { false };

    DisplayPluginPointer _autoSwitchDisplayModeSupportedHMDPlugin { nullptr };
    QString _autoSwitchDisplayModeSupportedHMDPluginName;
    bool _previousHMDWornStatus;
    void startHMDStandBySession();
    void endHMDSession();

    glm::vec3 _thirdPersonHMDCameraBoom { 0.0f, 0.0f, -1.0f };
    bool _thirdPersonHMDCameraBoomValid { true };

    QUrl _avatarOverrideUrl;
    bool _saveAvatarOverrideUrl { false };

    std::atomic<bool> _pendingIdleEvent { true };

    bool quitWhenFinished { false };

    bool _showTrackedObjects { false };
    bool _prevShowTrackedObjects { false };

    bool _resumeAfterLoginDialogActionTaken_WasPostponed { false };
    bool _resumeAfterLoginDialogActionTaken_SafeToRun { false };
    bool _startUpFinished { false };
    bool _overrideEntry { false };

    VisionSqueeze _visionSqueeze;

    bool _crashOnShutdown { false };
};
#endif // hifi_Application_h
