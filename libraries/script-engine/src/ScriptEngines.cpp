//
//  Created by Bradley Austin Davis on 2016/01/08
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptEngines.h"

#include <QtCore/QStandardPaths>

#include <QtWidgets/QApplication>
#include <QtCore/QThread>

#include <shared/QtHelpers.h>
#include <SettingHandle.h>
#include <UserActivityLogger.h>
#include <PathUtils.h>
#include <shared/FileUtils.h>

#include "ScriptCache.h"
#include "ScriptEngine.h"
#include "ScriptEngineLogging.h"

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "(" __STR1__(__LINE__) ") : Warning Msg: "

static const QString DESKTOP_LOCATION = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
static const bool HIFI_SCRIPT_DEBUGGABLES { true };
static const QString SETTINGS_KEY { "RunningScripts" };
static const QUrl DEFAULT_SCRIPTS_LOCATION { "file:///~//defaultScripts.js" };

// Using a QVariantList so this is human-readable in the settings file
static Setting::Handle<QVariantList> runningScriptsHandle(SETTINGS_KEY, { QVariant(DEFAULT_SCRIPTS_LOCATION) });

const int RELOAD_ALL_SCRIPTS_TIMEOUT = 1000;


ScriptsModel& getScriptsModel() {
    static ScriptsModel scriptsModel;
    return scriptsModel;
}

void ScriptEngines::onPrintedMessage(const QString& message, const QString& scriptName) {
    emit printedMessage(message, scriptName);
}

void ScriptEngines::onErrorMessage(const QString& message, const QString& scriptName) {
    emit errorMessage(message, scriptName);
}

void ScriptEngines::onWarningMessage(const QString& message, const QString& scriptName) {
    emit warningMessage(message, scriptName);
}

void ScriptEngines::onInfoMessage(const QString& message, const QString& scriptName) {
    emit infoMessage(message, scriptName);
}

void ScriptEngines::onClearDebugWindow() {
    emit clearDebugWindow();
}

void ScriptEngines::onErrorLoadingScript(const QString& url) {
    emit errorLoadingScript(url);
}

ScriptEngines::ScriptEngines(ScriptManager::Context context, const QUrl& defaultScriptsOverride)
    : _context(context), _defaultScriptsOverride(defaultScriptsOverride)
{
    scriptGatekeeper.initialize();
    
    _scriptsModelFilter.setSourceModel(&_scriptsModel);
    _scriptsModelFilter.sort(0, Qt::AscendingOrder);
    _scriptsModelFilter.setDynamicSortFilter(true);
}

QUrl normalizeScriptURL(const QUrl& rawScriptURL) {
    if (rawScriptURL.scheme() == "file") {
        QUrl fullNormal = rawScriptURL;
        QUrl defaultScriptLoc = PathUtils::defaultScriptsLocation();

        // if this url is something "beneath" the default script url, replace the local path with ~
        if (fullNormal.scheme() == defaultScriptLoc.scheme() &&
            fullNormal.host() == defaultScriptLoc.host() &&
            fullNormal.path().startsWith(defaultScriptLoc.path())) {
            fullNormal.setPath("/~/" + fullNormal.path().mid(defaultScriptLoc.path().size()));
        }
        return fullNormal;
    } else if (rawScriptURL.scheme() == "http" || rawScriptURL.scheme() == "https" || rawScriptURL.scheme() == "atp") {
        return rawScriptURL;
    } else {
        // don't accidently support gopher
        return QUrl("");
    }
}

QString expandScriptPath(const QString& rawPath) {
    QStringList splitPath = rawPath.split("/");
    QUrl defaultScriptsLoc = PathUtils::defaultScriptsLocation();
    return defaultScriptsLoc.path() + "/" + splitPath.mid(2).join("/"); // 2 to skip the slashes in /~/
}

QUrl expandScriptUrl(const QUrl& rawScriptURL) {
    QUrl normalizedScriptURL = normalizeScriptURL(rawScriptURL);
    if (normalizedScriptURL.scheme() == "http" ||
        normalizedScriptURL.scheme() == "https" ||
        normalizedScriptURL.scheme() == "atp") {
        return normalizedScriptURL;
    } else if (normalizedScriptURL.scheme() == "file") {
        if (normalizedScriptURL.path().startsWith("/~/")) {
            QUrl url = normalizedScriptURL;
            url.setPath(expandScriptPath(url.path()));

            // stop something like Script.include(["/~/../Desktop/naughty.js"]); from working
            QFileInfo fileInfo(url.toLocalFile());
            url = QUrl::fromLocalFile(fileInfo.canonicalFilePath());

            QUrl defaultScriptsLoc = PathUtils::defaultScriptsLocation();
            if (!defaultScriptsLoc.isParentOf(url) && defaultScriptsLoc != url) {
                qCWarning(scriptengine) << "Script.include() ignoring file path"
                                        << "-- outside of standard libraries: "
                                        << url.path()
                                        << defaultScriptsLoc.path();
                return rawScriptURL;
            }
            if (rawScriptURL.path().endsWith("/") && !url.path().endsWith("/")) {
                url.setPath(url.path() + "/");
            }
            return url;
        }
        return normalizedScriptURL;
    } else {
        return QUrl("");
    }
}


QObject* scriptsModel();

void ScriptEngines::addScriptEngine(ScriptManagerPointer manager) {
    if (!_isStopped) {
        QMutexLocker locker(&_allScriptsMutex);
        _allKnownScriptManagers.insert(manager);
    }
}

void ScriptEngines::removeScriptEngine(ScriptManagerPointer manager) {
    // If we're not already in the middle of stopping all scripts, then we should remove ourselves
    // from the list of running scripts. We don't do this if we're in the process of stopping all scripts
    // because that method removes scripts from its list as it iterates them
    if (!_isStopped) {
        QMutexLocker locker(&_allScriptsMutex);
        _allKnownScriptManagers.remove(manager);
    }
}

void ScriptEngines::shutdownScripting() {
    _isStopped = true;
    QMutexLocker locker(&_allScriptsMutex);
    qCDebug(scriptengine) << "Stopping all scripts.... currently known scripts:" << _allKnownScriptManagers.size();

    QMutableSetIterator<ScriptManagerPointer> i(_allKnownScriptManagers);
    while (i.hasNext()) {
        ScriptManagerPointer scriptManager = i.next();
        QString scriptName = scriptManager->getFilename();

        // NOTE: typically all script engines are running. But there's at least one known exception to this, the
        // "entities sandbox" which is only used to evaluate entities scripts to test their validity before using
        // them. We don't need to stop scripts that aren't running.
        // TODO: Scripts could be shut down faster if we spread them across a threadpool.
        if (scriptManager->isRunning()) {
            qCDebug(scriptengine) << "about to shutdown script:" << scriptName;

            // We disconnect any script engine signals from the application because we don't want to do any
            // extra stopScript/loadScript processing that the Application normally does when scripts start
            // and stop. We can safely short circuit this because we know we're in the "quitting" process
            scriptManager->disconnect(this);

            // Gracefully stop the engine's scripting thread
            scriptManager->stop();

            // We need to wait for the engine to be done running before we proceed, because we don't
            // want any of the scripts final "scriptEnding()" or pending "update()" methods from accessing
            // any application state after we leave this stopAllScripts() method
            qCDebug(scriptengine) << "waiting on script:" << scriptName;
            scriptManager->waitTillDoneRunning(true);
            qCDebug(scriptengine) << "done waiting on script:" << scriptName;
        }
        // Once the script is stopped, we can remove it from our set
        i.remove();
    }
    qCDebug(scriptengine) << "DONE Stopping all scripts....";
}

/*@jsdoc
 * Information on a public script, i.e., a script that's included in the Interface installation.
 * @typedef {object} ScriptDiscoveryService.PublicScript
 * @property {string} name - The script's file name.
 * @property {string} type - <code>"script"</code> or <code>"folder"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed. It currently always has the value, 
 *     <code>"script"</code>.</p>
 * @property {ScriptDiscoveryService.PublicScript[]} [children] - Only present if <code>type == "folder"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed. It currently is never present.
 * @property {string} [url] - The full URL of the script &mdash; including the <code>"file:///"</code> scheme at the start.
 *     <p>Only present if <code>type == "script"</code>.</p>
 */
QVariantList getPublicChildNodes(TreeNodeFolder* parent) {
    QVariantList result;
    QList<TreeNodeBase*> treeNodes = getScriptsModel().getFolderNodes(parent);
    for (int i = 0; i < treeNodes.size(); i++) {
        TreeNodeBase* node = treeNodes.at(i);
        if (node->getType() == TREE_NODE_TYPE_FOLDER) {
            TreeNodeFolder* folder = static_cast<TreeNodeFolder*>(node);
            QVariantMap resultNode;
            resultNode.insert("name", node->getName());
            resultNode.insert("type", "folder");
            resultNode.insert("children", getPublicChildNodes(folder));
            result.append(resultNode);
            continue;
        }
        TreeNodeScript* script = static_cast<TreeNodeScript*>(node);
        if (script->getOrigin() == ScriptOrigin::SCRIPT_ORIGIN_LOCAL) {
            continue;
        }
        QVariantMap resultNode;
        resultNode.insert("name", node->getName());
        resultNode.insert("type", "script");
        resultNode.insert("url", script->getFullPath());
        result.append(resultNode);
    }
    return result;
}

QVariantList ScriptEngines::getPublic() {
    return getPublicChildNodes(NULL);
}

/*@jsdoc
 * Information on a local script.
 * @typedef {object} ScriptDiscoveryService.LocalScript
 * @property {string} name - The script's file name.
 * @property {string} path - The script's path.
 * @deprecated This type is deprecated and will be removed.
 */
QVariantList ScriptEngines::getLocal() {
    QVariantList result;
    QList<TreeNodeBase*> treeNodes = getScriptsModel().getFolderNodes(NULL);
    for (int i = 0; i < treeNodes.size(); i++) {
        TreeNodeBase* node = treeNodes.at(i);
        if (node->getType() != TREE_NODE_TYPE_SCRIPT) {
            continue;
        }
        TreeNodeScript* script = static_cast<TreeNodeScript*>(node);
        if (script->getOrigin() != ScriptOrigin::SCRIPT_ORIGIN_LOCAL) {
            continue;
        }
        QVariantMap resultNode;
        resultNode.insert("name", node->getName());
        resultNode.insert("path", script->getFullPath());
        result.append(resultNode);
    }
    return result;
}

/*@jsdoc
 * Information on a running script.
 * @typedef {object} ScriptDiscoveryService.RunningScript
 * @property {boolean} local - <code>true</code> if the script is a local file (i.e., the scheme is "file"), <code>false</code> 
 *     if it isn't (e.g., the scheme is "http").
 * @property {string} name - The script's file name.
 * @property {string} path - The script's path and file name &mdash; excluding the scheme if a local file.
 * @property {string} url - The full URL of the script &mdash; including the scheme if a local file.
 */
QVariantList ScriptEngines::getRunning() {
    QVariantList result;
    auto runningScripts = getRunningScripts();
    foreach(const QString& runningScript, runningScripts) {
        QUrl runningScriptURL = QUrl(runningScript);
        if (!runningScriptURL.isValid()) {
            runningScriptURL = QUrl::fromLocalFile(runningScriptURL.toDisplayString(QUrl::FormattingOptions(QUrl::FullyEncoded)));
        }
        QVariantMap resultNode;
        resultNode.insert("name", runningScriptURL.fileName());
        QUrl displayURL = expandScriptUrl(runningScriptURL);
        QString displayURLString;
        if (displayURL.isLocalFile()) {
            displayURLString = displayURL.toLocalFile();
        } else {
            displayURLString = displayURL.toDisplayString(QUrl::FormattingOptions(QUrl::FullyEncoded));
        }
        // The path contains the exact path/URL of the script, which also is used in the stopScript function.
        resultNode.insert("path", displayURLString);
        resultNode.insert("url", normalizeScriptURL(runningScript).toString());
        resultNode.insert("local", runningScriptURL.isLocalFile());
        result.append(resultNode);
    }
    return result;
}

void ScriptEngines::loadDefaultScripts() {
    loadScript(DEFAULT_SCRIPTS_LOCATION);
}

void ScriptEngines::loadOneScript(const QString& scriptFilename) {
    loadScript(scriptFilename);
}

void ScriptEngines::loadScripts() {
    // START BACKWARD COMPATIBILITY CODE
    // The following code makes sure people don't lose all their scripts
    // This should be removed after a reasonable ammount of time went by
    // Load old setting format if present
    bool foundDeprecatedSetting = false;
    Settings settings;
    int size = settings.beginReadArray(SETTINGS_KEY);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString string = settings.value("script").toString();
        if (!string.isEmpty()) {
            loadScript(string);
            foundDeprecatedSetting = true;
        }
    }
    settings.endArray();
    if (foundDeprecatedSetting) {
        // Remove old settings found and return
        settings.beginWriteArray(SETTINGS_KEY);
        settings.remove("");
        settings.endArray();
        settings.remove(SETTINGS_KEY + "/size");
        return;
    }
    // END BACKWARD COMPATIBILITY CODE

    // loads all saved scripts
    auto runningScripts = runningScriptsHandle.get();
    bool defaultScriptsOverrideSet = !_defaultScriptsOverride.isEmpty();

    for (auto script : runningScripts) {
        auto url = script.toUrl();
        if (!url.isEmpty()) {
            if (defaultScriptsOverrideSet && url == DEFAULT_SCRIPTS_LOCATION) {
                _defaultScriptsWasRunning = true;
            } else {
                loadScript(url);
            }
        }
    }

    if (defaultScriptsOverrideSet) {
        loadScript(_defaultScriptsOverride, false);
    }
}

void ScriptEngines::saveScripts() {
    // Do not save anything if we are in the process of shutting down
    if (qApp->closingDown()) {
        qWarning() << "Trying to save scripts during shutdown.";
        return;
    }

    // don't save scripts if we started with --scripts, as we would overwrite
    // the scripts that the user expects to be there when launched without the
    // --scripts override.
    if (_defaultScriptsLocationOverridden) {
        runningScriptsHandle.set(QVariantList{ DEFAULT_SCRIPTS_LOCATION });
        return;
    }

    // Saves all currently running user-loaded scripts
    QVariantList list;

    {
        QReadLocker lock(&_scriptManagersHashLock);
        for (auto it = _scriptManagersHash.begin(); it != _scriptManagersHash.end(); ++it) {
            // Save user-loaded scripts, only if they are set to quit when finished
            if (it.value() && it.value()->isUserLoaded()  && !it.value()->isQuitWhenFinished()) {
                auto normalizedUrl = normalizeScriptURL(it.key());
                list.append(normalizedUrl.toString());
            }
        }
    }

    if (_defaultScriptsWasRunning) {
        list.append(DEFAULT_SCRIPTS_LOCATION);
    }

    runningScriptsHandle.set(list);
}

QStringList ScriptEngines::getRunningScripts() {
    QReadLocker lock(&_scriptManagersHashLock);
    QList<QUrl> urls = _scriptManagersHash.keys();
    QStringList result;
    for (auto url : urls) {
        result.append(url.toString());
    }
    return result;
}

void ScriptEngines::stopAllScripts(bool restart) {
    QReadLocker lock(&_scriptManagersHashLock);

    if (_isReloading) {
        return;
    }

    for (QHash<QUrl, ScriptManagerPointer>::const_iterator it = _scriptManagersHash.constBegin();
        it != _scriptManagersHash.constEnd(); it++) {
        ScriptManagerPointer scriptManager = it.value();
        // skip already stopped scripts
        if (scriptManager->isFinished() || scriptManager->isStopping()) {
            continue;
        }

        bool isOverrideScript = it.key().toString().compare(this->_defaultScriptsOverride.toString()) == 0;
        // queue user scripts if restarting
        if (restart && (scriptManager->isUserLoaded() || isOverrideScript)) {
            _isReloading = true;
            ScriptManager::Type type = scriptManager->getType();

            connect(scriptManager.data(), &ScriptManager::finished, this, [this, type, isOverrideScript](QString scriptName) {
                reloadScript(scriptName, !isOverrideScript)->setType(type);
            });
        }

        // stop all scripts
        scriptManager->stop();
    }

    if (restart) {
        qCDebug(scriptengine) << "stopAllScripts -- emitting scriptsReloading";
        QTimer::singleShot(RELOAD_ALL_SCRIPTS_TIMEOUT, this, [&] {
            _isReloading = false;
        });
        emit scriptsReloading();
    }
}

bool ScriptEngines::stopScript(const QString& rawScriptURL, bool restart) {
    bool stoppedScript = false;
    {
        QUrl scriptURL = normalizeScriptURL(QUrl(rawScriptURL));
        if (!scriptURL.isValid()) {
            scriptURL = normalizeScriptURL(QUrl::fromLocalFile(rawScriptURL));
        }

        QReadLocker lock(&_scriptManagersHashLock);
        if (_scriptManagersHash.contains(scriptURL)) {
            ScriptManagerPointer scriptManager = _scriptManagersHash[scriptURL];
            if (restart) {
                bool isUserLoaded = scriptManager->isUserLoaded();
                ScriptManager::Type type = scriptManager->getType();
                auto scriptCache = DependencyManager::get<ScriptCache>();
                scriptCache->deleteScript(scriptURL);

                if (!scriptManager->isStopping()) {
                    connect(scriptManager.data(), &ScriptManager::finished,
                            this, [this, isUserLoaded, type](QString scriptName, ScriptManagerPointer manager) {
                            reloadScript(scriptName, isUserLoaded)->setType(type);
                    });
                }
            }
            scriptManager->stop();
            stoppedScript = true;
        }
    }
    return stoppedScript;
}

void ScriptEngines::reloadLocalFiles() {
    _scriptsModel.reloadLocalFiles();
}

void ScriptEngines::reloadAllScripts() {
    qCDebug(scriptengine) << "reloadAllScripts -- clearing caches";
    DependencyManager::get<ScriptCache>()->clearCache();
    qCDebug(scriptengine) << "reloadAllScripts -- stopping all scripts";
    stopAllScripts(true);
}

ScriptManagerPointer ScriptEngines::loadScript(const QUrl& scriptFilename, bool isUserLoaded, bool loadScriptFromEditor,
                                              bool activateMainWindow, bool reload, bool quitWhenFinished) {
    if (thread() != QThread::currentThread()) {
        ScriptManagerPointer result { nullptr };
        BLOCKING_INVOKE_METHOD(this, "loadScript", Q_RETURN_ARG(ScriptManagerPointer, result),
            Q_ARG(QUrl, scriptFilename),
            Q_ARG(bool, isUserLoaded),
            Q_ARG(bool, loadScriptFromEditor),
            Q_ARG(bool, activateMainWindow),
            Q_ARG(bool, reload),
            Q_ARG(bool, quitWhenFinished));
        return result;
    }
    QUrl scriptUrl;
    if (!scriptFilename.isValid() ||
        (scriptFilename.scheme() != "http" &&
         scriptFilename.scheme() != "https" &&
         scriptFilename.scheme() != "atp" &&
         scriptFilename.scheme() != "file" &&
         scriptFilename.scheme() != "about")) {
        // deal with a "url" like c:/something
        scriptUrl = normalizeScriptURL(QUrl::fromLocalFile(scriptFilename.toString()));
    } else {
        scriptUrl = normalizeScriptURL(scriptFilename);
    }

    scriptUrl = QUrl(FileUtils::selectFile(scriptUrl.toString()));

    auto scriptManager = getScriptEngine(scriptUrl);
    if (scriptManager && !scriptManager->isStopping()) {
        return scriptManager;
    }

    scriptManager = scriptManagerFactory(_context, NO_SCRIPT, "about:" + scriptFilename.fileName());
    scriptManager->setUserLoaded(isUserLoaded);
    scriptManager->setQuitWhenFinished(quitWhenFinished);

    if (scriptFilename.isEmpty() || !scriptUrl.isValid()) {
        launchScriptEngine(scriptManager);
    } else {
        // connect to the appropriate signals of this script engine
        connect(scriptManager.data(), &ScriptManager::scriptLoaded, this, &ScriptEngines::onScriptEngineLoaded);
        connect(scriptManager.data(), &ScriptManager::errorLoadingScript, this, &ScriptEngines::onScriptEngineError);

        // Shutdown Interface when script finishes, if requested
        if (quitWhenFinished) {
            connect(scriptManager.data(), &ScriptManager::finished, this, &ScriptEngines::quitWhenFinished);
        }

        // get the script engine object to load the script at the designated script URL
        scriptManager->loadURL(scriptUrl, reload);
    }

    return scriptManager;
}

ScriptManagerPointer ScriptEngines::getScriptEngine(const QUrl& rawScriptURL) {
    ScriptManagerPointer result;
    {
        QReadLocker lock(&_scriptManagersHashLock);
        const QUrl scriptURL = normalizeScriptURL(rawScriptURL);
        auto it = _scriptManagersHash.find(scriptURL);
        if (it != _scriptManagersHash.end()) {
            result = it.value();
        }
    }
    return result;
}

// FIXME - change to new version of ScriptCache loading notification
void ScriptEngines::onScriptEngineLoaded(const QString& rawScriptURL) {
    UserActivityLogger::getInstance().loadedScript(rawScriptURL);
    ScriptManagerPointer scriptEngine = qobject_cast<ScriptManager*>(sender())->sharedFromThis();

    launchScriptEngine(scriptEngine);

    {
        QWriteLocker lock(&_scriptManagersHashLock);
        QUrl url = QUrl(rawScriptURL);
        QUrl normalized = normalizeScriptURL(url);
        _scriptManagersHash.insertMulti(normalized, scriptEngine);
    }

    // Update settings with new script
    saveScripts();
    emit scriptCountChanged();
}

void ScriptEngines::quitWhenFinished() {
    qApp->quit();
}

int ScriptEngines::runScriptInitializers(ScriptManagerPointer scriptManager) {
    auto nativeCount = DependencyManager::get<ScriptInitializers>()->runScriptInitializers(scriptManager->engine().data());
    return nativeCount + ScriptInitializerMixin<ScriptManagerPointer>::runScriptInitializers(scriptManager);
}

void ScriptEngines::launchScriptEngine(ScriptManagerPointer scriptManager) {
    connect(scriptManager.data(), &ScriptManager::finished, this, &ScriptEngines::onScriptFinished, Qt::DirectConnection);
    connect(scriptManager.data(), &ScriptManager::loadScript, [this](const QString& scriptName, bool userLoaded) {
        loadScript(scriptName, userLoaded);
    });
    connect(scriptManager.data(), &ScriptManager::reloadScript, [this](const QString& scriptName, bool userLoaded) {
        loadScript(scriptName, userLoaded, false, false, true);
    });

    // register our application services and set it off on its own thread
    runScriptInitializers(scriptManager);

    // FIXME disabling 'shift key' debugging for now.  If you start up the application with
    // the shift key held down, it triggers a deadlock because of script interfaces running
    // on the main thread
    auto const wantDebug = scriptManager->isDebuggable(); //  || (qApp->queryKeyboardModifiers() & Qt::ShiftModifier);

    if (HIFI_SCRIPT_DEBUGGABLES && wantDebug) {
        scriptManager->runDebuggable();
    } else {
        scriptManager->runInThread();
    }
}

void ScriptEngines::onScriptFinished(const QString& rawScriptURL, ScriptManagerPointer manager) {
    bool removed = false;
    {
        QWriteLocker lock(&_scriptManagersHashLock);
        const QUrl scriptURL = normalizeScriptURL(QUrl(rawScriptURL));
        for (auto it = _scriptManagersHash.find(scriptURL); it != _scriptManagersHash.end(); ++it) {
            if (it.value() == manager) {
                _scriptManagersHash.erase(it);
                removed = true;
                break;
            }
        }
    }

    removeScriptEngine(manager);

    if (removed && !_isReloading) {
        // Update settings with removed script
        saveScripts();
        emit scriptCountChanged();
    }
}

// FIXME - change to new version of ScriptCache loading notification
void ScriptEngines::onScriptEngineError(const QString& scriptFilename) {
    qCDebug(scriptengine) << "Application::loadScript(), script failed to load...";
    emit scriptLoadError(scriptFilename, "");
}

QString ScriptEngines::getDefaultScriptsLocation() const {
    return PathUtils::defaultScriptsLocation().toString();
}
