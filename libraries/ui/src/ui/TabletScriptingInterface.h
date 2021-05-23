//
//  Created by Anthony J. Thibault on 2016-12-12
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TabletScriptingInterface_h
#define hifi_TabletScriptingInterface_h

#include <mutex>
#include <atomic>

#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QAbstractListModel>
#include <QSortFilterProxyModel>

#include <QtQuick/QQuickItem>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "SoundCache.h"
#include <DependencyManager.h>

class ToolbarProxy;
class ToolbarScriptingInterface;

class TabletProxy;
class TabletButtonProxy;
class QmlWindowClass;
class OffscreenQmlSurface;

/*@jsdoc
 * The <code>Tablet</code> API provides the facilities to work with the system or other tablet. In toolbar mode (see Developer 
 * &gt; UI options), the tablet's menu buttons are displayed in a toolbar and other tablet content is displayed in a dialog.
 *
 * <p>See also the {@link Toolbars} API for working with toolbars.</p>
 *
 * @namespace Tablet
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */
/*@jsdoc
 * The <code>tabletInterface</code> API provides the facilities to work with the system or other tablet.
 *
 * @namespace tabletInterface
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @deprecated This API is deprecated and will be removed. Use {@link Tablet} instead.
 *
 * @borrows Tablet.getTablet as getTablet
 * @borrows Tablet.playSound as playSound
 * @borrows Tablet.tabletNotification as tabletNotification
 */
class TabletScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
public:

    /*@jsdoc
     * <p>Standard tablet sounds.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>0</code></td><td>Button click.</td></tr>
     *     <tr><td><code>1</code></td><td>Button hover.</td></tr>
     *     <tr><td><code>2</code></td><td>Tablet open.</td></tr>
     *     <tr><td><code>3</code></td><td>Tablet hands in.</td></tr>
     *     <tr><td><code>4</code></td><td>Tablet hands out.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {number} Tablet.AudioEvents
     */
    enum TabletAudioEvents { ButtonClick, ButtonHover, TabletOpen, TabletHandsIn, TabletHandsOut, Last};
    Q_ENUM(TabletAudioEvents)

    //Different useful constants
    enum TabletConstants { ButtonsColumnsOnPage = 3, ButtonsRowsOnPage = 4, ButtonsOnPage = 12 };
    Q_ENUM(TabletConstants)

    TabletScriptingInterface();
    virtual ~TabletScriptingInterface();
    static const QString QML;

    void setToolbarScriptingInterface(ToolbarScriptingInterface* toolbarScriptingInterface) { _toolbarScriptingInterface = toolbarScriptingInterface; }

    /*@jsdoc
     * Gets an instance of a tablet. A new tablet is created if one with the specified name doesn't already exist.
     * @function Tablet.getTablet
     * @param {string} name - A unique name that identifies the tablet.
     * @returns {TabletProxy} The tablet instance.
     * @example <caption>Display the Vircadia home page on the system tablet.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * tablet.gotoWebScreen("https://vircadia.com/");
     */
    Q_INVOKABLE TabletProxy* getTablet(const QString& tabletId);

    void preloadSounds();

    /*@jsdoc
     * Plays a standard tablet sound. The sound is played locally (only the user running the script hears it) without a 
     * position.
     * @function Tablet.playSound
     * @param {Tablet.AudioEvents} sound - The tablet sound to play.
     * @example <caption>Play a tablet sound.</caption>
     * var TABLET_BUTTON_CLICK = 0;
     * Tablet.playSound(TABLET_BUTTON_CLICK);
     */
    Q_INVOKABLE void playSound(TabletAudioEvents aEvent);

    void setToolbarMode(bool toolbarMode);

    void setQmlTabletRoot(QString tabletId, OffscreenQmlSurface* offscreenQmlSurface);

    void processEvent(const QKeyEvent* event);

    QQuickWindow* getTabletWindow();

    QObject* getFlags();
signals:
    /*@jsdoc
     * Triggered when a tablet message or dialog is displayed on the tablet that needs the user's attention.
     * <p><strong>Note:</strong> Only triggered if the script is running in the same script engine as the script that created 
     * the tablet. By default, this means in scripts included as part of the default scripts.</p>
     * @function Tablet.tabletNotification
     * @returns {Signal}
     */
    void tabletNotification();

private:
    friend class TabletProxy;
    void processMenuEvents(QObject* object, const QKeyEvent* event);
    void processTabletEvents(QObject* object, const QKeyEvent* event);
    ToolbarProxy* getSystemToolbarProxy();

    QMap<TabletAudioEvents, SharedSoundPointer> _audioEvents;
protected:
    std::map<QString, TabletProxy*> _tabletProxies;
    ToolbarScriptingInterface* _toolbarScriptingInterface { nullptr };
    bool _toolbarMode { false };
};

/*@jsdoc
 * Information on the buttons in the tablet main menu (toolbar in toolbar mode) for use in QML. Has properties and functions 
 * per <a href="http://doc.qt.io/qt-5/qabstractlistmodel.html">http://doc.qt.io/qt-5/qabstractlistmodel.html</a>.
 * @typedef {object} TabletProxy.TabletButtonListModel
 */
class TabletButtonListModel : public QAbstractListModel {
    Q_OBJECT

public:
    TabletButtonListModel();
    ~TabletButtonListModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { Q_UNUSED(parent); return (int)_buttons.size(); }
    QHash<int, QByteArray> roleNames() const override { return _roles; }
    Qt::ItemFlags flags(const QModelIndex& index) const override { return _flags; }
    QVariant data(const QModelIndex& index, int role) const override;


protected:
    friend class TabletProxy;
    TabletButtonProxy* addButton(const QVariant& properties);
    void removeButton(TabletButtonProxy* button);
    int computeNewButtonIndex(const QVariantMap& newButtonProperties);
    using List = std::list<QSharedPointer<TabletButtonProxy>>;
    static QHash<int, QByteArray> _roles;
    static Qt::ItemFlags _flags;
    std::vector<QSharedPointer<TabletButtonProxy>> _buttons;
};

Q_DECLARE_METATYPE(TabletButtonListModel*);

class TabletButtonsProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
public:
    TabletButtonsProxyModel(QObject* parent = 0);
    int pageIndex() const;
    Q_INVOKABLE int buttonIndex(const QString& uuid);

public slots:
    void setPageIndex(int pageIndex);

signals:
    void pageIndexChanged(int pageIndex);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    int _pageIndex { -1 };
};

Q_DECLARE_METATYPE(TabletButtonsProxyModel*);

/*@jsdoc
 * An instance of a tablet. In toolbar mode (see Developer &gt; UI options), the tablet's menu buttons are displayed in a 
 * toolbar and other tablet content is displayed in a dialog.
 *
 * <p>Retrieve an existing tablet or create a new tablet using {@link Tablet.getTablet}.</p>
 *
 * @class TabletProxy
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {string} name - A unique name that identifies the tablet. <em>Read-only.</em>
 * @property {boolean} toolbarMode - <code>true</code> if the tablet is in toolbar mode, <code>false</code> if it isn't.
 * @property {boolean} landscape - <code>true</code> if the tablet is displayed in landscape mode, <code>false</code> if it is 
 *     displayed in portrait mode.
 *     <p>Note: This property isn't used in toolbar mode.</p>
 * @property {boolean} tabletShown - <code>true</code> if the tablet is currently displayed, <code>false</code> if it isn't.
 *     <p>Note: This property isn't used in toolbar mode.</p>
 * @property {TabletProxy.TabletButtonListModel} buttons - Information on the buttons in the tablet main menu (or toolbar in 
 *     toolbar mode) for use in QML. <em>Read-only.</em>
 */
class TabletProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ getName)
    Q_PROPERTY(bool toolbarMode READ getToolbarMode WRITE setToolbarMode NOTIFY toolbarModeChanged)
    Q_PROPERTY(bool landscape READ getLandscape WRITE setLandscape)
    Q_PROPERTY(bool tabletShown MEMBER _tabletShown NOTIFY tabletShownChanged)
    Q_PROPERTY(TabletButtonListModel* buttons READ getButtons CONSTANT)
public:
    TabletProxy(QObject* parent, const QString& name);
    ~TabletProxy();

    void setQmlTabletRoot(OffscreenQmlSurface* offscreenQmlSurface);

    const QString getName() const { return _name; }
    bool getToolbarMode() const { return _toolbarMode; }
    void setToolbarMode(bool toolbarMode);
    void unfocus();

    /*@jsdoc
     * Displays the tablet menu. The tablet is opened if it isn't already open.
     * @function TabletProxy#gotoMenuScreen
     * @param {string} [submenu=""] - The name of a submenu to display, if any.
     * @example <caption>Go to the "View" menu.</caption>
     * tablet.gotoMenuScreen("View");
     */
    Q_INVOKABLE void gotoMenuScreen(const QString& submenu = "");

    /*@jsdoc
     * @function TabletProxy#initialScreen
     * @param {string} url - URL.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void initialScreen(const QVariant& url);

    /*@jsdoc
     * Displays the tablet home screen, if the tablet is open.
     * @function TabletProxy#gotoHomeScreen
     */
    Q_INVOKABLE void gotoHomeScreen();

    /*@jsdoc
     * Opens a web app or page in addition to any current app. In tablet mode, the app or page is displayed over the top of the
     * current app; in toolbar mode, the app is opened in a new window that replaces any current window open. If in tablet
     * mode, the app or page can be closed using {@link TabletProxy#returnToPreviousApp}.
     * @function TabletProxy#gotoWebScreen
     * @param {string} url - The URL of the web page or app.
     * @param {string} [injectedJavaScriptUrl=""] - The URL of JavaScript to inject into the web page.
     * @param {boolean} [loadOtherBase=false] - If <code>true</code>, the web page or app is displayed in a frame with "back" 
     * and "close" buttons.
     * <p class="important">Deprecated: This parameter is deprecated and will be removed.</p>
     */
    Q_INVOKABLE void gotoWebScreen(const QString& url);
    Q_INVOKABLE void gotoWebScreen(const QString& url, const QString& injectedJavaScriptUrl, bool loadOtherBase = false);

    /*@jsdoc
     * Opens a QML app or dialog on the tablet.
     * @function TabletProxy#loadQMLSource
     * @param {string} path - The path of the QML app or dialog.
     * @param {boolean} [resizable=false] - <code>true</code> to make the dialog resizable in toolbar mode, <code>false</code> 
     *     to have it not resizable.
     */
    Q_INVOKABLE void loadQMLSource(const QVariant& path, bool resizable = false);

    /*@jsdoc
     * @function TabletProxy#loadQMLSourceImpl
     * @deprecated This function is deprecated and will be removed.
     */
    // Internal function, do not call from scripts.
    Q_INVOKABLE void loadQMLSourceImpl(const QVariant& path, bool resizable, bool localSafeContext);

    /*@jsdoc
     * @function TabletProxy#loadHTMLSourceOnTopImpl
     * @deprecated This function is deprecated and will be removed.
     */
    // Internal function, do not call from scripts.
    Q_INVOKABLE void loadHTMLSourceOnTopImpl(const QString& url, const QString& injectedJavaScriptUrl, bool loadOtherBase, bool localSafeContext);

    /*@jsdoc
     * @function TabletProxy#returnToPreviousAppImpl
     * @deprecated This function is deprecated and will be removed.
     */
    // Internal function, do not call from scripts.
    Q_INVOKABLE void returnToPreviousAppImpl(bool localSafeContext);

    /*@jsdoc
     * @function TabletProxy#loadQMLOnTopImpl
     * @deprecated This function is deprecated and will be removed.
     */
    // Internal function, do not call from scripts.
    Q_INVOKABLE void loadQMLOnTopImpl(const QVariant& path, bool localSafeContext);

    // FIXME: This currently relies on a script initializing the tablet (hence the bool denoting success);
    //        it should be initialized internally so it cannot fail

    /*@jsdoc
     * Displays a QML dialog over the top of the current dialog, without closing the current dialog. Use 
     * {@link TabletProxy#popFromStack|popFromStack} to close the dialog.
     * <p>If the current dialog or its ancestors contain a QML <code>StackView</code> with <code>objectName: "stack"</code> and 
     * function <code>pushSource(path)</code>, that function is called; otherwise, 
     * {@link TabletProxy#loadQMLSource|loadQMLSource} is called. The Create app provides an example of using a QML 
     * <code>StackView</code>.</p>
     * @function TabletProxy#pushOntoStack
     * @param {string} path - The path to the dialog's QML.
     * @returns {boolean} <code>true</code> if the dialog was successfully opened, <code>false</code> if it wasn't.
     */
    // edit.js provides an example of using this outside of main menu.
    Q_INVOKABLE bool pushOntoStack(const QVariant& path);

    /*@jsdoc
     * Closes a QML dialog that was displayed using {@link Tablet#pushOntoStack|pushOntoStack} with a dialog implementing a QML 
     * <code>StackView</code>; otherwise, no action is taken.
     * <p>If using a QML <code>StackView</code>, its <code>popSource()</code> function is called.</p>
     * @function TabletProxy#popFromStack
     */
    Q_INVOKABLE void popFromStack();

    /*@jsdoc
     * Opens a QML app or dialog in addition to any current app. In tablet mode, the app or dialog is displayed over the top of 
     * the current app; in toolbar mode, the app or dialog is opened in a new window. If in tablet mode, the app can be closed 
     * using {@link TabletProxy#returnToPreviousApp}.
     * @function TabletProxy#loadQMLOnTop
     * @param {string} path - The path to the app's QML.
     */
    Q_INVOKABLE void loadQMLOnTop(const QVariant& path);

    /*@jsdoc
     * Opens a web app or page in addition to any current app. In tablet mode, the app or page is displayed over the top of the
     * current app; in toolbar mode, the app is opened in a new window that replaces any current window open. If in tablet 
     * mode, the app or page can be closed using {@link TabletProxy#returnToPreviousApp}.
     * @function TabletProxy#loadWebScreenOnTop
     * @param {string} path - The URL of the web page or HTML app.
     * @param {string} [injectedJavaScriptURL=""] - The URL of JavaScript to inject into the web page.
     */
    Q_INVOKABLE void loadWebScreenOnTop(const QVariant& url);
    Q_INVOKABLE void loadWebScreenOnTop(const QVariant& url, const QString& injectedJavaScriptUrl);

    /*@jsdoc
     * Closes the current app and returns to the previous app, if in tablet mode and the current app was loaded using 
     * {@link TabletProxy#loadQMLOnTop|loadQMLOnTop} or {@link TabletProxy#loadWebScreenOnTop|loadWebScreenOnTop}.
     * @function TabletProxy#returnToPreviousApp
     */
    Q_INVOKABLE void returnToPreviousApp();

    /*@jsdoc
     * Checks if the tablet has a modal, non-modal, or message dialog open.
     * @function TabletProxy#isMessageDialogOpen
     * @returns {boolean} <code>true</code> if a modal, non-modal, or message dialog is open, <code>false</code> if there isn't.
     */
    Q_INVOKABLE bool isMessageDialogOpen();

    /*@jsdoc
     * Closes any open modal, non-modal, or message dialog, opened by {@link Window.prompt}, {@link Window.promptAsync}, 
     * {@link Window.openMessageBox}, or similar.
     * @function TabletProxy#closeDialog
     */
    Q_INVOKABLE void closeDialog();

    /*@jsdoc
     * Adds a new button to the tablet menu.
     * @function TabletProxy#addButton
     * @param {TabletButtonProxy.ButtonProperties} properties - Button properties.
     * @returns {TabletButtonProxy} The button added.
     * @example <caption>Add a menu button.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * var button = tablet.addButton({ text: "TEST" });
     * 
     * button.clicked.connect(function () {
     *     print("TEST button clicked");
     * });
     * 
     * Script.scriptEnding.connect(function () {
     *     tablet.removeButton(button);
     * });
     */
    //FIXME: UI_TABLET_HACK: enumerate the button properties when we figure out what they should be!
    Q_INVOKABLE TabletButtonProxy* addButton(const QVariant& properties);

    /*@jsdoc
     * Removes a button from the tablet menu.
     * @function TabletProxy#removeButton
     * @param {TabletButtonProxy} button - The button to remove.
     */
    Q_INVOKABLE void removeButton(TabletButtonProxy* tabletButtonProxy);

    /*@jsdoc
     * Sends a message to the current web page. To receive the message, the web page's script must connect to the
     * <code>EventBridge</code> that is automatically provided to the script:
     * <pre class="prettyprint"><code>EventBridge.scriptEventReceived.connect(function(message) {
     *     ...
     * });</code></pre>
     * <p><strong>Warning:</strong> The <code>EventBridge</code> object is not necessarily set up immediately ready for the web 
     * page's script to use. A simple workaround that normally works is to add a delay before calling 
     * <code>EventBridge.scriptEventReceived.connect(...)</code>. A better solution is to periodically call 
     * <code>EventBridge.scriptEventReceived.connect(...)</code> and then <code>EventBridge.emitWebEvent(...)</code> to send a 
     * message to the Interface script, and have that send a message back using <code>emitScriptEvent(...)</code>; when the 
     * return message is received, the <codE>EventBridge</code> is ready for use.</p>
     * @function TabletProxy#emitScriptEvent
     * @param {string|object} message - The message to send to the web page.
     */
    Q_INVOKABLE void emitScriptEvent(const QVariant& msg);

    /*@jsdoc
     * Sends a message to the current QML page. To receive the message, the QML page must implement a function:
     * <pre class="prettyprint"><code>function fromScript(message) {
     *   ...
     * }</code></pre>
     * @function TabletProxy#sendToQml
     * @param {string|object} message - The message to send to the QML page.
     */
    Q_INVOKABLE void sendToQml(const QVariant& msg);

    /*@jsdoc
     * Checks if the tablet is on the home screen.
     * @function TabletProxy#onHomeScreen
     * @returns {boolean} <code>true</code> if the tablet is on the home screen, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool onHomeScreen();

    /*@jsdoc
     * Sets whether the tablet is displayed in landscape or portrait mode.
     * <p>Note: The setting isn't used in toolbar mode.</p>
     * @function TabletProxy#setLandscape
     * @param {boolean} landscape - <code>true</code> to display the tablet in landscape mode, <code>false</code> to display it 
     *     in portrait mode.
     */
    Q_INVOKABLE void setLandscape(bool landscape) { _landscape = landscape; }

    /*@jsdoc
     * Gets whether the tablet is displayed in landscape or portrait mode.
     * <p>Note: The setting isn't used in toolbar mode.</p>
     * @function TabletProxy#getLandscape
     * @returns {boolean} <code>true</code> if the tablet is displayed in landscape mode, <code>false</code> if it is displayed 
     *     in portrait mode.
     */
    Q_INVOKABLE bool getLandscape() { return _landscape; }

    /*@jsdoc
     * Checks if a path is the current app or dialog displayed.
     * @function TabletProxy#isPathLoaded
     * @param {string} path - The path to test.
     * @returns {boolean} <code>true</code> if <code>path</code> is the current app or dialog, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isPathLoaded(const QVariant& path);

    QQuickItem* getTabletRoot() const { return _qmlTabletRoot; }

    OffscreenQmlSurface* getTabletSurface();

    QQuickItem* getQmlTablet() const;

    QQuickItem* getQmlMenu() const;

    TabletButtonListModel* getButtons() { return &_buttons; }

signals:
    /*@jsdoc
     * Triggered when a message from the current HTML web page displayed on the tablet is received. The HTML web page can send 
     * a message by calling:
     * <pre class="prettyprint"><code>EventBridge.emitWebEvent(message);</code></pre>
     * @function TabletProxy#webEventReceived
     * @param {string|object} message - The message received.
     * @returns {Signal}
     */
    void webEventReceived(QVariant msg);

    /*@jsdoc
     * Triggered when a message from the current QML page displayed on the tablet is received. The QML page can send a message 
     * (string or object) by calling: <pre class="prettyprint"><code>sendToScript(message);</code></pre>
     * @function TabletProxy#fromQml
     * @param {string|object} message - The message received.
     * @returns {Signal}
     */
    void fromQml(QVariant msg);

    /*@jsdoc
     * Triggered when the tablet's screen changes.
     * @function TabletProxy#screenChanged
     * @param type {string} - The type of the new screen or change: <code>"Home"</code>, <code>"Menu"</code>, 
     *     <code>"QML"</code>, <code>"Web"</code>, <code>"Closed"</code>, or <code>"Unknown"</code>.
     * @param url {string} - The url of the page displayed. Only valid for Web and QML.
     * @returns {Signal}
     */
    void screenChanged(QVariant type, QVariant url);

    /*@jsdoc
     * Triggered when the tablet is opened or closed.
     * <p>Note: Doesn't apply in toolbar mode.</p>
     * @function TabletProxy#tabletShownChanged
     * @returns {Signal}
     */
    void tabletShownChanged();

    /*@jsdoc
     * Triggered when the tablet's toolbar mode changes.
     * @function TabletProxy#toolbarModeChanged
     * @returns {Signal}
     * @example <caption>Report when the system tablet's toolbar mode changes.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * tablet.toolbarModeChanged.connect(function () {
     *     print("Tablet toolbar mode changed to: " + tablet.toolbarMode);
     * });
     * // Use Developer > UI > Tablet Becomes Toolbar to change the toolbar mode.
     */
    void toolbarModeChanged();

protected slots:
    
    /*@jsdoc
     * @function TabletProxy#desktopWindowClosed
     * @deprecated This function is deprecated and will be removed.
     */
    void desktopWindowClosed();

    /*@jsdoc
     * @function TabletProxy#emitWebEvent
     * @param {object|string} message - Message
     * @deprecated This function is deprecated and will be removed.
     */
    void emitWebEvent(const QVariant& msg);

    /*@jsdoc
     * @function TabletProxy#onTabletShown
     * @deprecated This function is deprecated and will be removed.
     */
    void onTabletShown();

protected:
    void loadHomeScreen(bool forceOntoHomeScreen);

    bool _initialScreen { false };
    QVariant _currentPathLoaded { "" };
    QString _name;
    QQuickItem* _qmlTabletRoot { nullptr };
    OffscreenQmlSurface* _qmlOffscreenSurface { nullptr };
    QmlWindowClass* _desktopWindow { nullptr };
    bool _toolbarMode { false };
    bool _tabletShown { false };

    enum class State { Uninitialized, Home, Web, Menu, QML };
    State _state { State::Uninitialized };
    std::pair<QVariant, State> _initialPath { "", State::Uninitialized };
    std::pair<QVariant, bool> _initialWebPathParams;
    bool _landscape { false };
    bool _showRunningScripts { false };

    TabletButtonListModel _buttons;

private:
    void stopQMLSource();
};

Q_DECLARE_METATYPE(TabletProxy*);

/*@jsdoc
 * A tablet button. In toolbar mode (Developer &gt; UI &gt; Tablet Becomes Toolbar), the tablet button is displayed on the 
 * toolbar.
 *
 * <p>Create a new button using {@link TabletProxy#addButton}.</p>
 *
 * @class TabletButtonProxy
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {Uuid} uuid - The ID of the button. <em>Read-only.</em>
 * @property {TabletButtonProxy.ButtonProperties} properties - The current values of the button's properties. Only properties 
 *     that have been set during button creation or subsequently edited are returned. <em>Read-only.</em>
 */
class TabletButtonProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUuid uuid READ getUuid)
    Q_PROPERTY(QVariantMap properties READ getProperties NOTIFY propertiesChanged)
public:
    TabletButtonProxy(const QVariantMap& properties);
    ~TabletButtonProxy();

    QUuid getUuid() const { return _uuid; }

    /*@jsdoc
     * Gets the current values of the button's properties. Only properties that have been set during button creation or 
     * subsequently edited are returned.
     * @function TabletButtonProxy#getProperties
     * @returns {TabletButtonProxy.ButtonProperties} The button properties.
     * @example <caption>Report a test button's properties.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * var button = tablet.addButton({ text: "TEST" });
     * 
     * var properties = button.getProperties();
     * print("TEST button properties: " + JSON.stringify(properties));
     * 
     * Script.scriptEnding.connect(function () {
     *     tablet.removeButton(button);
     * });
     */
    Q_INVOKABLE QVariantMap getProperties();

    /*@jsdoc
     * Changes the values of the button's properties.
     * @function TabletButtonProxy#editProperties
     * @param {TabletButtonProxy.ButtonProperties} properties - The properties to change.
     * @example <caption>Set a button's hover text after a delay.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * var button = tablet.addButton({ text: "TEST" });
     * 
     * button.propertiesChanged.connect(function () {
     *     print("TEST button properties changed");
     * });
     * 
     * Script.setTimeout(function () {
     *     button.editProperties({ text: "CHANGED" });
     * }, 2000);
     * 
     * Script.scriptEnding.connect(function () {
     *     tablet.removeButton(button);
     * });
     */
    Q_INVOKABLE void editProperties(const QVariantMap& properties);

signals:
    /*@jsdoc
     * Triggered when the button is clicked.
     * @function TabletButtonProxy#clicked
     * @returns {Signal}
     * @example <caption>Report a menu button click.</caption>
     * var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
     * var button = tablet.addButton({ text: "TEST" });
     *
     * button.clicked.connect(function () {
     *     print("TEST button clicked");
     * });
     *
     * Script.scriptEnding.connect(function () {
     *     tablet.removeButton(button);
     * });
     */
    void clicked();

    /*@jsdoc
     * Triggered when a button's properties are changed.
     * @function TabletButtonProxy#propertiesChanged
     * @returns {Signal}
     */
    void propertiesChanged();

protected:
    QUuid _uuid;
    int _stableOrder;

    // FIXME: There are additional properties.
    QVariantMap _properties;
};

Q_DECLARE_METATYPE(TabletButtonProxy*);

#endif // hifi_TabletScriptingInterface_h
