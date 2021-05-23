//
//  Created by Bradley Austin Davis on 2016/01/08
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptEngines_h
#define hifi_ScriptEngines_h

#include <functional>
#include <atomic>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>

#include <SettingHandle.h>
#include <DependencyManager.h>
#include <shared/ScriptInitializerMixin.h>
#include "ScriptManager.h"

#include "ScriptsModel.h"
#include "ScriptsModelFilter.h"
#include "ScriptGatekeeper.h"

/*@jsdoc
 * The <code>ScriptDiscoveryService</code> API provides facilities to work with Interface scripts.
 *
 * @namespace ScriptDiscoveryService
 *
 * @hifi-interface
 * @hifi-avatar
 * @hifi-client-entity
 *
 * @property {string} debugScriptUrl="" - The path and name of a script to debug using the "API Debugger" developer tool 
 *     (currentAPI.js). If set, the API Debugger dialog displays the objects and values exposed by the script using 
 *     {@link Script.registerValue} and similar.
 * @property {string} defaultScriptsPath - The path where the default scripts are located in the Interface installation. 
 *     <em>Read-only.</em>
 * @property {ScriptsModel} scriptsModel - Information on the scripts that are in the default scripts directory of the 
 *     Interface installation.
 *     <em>Read-only.</em>
 * @property {ScriptsModelFilter} scriptsModelFilter - Sorted and filtered information on the scripts that are in the default 
 *     scripts directory of the Interface installation.
 *     <em>Read-only.</em>
 */

class ScriptEngines : public QObject, public Dependency, public ScriptInitializerMixin<ScriptManagerPointer> {
    Q_OBJECT

    Q_PROPERTY(ScriptsModel* scriptsModel READ scriptsModel CONSTANT)
    Q_PROPERTY(ScriptsModelFilter* scriptsModelFilter READ scriptsModelFilter CONSTANT)
    Q_PROPERTY(QString debugScriptUrl READ getDebugScriptUrl WRITE setDebugScriptUrl)

public:
    ScriptEngines(ScriptManager::Context context, const QUrl& defaultScriptsOverride = QUrl());
    int runScriptInitializers(ScriptManagerPointer manager) override;

    void loadScripts();
    void saveScripts();

    QString getDebugScriptUrl() { return _debugScriptUrl; };
    void setDebugScriptUrl(const QString& url) { _debugScriptUrl = url; };

    void loadDefaultScripts();
    void reloadLocalFiles();

    QStringList getRunningScripts();
    ScriptManagerPointer getScriptEngine(const QUrl& scriptHash);

    ScriptsModel* scriptsModel() { return &_scriptsModel; };
    ScriptsModelFilter* scriptsModelFilter() { return &_scriptsModelFilter; };

    QString getDefaultScriptsLocation() const;

    /*@jsdoc
     * Starts running an Interface script, if it isn't already running. The script is automatically loaded next time Interface 
     * starts.
     * <p>This is a synonym for calling {@link ScriptDiscoveryService.loadScript|loadScript} with just the script URL.</p>
     * <p class="availableIn"><strong>Supported Script Types:</strong> Interface Scripts &bull; Avatar Scripts</p>
     * <p>See also, {@link Script.load}.</p>
     * @function ScriptDiscoveryService.loadOneScript
     * @param {string} url - The path and name of the script. If a local file, including the <code>"file:///"</code> scheme is 
     *     optional.
     */
    Q_INVOKABLE void loadOneScript(const QString& scriptFilename);

    /*@jsdoc
     * Starts running an Interface script, if it isn't already running.
     * <p class="availableIn"><strong>Supported Script Types:</strong> Interface Scripts &bull; Avatar Scripts</p>
     * <p>See also, {@link Script.load}.</p>
     * @function ScriptDiscoveryService.loadScript
     * @param {string} [url=""] - The path and name of the script. If a local file, including the <code>"file:///"</code> 
     *     scheme is optional.
     * @param {boolean} [isUserLoaded=true] - <code>true</code> if the user specifically loaded it, <code>false</code> if not 
     *     (e.g., a script loaded it). If <code>false</code>, the script is not automatically loaded next time Interface starts.
     * @param {boolean} [loadScriptFromEditor=false] - <em>Not used.</em>
     * @param {boolean} [activateMainWindow=false] - <em>Not used.</em>
     * @param {boolean} [reload=false] - <code>true</code> to redownload the script, <code>false</code> to use the copy from 
     *     the cache if available.
     * @param {boolean} [quitWhenFinished=false] - <code>true</code> to close Interface when the script finishes, 
     *     <code>false</code> to not close Interface.
     * @returns {object} An empty object, <code>{}</code>.
     */
    Q_INVOKABLE ScriptManagerPointer loadScript(const QUrl& scriptFilename = QString(),
        bool isUserLoaded = true, bool loadScriptFromEditor = false, bool activateMainWindow = false, bool reload = false, bool quitWhenFinished = false);

    /*@jsdoc
     * Stops or restarts an Interface script.
     * @function ScriptDiscoveryService.stopScript
     * @param {string} url - The path and name of the script. If a local file, including the <code>"file:///"</code> scheme is 
     *     optional.
     * @param {boolean} [restart=false] -  <code>true</code> to redownload and restart the script, <code>false</code> to stop 
     *     it.
     * @returns {boolean} <code>true</code> if the script was successfully stopped or restarted, <code>false</code> if it 
     *     wasn't (e.g., the script couldn't be found).
     */
    Q_INVOKABLE bool stopScript(const QString& scriptHash, bool restart = false);


    /*@jsdoc
     * Restarts all Interface, avatar, and client entity scripts after clearing the scripts cache.
     * @function ScriptDiscoveryService.reloadAllScripts
     */
    Q_INVOKABLE void reloadAllScripts();

    /*@jsdoc
     * Stops or restarts all Interface scripts. The scripts cache is not cleared. If restarting, avatar and client entity 
     * scripts are also restarted.
     * @function ScriptDiscoveryService.stopAllScripts
     * @param {boolean} [restart=false] - <code>true</code> to restart the scripts, <code>false</code> to stop them.
     */
    Q_INVOKABLE void stopAllScripts(bool restart = false);


    /*@jsdoc
     * Gets a list of all Interface scripts that are currently running.
     * @function ScriptDiscoveryService.getRunning
     * @returns {ScriptDiscoveryService.RunningScript[]} All Interface scripts that are currently running.
     * @example <caption>Report all running scripts.</caption>
     * var runningScripts = ScriptDiscoveryService.getRunning();
     * print("Running scripts:");
     * for (var i = 0; i < runningScripts.length; i++) {
     *     print(JSON.stringify(runningScripts[i]));
     * }
     */
    Q_INVOKABLE QVariantList getRunning();

    /*@jsdoc
     * Gets a list of all script files that are in the default scripts directory of the Interface installation.
     * @function ScriptDiscoveryService.getPublic
     * @returns {ScriptDiscoveryService.PublicScript[]} All scripts in the "scripts" directory of the Interface 
     *     installation.
     */
    Q_INVOKABLE QVariantList getPublic();

    /*@jsdoc
     * @function ScriptDiscoveryService.getLocal
     * @returns {ScriptDiscoveryService.LocalScript[]} Local scripts.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because there is no longer a notion of a "local" scripts folder where you would put your personal scripts.
    Q_INVOKABLE QVariantList getLocal();

    // FIXME: Move to other Q_PROPERTY declarations.
    Q_PROPERTY(QString defaultScriptsPath READ getDefaultScriptsLocation)

    void defaultScriptsLocationOverridden(bool overridden) { _defaultScriptsLocationOverridden = overridden; };

    // Called at shutdown time
    void shutdownScripting();
    bool isStopped() const { return _isStopped; }

    void addScriptEngine(ScriptManagerPointer);
    
    ScriptGatekeeper scriptGatekeeper;

signals:

    /*@jsdoc
     * Triggered when the number of Interface scripts running changes.
     * @function ScriptDiscoveryService.scriptCountChanged
     * @returns {Signal}
     * @example <caption>Report when the number of running scripts changes.</caption>
     * ScriptDiscoveryService.scriptCountChanged.connect(function () {
     *     print("Scripts count changed: " + ScriptDiscoveryService.getRunning().length);
     * });
     */
    void scriptCountChanged();

    /*@jsdoc
     * Triggered when Interface, avatar, and client entity scripts are restarting as a result of
     * {@link ScriptDiscoveryService.reloadAllScripts|reloadAllScripts} or 
     * {@link ScriptDiscoveryService.stopAllScripts|stopAllScripts}.
     * @function ScriptDiscoveryService.scriptsReloading
     * @returns {Signal}
     */
    void scriptsReloading();

    /*@jsdoc
     * Triggered when a script could not be loaded.
     * @function ScriptDiscoveryService.scriptLoadError
     * @param {string} url - The path and name of the script that could not be loaded.
     * @param {string} error - <code>""</code> always.
     * @returns {Signal}
     */
    void scriptLoadError(const QString& filename, const QString& error);

    /*@jsdoc
     * Triggered when any script prints a message to the program log via {@link  print}, {@link Script.print},
     * {@link console.log}, or {@link console.debug}.
     * @function ScriptDiscoveryService.printedMessage
     * @param {string} message - The message.
     * @param {string} scriptName - The name of the script that generated the message.
     * @returns {Signal}
     */
    void printedMessage(const QString& message, const QString& engineName);

    /*@jsdoc
     * Triggered when any script generates an error or {@link console.error} is called.
     * @function ScriptDiscoveryService.errorMessage
     * @param {string} message - The error message.
     * @param {string} scriptName - The name of the script that generated the error message.
     * @returns {Signal}
     */
    void errorMessage(const QString& message, const QString& engineName);

    /*@jsdoc
     * Triggered when any script generates a warning or {@link console.warn} is called.
     * @function ScriptDiscoveryService.warningMessage
     * @param {string} message - The warning message.
     * @param {string} scriptName - The name of the script that generated the warning message.
     * @returns {Signal}
     */
    void warningMessage(const QString& message, const QString& engineName);

    /*@jsdoc
     * Triggered when any script generates an information message or {@link console.info} is called.
     * @function ScriptDiscoveryService.infoMessage
     * @param {string} message - The information message.
     * @param {string} scriptName - The name of the script that generated the informaton message.
     * @returns {Signal}
     */
    void infoMessage(const QString& message, const QString& engineName);

    /*@jsdoc
     * @function ScriptDiscoveryService.errorLoadingScript
     * @param {string} url - URL.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    // Deprecated because never emitted.
    void errorLoadingScript(const QString& url);

    /*@jsdoc
     * Triggered when the Debug Window is cleared.
     * @function ScriptDiscoveryService.clearDebugWindow
     * @returns {Signal}
     */
    void clearDebugWindow();

public slots:

    /*@jsdoc
     * @function ScriptDiscoveryService.onPrintedMessage
     * @param {string} message - Message.
     * @param {string} scriptName - Script name.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal.
    void onPrintedMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * @function ScriptDiscoveryService.onErrorMessage
     * @param {string} message - Message.
     * @param {string} scriptName - Script name.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal.
    void onErrorMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * @function ScriptDiscoveryService.onWarningMessage
     * @param {string} message - Message.
     * @param {string} scriptName - Script name.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal.
    void onWarningMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * @function ScriptDiscoveryService.onInfoMessage
     * @param {string} message - Message.
     * @param {string} scriptName - Script name.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal.
    void onInfoMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * @function ScriptDiscoveryService.onErrorLoadingScript
     * @param {string} url - URL.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal. And it isn't used.
    void onErrorLoadingScript(const QString& url);

    /*@jsdoc
     * @function ScriptDiscoveryService.onClearDebugWindow
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because only use is to emit a signal.
    void onClearDebugWindow();

protected slots:

    /*@jsdoc
     * @function ScriptDiscoveryService.onScriptFinished
     * @param {string} scriptName - Script name.
     * @param {object} manager - Script manager.
     * @deprecated This function is deprecated and will be removed.
     */
    // Deprecated because it wasn't intended to be in the API.
    void onScriptFinished(const QString& fileNameString, ScriptManagerPointer manager);

protected:
    ScriptManagerPointer reloadScript(const QString& scriptName, bool isUserLoaded = true) { return loadScript(scriptName, isUserLoaded, false, false, true); }
    void removeScriptEngine(ScriptManagerPointer);
    void onScriptEngineLoaded(const QString& scriptFilename);
    void quitWhenFinished();
    void onScriptEngineError(const QString& scriptFilename);
    void launchScriptEngine(ScriptManagerPointer);

    ScriptManager::Context _context;
    QReadWriteLock _scriptManagersHashLock;
    QHash<QUrl, ScriptManagerPointer> _scriptManagersHash;
    QSet<ScriptManagerPointer> _allKnownScriptManagers;
    QMutex _allScriptsMutex;
    ScriptsModel _scriptsModel;
    ScriptsModelFilter _scriptsModelFilter;
    std::atomic<bool> _isStopped { false };
    std::atomic<bool> _isReloading { false };
    bool _defaultScriptsLocationOverridden { false };
    QString _debugScriptUrl;

    // If this is set, defaultScripts.js will not be run if it is in the settings,
    // and this will be run instead. This script will not be persisted to settings.
    const QUrl _defaultScriptsOverride { };
    // If an override is set, this will be true if defaultScripts.js was previously running.
    bool _defaultScriptsWasRunning { false };
};

QUrl normalizeScriptURL(const QUrl& rawScriptURL);
QString expandScriptPath(const QString& rawPath);
QUrl expandScriptUrl(const QUrl& rawScriptURL);

#endif // hifi_ScriptEngine_h
