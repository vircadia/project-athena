//
//  WindowScriptingInterface.cpp
//  interface/src/scripting
//
//  Created by Ryan Huffman on 4/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WindowScriptingInterface.h"

#include <QClipboard>
#include <QtCore/QDir>
#include <QMessageBox>
#include <ScriptValue.h>
#include <QtGui/QDesktopServices>
#include <shared/QtHelpers.h>
#include <SettingHandle.h>

#include <plugins/PluginManager.h>
#include <display-plugins/CompositorHelper.h>
#include <AddressManager.h>
#include "AndroidHelper.h"
#include "Application.h"
#include "DomainHandler.h"
#include "MainWindow.h"
#include "Menu.h"
#include "OffscreenUi.h"
#include "commerce/QmlCommerce.h"
#include "NetworkingConstants.h"

static const QString DESKTOP_LOCATION = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
static const QString LAST_BROWSE_LOCATION_SETTING = "LastBrowseLocation";
static const QString LAST_BROWSE_ASSETS_LOCATION_SETTING = "LastBrowseAssetsLocation";


WindowScriptingInterface::WindowScriptingInterface() {
    const DomainHandler& domainHandler = DependencyManager::get<NodeList>()->getDomainHandler();
    connect(&domainHandler, &DomainHandler::connectedToDomain, this, &WindowScriptingInterface::domainChanged);
    connect(&domainHandler, &DomainHandler::disconnectedFromDomain, this, &WindowScriptingInterface::disconnectedFromDomain);

    connect(&domainHandler, &DomainHandler::domainConnectionRefused, this, &WindowScriptingInterface::domainConnectionRefused);
    connect(&domainHandler, &DomainHandler::redirectErrorStateChanged, this, &WindowScriptingInterface::redirectErrorStateChanged);

    connect(qApp, &Application::svoImportRequested, [this](const QString& urlString) {
        static const QMetaMethod svoImportRequestedSignal =
            QMetaMethod::fromSignal(&WindowScriptingInterface::svoImportRequested);

        if (isSignalConnected(svoImportRequestedSignal)) {
            QUrl url(urlString);
            emit svoImportRequested(url.url());
        } else {
            OffscreenUi::asyncWarning("Import SVO Error", "You need to be running edit.js to import entities.");
        }
    });

    connect(qApp->getWindow(), &MainWindow::windowGeometryChanged, this, &WindowScriptingInterface::onWindowGeometryChanged);
    connect(qApp->getWindow(), &MainWindow::windowMinimizedChanged, this, &WindowScriptingInterface::minimizedChanged);
    connect(qApp, &Application::interstitialModeChanged, [this] (bool interstitialMode) {
        emit interstitialModeChanged(interstitialMode);
    });
}

WindowScriptingInterface::~WindowScriptingInterface() {
    QHashIterator<int, QQuickItem*> i(_messageBoxes);
    while (i.hasNext()) {
        i.next();
        auto messageBox = i.value();
        disconnect(messageBox);
        messageBox->setVisible(false);
        messageBox->deleteLater();
    }

    _messageBoxes.clear();
}

ScriptValuePointer WindowScriptingInterface::hasFocus() {
    return engine()->newValue(qApp->hasFocus());
}

void WindowScriptingInterface::setFocus() {
    // It's forbidden to call focus() from another thread.
    qApp->postLambdaEvent([] {
        qApp->setFocus();
    });
}

void WindowScriptingInterface::raise() {
    // It's forbidden to call raise() from another thread.
    qApp->postLambdaEvent([] {
        qApp->raise();
    });
}

/// Display an alert box
/// \param const QString& message message to display
/// \return ScriptValuePointer::UndefinedValue
void WindowScriptingInterface::alert(const QString& message) {
    OffscreenUi::asyncWarning("", message, QMessageBox::Ok, QMessageBox::Ok);
}

/// Display a confirmation box with the options 'Yes' and 'No'
/// \param const QString& message message to display
/// \return ScriptValuePointer `true` if 'Yes' was clicked, `false` otherwise
ScriptValuePointer WindowScriptingInterface::confirm(const QString& message) {
    return engine()->newValue((QMessageBox::Yes == OffscreenUi::question("", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)));
}

/// Display a prompt with a text box
/// \param const QString& message message to display
/// \param const QString& defaultText default text in the text box
/// \return ScriptValuePointer string text value in text box if the dialog was accepted, `null` otherwise.
ScriptValuePointer WindowScriptingInterface::prompt(const QString& message, const QString& defaultText) {
    QString result = OffscreenUi::getText(nullptr, "", message, QLineEdit::Normal, defaultText);
    auto sResult = engine()->newValue(result);
    if (sResult->equals(engine()->newValue(""))) {
        return engine()->nullValue();
    }
    return sResult;
}

/// Display a prompt with a text box
/// \param const QString& message message to display
/// \param const QString& defaultText default text in the text box
void WindowScriptingInterface::promptAsync(const QString& message, const QString& defaultText) {
    bool ok = false;
    ModalDialogListener* dlg = OffscreenUi::getTextAsync(nullptr, "", message, QLineEdit::Normal, defaultText, &ok);
    connect(dlg, &ModalDialogListener::response, this, [=] (QVariant result) {
        disconnect(dlg, &ModalDialogListener::response, this, nullptr);
        emit promptTextChanged(result.toString());
    });
}

void WindowScriptingInterface::disconnectedFromDomain() {
    emit domainChanged(QUrl());
}

void WindowScriptingInterface::openUrl(const QUrl& url) {
    if (!url.isEmpty()) {
        auto scheme = url.scheme();
        if (scheme == URL_SCHEME_HIFI) {
            DependencyManager::get<AddressManager>()->handleLookupString(url.toString());
        } else if (scheme == URL_SCHEME_HIFIAPP) {
            DependencyManager::get<QmlCommerce>()->openSystemApp(url.path());
        } else {
#if defined(Q_OS_ANDROID)
            QMap<QString, QString> args;
            args["url"] = url.toString();
            AndroidHelper::instance().requestActivity("WebView", true, args);
#else
            QDesktopServices::openUrl(url);
#endif
        }
    }
}

void WindowScriptingInterface::openAndroidActivity(const QString& activityName, const bool backToScene) {
#if defined(Q_OS_ANDROID)
    AndroidHelper::instance().requestActivity(activityName, backToScene);
#endif
}


QString fixupPathForMac(const QString& directory) {
    // On OS X `directory` does not work as expected unless a file is included in the path, so we append a bogus
    // filename if the directory is valid.
    QString path = "";
    QFileInfo fileInfo = QFileInfo(directory);
    if (fileInfo.isDir()) {
        fileInfo.setFile(directory, "__HIFI_INVALID_FILE__");
        path = fileInfo.filePath();
    }
    return path;
}

QString WindowScriptingInterface::getPreviousBrowseLocation() const {
    return Setting::Handle<QString>(LAST_BROWSE_LOCATION_SETTING, DESKTOP_LOCATION).get();
}

void WindowScriptingInterface::setPreviousBrowseLocation(const QString& location) {
    Setting::Handle<QVariant>(LAST_BROWSE_LOCATION_SETTING).set(location);
}

QString WindowScriptingInterface::getPreviousBrowseAssetLocation() const {
    QString ASSETS_ROOT_PATH = "/";
    return Setting::Handle<QString>(LAST_BROWSE_ASSETS_LOCATION_SETTING, ASSETS_ROOT_PATH).get();
}

void WindowScriptingInterface::setPreviousBrowseAssetLocation(const QString& location) {
    Setting::Handle<QVariant>(LAST_BROWSE_ASSETS_LOCATION_SETTING).set(location);
}

bool WindowScriptingInterface::getInterstitialModeEnabled() const {
    return DependencyManager::get<NodeList>()->getDomainHandler().getInterstitialModeEnabled();
}

void WindowScriptingInterface::setInterstitialModeEnabled(bool enableInterstitialMode) {
    DependencyManager::get<NodeList>()->getDomainHandler().setInterstitialModeEnabled(enableInterstitialMode);
}

bool WindowScriptingInterface::isPointOnDesktopWindow(QVariant point) {
    auto offscreenUI = DependencyManager::get<OffscreenUi>();
    return offscreenUI ? offscreenUI->isPointOnDesktopWindow(point) : false;
}

/// Makes sure that the reticle is visible, use this in blocking forms that require a reticle and
/// might be in same thread as a script that sets the reticle to invisible
void WindowScriptingInterface::ensureReticleVisible() const {
    auto compositorHelper = DependencyManager::get<CompositorHelper>();
    if (!compositorHelper->getReticleVisible()) {
        compositorHelper->setReticleVisible(true);
    }
}

/// Display a "browse to directory" dialog.  If `directory` is an invalid file or directory the browser will start at the current
/// working directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the directory browser at
/// \return ScriptValuePointer file path as a string if one was selected, otherwise `ScriptValuePointer::NullValue`
ScriptValuePointer WindowScriptingInterface::browseDir(const QString& title, const QString& directory) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    QString result = OffscreenUi::getExistingDirectory(nullptr, title, path);
    if (!result.isEmpty()) {
        setPreviousBrowseLocation(QFileInfo(result).absolutePath());
    }
    return result.isEmpty() ? engine()->nullValue() : engine()->newValue(result);
}

/// Display a "browse to directory" dialog.  If `directory` is an invalid file or directory the browser will start at the current
/// working directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the directory browser at
void WindowScriptingInterface::browseDirAsync(const QString& title, const QString& directory) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    ModalDialogListener* dlg = OffscreenUi::getExistingDirectoryAsync(nullptr, title, path);
    connect(dlg, &ModalDialogListener::response, this, [=] (QVariant response) {
        const QString& result = response.toString();
        disconnect(dlg, &ModalDialogListener::response, this, nullptr);
        if (!result.isEmpty()) {
            setPreviousBrowseLocation(QFileInfo(result).absolutePath());
        }
        emit browseDirChanged(result);
    });
}

/// \param const QString& title title of the window
/// \param const QString& directory directory to start the file browser at
/// \param const QString& nameFilter filter to filter filenames by - see `QFileDialog`
/// \return ScriptValuePointer file path as a string if one was selected, otherwise `ScriptValuePointer::NullValue`
ScriptValuePointer WindowScriptingInterface::browse(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    QString result = OffscreenUi::getOpenFileName(nullptr, title, path, nameFilter);
    if (!result.isEmpty()) {
        setPreviousBrowseLocation(QFileInfo(result).absolutePath());
    }
    return result.isEmpty() ? engine()->nullValue() : engine()->newValue(result);
}

/// Display an open file dialog.  If `directory` is an invalid file or directory the browser will start at the current
/// working directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the file browser at
/// \param const QString& nameFilter filter to filter filenames by - see `QFileDialog`
void WindowScriptingInterface::browseAsync(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    ModalDialogListener* dlg = OffscreenUi::getOpenFileNameAsync(nullptr, title, path, nameFilter);
    connect(dlg, &ModalDialogListener::response, this, [=] (QVariant response) {
        const QString& result = response.toString();
        disconnect(dlg, &ModalDialogListener::response, this, nullptr);
        if (!result.isEmpty()) {
            setPreviousBrowseLocation(QFileInfo(result).absolutePath());
        }
        emit browseChanged(result);
    });
}

/// Display a save file dialog.  If `directory` is an invalid file or directory the browser will start at the current
/// working directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the file browser at
/// \param const QString& nameFilter filter to filter filenames by - see `QFileDialog`
/// \return ScriptValuePointer file path as a string if one was selected, otherwise `ScriptValuePointer::NullValue`
ScriptValuePointer WindowScriptingInterface::save(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    QString result = OffscreenUi::getSaveFileName(nullptr, title, path, nameFilter);
    if (!result.isEmpty()) {
        setPreviousBrowseLocation(QFileInfo(result).absolutePath());
    }
    return result.isEmpty() ? engine()->nullValue() : engine()->newValue(result);
}

/// Display a save file dialog.  If `directory` is an invalid file or directory the browser will start at the current
/// working directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the file browser at
/// \param const QString& nameFilter filter to filter filenames by - see `QFileDialog`
void WindowScriptingInterface::saveAsync(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseLocation();
    }
#ifndef Q_OS_WIN
    path = fixupPathForMac(directory);
#endif
    ModalDialogListener* dlg = OffscreenUi::getSaveFileNameAsync(nullptr, title, path, nameFilter);
    connect(dlg, &ModalDialogListener::response, this, [=] (QVariant response) {
        const QString& result = response.toString();
        disconnect(dlg, &ModalDialogListener::response, this, nullptr);
        if (!result.isEmpty()) {
            setPreviousBrowseLocation(QFileInfo(result).absolutePath());
        }
        emit saveFileChanged(result);
    });
}

/// Display a select asset dialog that lets the user select an asset from the Asset Server.  If `directory` is an invalid
/// directory the browser will start at the root directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the asset browser at
/// \param const QString& nameFilter filter to filter asset names by - see `QFileDialog`
/// \return ScriptValuePointer asset path as a string if one was selected, otherwise `ScriptValuePointer::NullValue`
ScriptValuePointer WindowScriptingInterface::browseAssets(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseAssetLocation();
    }
    if (path.left(1) != "/") {
        path = "/" + path;
    }
    if (path.right(1) != "/") {
        path = path + "/";
    }
    QString result = OffscreenUi::getOpenAssetName(nullptr, title, path, nameFilter);
    if (!result.isEmpty()) {
        setPreviousBrowseAssetLocation(QFileInfo(result).absolutePath());
    }
    return result.isEmpty() ? engine()->nullValue() : engine()->newValue(result);
}

/// Display a select asset dialog that lets the user select an asset from the Asset Server.  If `directory` is an invalid
/// directory the browser will start at the root directory.
/// \param const QString& title title of the window
/// \param const QString& directory directory to start the asset browser at
/// \param const QString& nameFilter filter to filter asset names by - see `QFileDialog`
void WindowScriptingInterface::browseAssetsAsync(const QString& title, const QString& directory, const QString& nameFilter) {
    ensureReticleVisible();
    QString path = directory;
    if (path.isEmpty()) {
        path = getPreviousBrowseAssetLocation();
    }
    if (path.left(1) != "/") {
        path = "/" + path;
    }
    if (path.right(1) != "/") {
        path = path + "/";
    }

    ModalDialogListener* dlg = OffscreenUi::getOpenAssetNameAsync(nullptr, title, path, nameFilter);
    connect(dlg, &ModalDialogListener::response, this, [=] (QVariant response) {
        const QString& result = response.toString();
        disconnect(dlg, &ModalDialogListener::response, this, nullptr);
        if (!result.isEmpty()) {
            setPreviousBrowseAssetLocation(QFileInfo(result).absolutePath());
        }
        emit assetsDirChanged(result);
    });
}

void WindowScriptingInterface::showAssetServer(const QString& upload) {
    QMetaObject::invokeMethod(qApp, "showAssetServerWidget", Qt::QueuedConnection, Q_ARG(QString, upload));
}

QString WindowScriptingInterface::checkVersion() {
    return QCoreApplication::applicationVersion();
}

QString WindowScriptingInterface::getUserAgent() {
    return NetworkingConstants::VIRCADIA_USER_AGENT;
}

QString WindowScriptingInterface::protocolSignature() {
    return protocolVersionsSignatureBase64();
}

int WindowScriptingInterface::getInnerWidth() {
    return qApp->getPrimaryWidget()->geometry().width();
}

int WindowScriptingInterface::getInnerHeight() {
    return qApp->getPrimaryWidget()->geometry().height();
}

glm::vec2 WindowScriptingInterface::getDeviceSize() const {
    return qApp->getDeviceSize();
}

int WindowScriptingInterface::getLastDomainConnectionError() const {
    return DependencyManager::get<NodeList>()->getDomainHandler().getLastDomainConnectionError();
}

int WindowScriptingInterface::getX() {
    return qApp->getWindow()->geometry().x();
}

int WindowScriptingInterface::getY() {
    auto menu = qApp->getPrimaryMenu();
    int menuHeight = menu ? menu->geometry().height() : 0;
    return qApp->getWindow()->geometry().y() + menuHeight;
}

void WindowScriptingInterface::onWindowGeometryChanged(const QRect& windowGeometry) {
    auto geometry = windowGeometry;
    auto menu = qApp->getPrimaryMenu();
    if (menu) {
        geometry.setY(geometry.y() + menu->geometry().height());
    }
    emit geometryChanged(geometry);
}

void WindowScriptingInterface::copyToClipboard(const QString& text) {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, "copyToClipboard", Q_ARG(QString, text));
        return;
    }

    qDebug() << "Copying";
    QApplication::clipboard()->setText(text);
}


bool WindowScriptingInterface::setDisplayTexture(const QString& name) {
    return  qApp->getActiveDisplayPlugin()->setDisplayTexture(name);   // Plugins that don't know how, answer false.
}

void WindowScriptingInterface::takeSnapshot(bool notify, bool includeAnimated, float aspectRatio, const QString& filename) {
    qApp->takeSnapshot(notify, includeAnimated, aspectRatio, filename);
}

void WindowScriptingInterface::takeSecondaryCameraSnapshot(const bool& notify, const QString& filename) {
    qApp->takeSecondaryCameraSnapshot(notify, filename);
}

void WindowScriptingInterface::takeSecondaryCamera360Snapshot(const glm::vec3& cameraPosition, const bool& cubemapOutputFormat, const bool& notify, const QString& filename) {
    qApp->takeSecondaryCamera360Snapshot(cameraPosition, cubemapOutputFormat, notify, filename);
}

void WindowScriptingInterface::shareSnapshot(const QString& path, const QUrl& href) {
    qApp->shareSnapshot(path, href);
}

void WindowScriptingInterface::makeConnection(bool success, const QString& userNameOrError) {
    if (success) {
        emit connectionAdded(userNameOrError);
    } else {
        emit connectionError(userNameOrError);
    }
}

void WindowScriptingInterface::displayAnnouncement(const QString& message) {
    emit announcement(message);
}

bool WindowScriptingInterface::isPhysicsEnabled() {
    return qApp->isPhysicsEnabled();
}

int WindowScriptingInterface::openMessageBox(QString title, QString text, int buttons, int defaultButton) {
    if (QThread::currentThread() != thread()) {
        int result;
        BLOCKING_INVOKE_METHOD(this, "openMessageBox",
            Q_RETURN_ARG(int, result),
            Q_ARG(QString, title),
            Q_ARG(QString, text),
            Q_ARG(int, buttons),
            Q_ARG(int, defaultButton));
        return result;
    }

    return createMessageBox(title, text, buttons, defaultButton);
}

/*@jsdoc
 * <p>The buttons that may be included in a message box created by {@link Window.openMessageBox|openMessageBox} are defined by
 * numeric values:
 * <table>
 *   <thead>
 *     <tr>
 *       <th>Button</th>
 *       <th>Value</th>
 *       <th>Description</th>
 *     </tr>
 *   </thead>
 *   <tbody>
 *     <tr> <td><strong>NoButton</strong></td> <td><code>0x0</code></td> <td>An invalid button.</td> </tr>
 *     <tr> <td><strong>Ok</strong></td> <td><code>0x400</code></td> <td>"OK"</td> </tr>
 *     <tr> <td><strong>Save</strong></td> <td><code>0x800</code></td> <td>"Save"</td> </tr>
 *     <tr> <td><strong>SaveAll</strong></td> <td><code>0x1000</code></td> <td>"Save All"</td> </tr>
 *     <tr> <td><strong>Open</strong></td> <td><code>0x2000</code></td> <td>"Open"</td> </tr>
 *     <tr> <td><strong>Yes</strong></td> <td><code>0x4000</code></td> <td>"Yes"</td> </tr>
 *     <tr> <td><strong>YesToAll</strong></td> <td><code>0x8000</code></td> <td>"Yes to All"</td> </tr>
 *     <tr> <td><strong>No</strong></td> <td><code>0x10000</code></td> <td>"No"</td> </tr>
 *     <tr> <td><strong>NoToAll</strong></td> <td><code>0x20000</code></td> <td>"No to All"</td> </tr>
 *     <tr> <td><strong>Abort</strong></td> <td><code>0x40000</code></td> <td>"Abort"</td> </tr>
 *     <tr> <td><strong>Retry</strong></td> <td><code>0x80000</code></td> <td>"Retry"</td> </tr>
 *     <tr> <td><strong>Ignore</strong></td> <td><code>0x100000</code></td> <td>"Ignore"</td> </tr>
 *     <tr> <td><strong>Close</strong></td> <td><code>0x200000</code></td> <td>"Close"</td> </tr>
 *     <tr> <td><strong>Cancel</strong></td> <td><code>0x400000</code></td> <td>"Cancel"</td> </tr>
 *     <tr> <td><strong>Discard</strong></td> <td><code>0x800000</code></td> <td>"Discard" or "Don't Save"</td> </tr>
 *     <tr> <td><strong>Help</strong></td> <td><code>0x1000000</code></td> <td>"Help"</td> </tr>
 *     <tr> <td><strong>Apply</strong></td> <td><code>0x2000000</code></td> <td>"Apply"</td> </tr>
 *     <tr> <td><strong>Reset</strong></td> <td><code>0x4000000</code></td> <td>"Reset"</td> </tr>
 *     <tr> <td><strong>RestoreDefaults</strong></td> <td><code>0x8000000</code></td> <td>"Restore Defaults"</td> </tr>
 *   </tbody>
 * </table>
 * @typedef {number} Window.MessageBoxButton
 */
int WindowScriptingInterface::createMessageBox(QString title, QString text, int buttons, int defaultButton) {
    if (auto offscreenUI = DependencyManager::get<OffscreenUi>()) {
        auto messageBox = offscreenUI->createMessageBox(OffscreenUi::ICON_INFORMATION, title, text,
            static_cast<QFlags<QMessageBox::StandardButton>>(buttons), static_cast<QMessageBox::StandardButton>(defaultButton));
        connect(messageBox, SIGNAL(selected(int)), this, SLOT(onMessageBoxSelected(int)));

        _lastMessageBoxID += 1;
        _messageBoxes.insert(_lastMessageBoxID, messageBox);
    }

    return _lastMessageBoxID;
}

void WindowScriptingInterface::updateMessageBox(int id, QString title, QString text, int buttons, int defaultButton) {
    auto messageBox = _messageBoxes.value(id);
    if (messageBox) {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, "updateMessageBox",
                Q_ARG(int, id),
                Q_ARG(QString, title),
                Q_ARG(QString, text),
                Q_ARG(int, buttons),
                Q_ARG(int, defaultButton));
            return;
        }

        messageBox->setProperty("title", title);
        messageBox->setProperty("text", text);
        messageBox->setProperty("buttons", buttons);
        messageBox->setProperty("defaultButton", defaultButton);
    }
}

void WindowScriptingInterface::closeMessageBox(int id) {
    auto messageBox = _messageBoxes.value(id);
    if (messageBox) {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, "closeMessageBox",
                Q_ARG(int, id));
            return;
        }

        disconnect(messageBox);
        messageBox->setVisible(false);
        messageBox->deleteLater();
        _messageBoxes.remove(id);
    }
}

void WindowScriptingInterface::onMessageBoxSelected(int button) {
    auto messageBox = qobject_cast<QQuickItem*>(sender());
    auto keys = _messageBoxes.keys(messageBox);
    if (keys.length() > 0) {
        auto id = keys[0];  // Should be just one message box.
        emit messageBoxClosed(id, button);
        disconnect(messageBox);
        messageBox->setVisible(false);
        messageBox->deleteLater();
        _messageBoxes.remove(id);
    }
}


float WindowScriptingInterface::domainLoadingProgress() {
    return qApp->getOctreePacketProcessor().domainLoadingProgress();
}

int WindowScriptingInterface::getDisplayPluginCount() {
    return (int)PluginManager::getInstance()->getDisplayPlugins().size();
}

QString WindowScriptingInterface::getDisplayPluginName(int index) {
    return PluginManager::getInstance()->getDisplayPlugins().at(index)->getName();
}

bool WindowScriptingInterface::isDisplayPluginHmd(int index) {
    return PluginManager::getInstance()->getDisplayPlugins().at(index)->isHmd();
}

int WindowScriptingInterface::getActiveDisplayPlugin() {
    auto active = qApp->getActiveDisplayPlugin();
    auto size = getDisplayPluginCount();
    for (int i = 0; i < size; ++i) {
        if (PluginManager::getInstance()->getDisplayPlugins().at(i) == active) {
            return i;
        }
    }
    return -1;
}

void WindowScriptingInterface::setActiveDisplayPlugin(int index) {
    auto name = PluginManager::getInstance()->getDisplayPlugins().at(index)->getName();
    qApp->setActiveDisplayPlugin(name);
}

void WindowScriptingInterface::openWebBrowser(const QString& url) {
    auto offscreenUI = DependencyManager::get<OffscreenUi>();
    if (!offscreenUI) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "openWebBrowser", Q_ARG(const QString&, url));
        return;
    }

    offscreenUI->load("Browser.qml", [=](QQmlContext* context, QObject* newObject) {
        if (!url.isEmpty()) {
            newObject->setProperty("url", url);
        }
    });
}
