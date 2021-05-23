//
//  ScriptManager.h
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 12/14/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptManager_h
#define hifi_ScriptManager_h

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

#include <QtCore/QEnableSharedFromThis>
#include <QtCore/QFuture>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include <AnimVariant.h>
#include "EntityItemID.h"
#include "EntitiesScriptEngineProvider.h"
#include <EntityScriptUtils.h>
#include <ExternalResource.h>
#include <SettingHandle.h>

#include "AssetScriptingInterface.h"
#include "ConsoleScriptingInterface.h"
#include "Mat4.h"
#include "PointerEvent.h"
#include "Quat.h"
#include "ScriptUUID.h"
#include "Vec3.h"

class QScriptEngineDebugger;

static const QString NO_SCRIPT("");

static const int SCRIPT_FPS = 60;
static const int DEFAULT_MAX_ENTITY_PPS = 9000;
static const int DEFAULT_ENTITY_PPS_PER_SCRIPT = 900;

class ScriptEngine;
class ScriptEngines;
class ScriptManager;
class ScriptValue;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;
using ScriptValueList = QList<ScriptValuePointer>;

Q_DECLARE_METATYPE(ScriptManagerPointer)

class CallbackData {
public:
    ScriptValuePointer function;
    EntityItemID definingEntityIdentifier;
    QUrl definingSandboxURL;
};

class DeferredLoadEntity {
public:
    EntityItemID entityID;
    QString entityScript;
    //bool forceRedownload;
};

struct EntityScriptContentAvailable {
    EntityItemID entityID;
    QString scriptOrURL;
    QString contents;
    bool isURL;
    bool success;
    QString status;
};

typedef std::unordered_map<EntityItemID, EntityScriptContentAvailable> EntityScriptContentAvailableMap;

typedef QList<CallbackData> CallbackList;
typedef QHash<QString, CallbackList> RegisteredEventHandlers;

class EntityScriptDetails {
public:
    EntityScriptStatus status { EntityScriptStatus::PENDING };

    // If status indicates an error, this contains a human-readable string giving more information about the error.
    QString errorInfo { "" };

    QString scriptText { "" };
    ScriptValuePointer scriptObject{ ScriptValuePointer() };
    int64_t lastModified { 0 };
    QUrl definingSandboxURL { QUrl("about:EntityScript") };
};

/*@jsdoc
 * The <code>Script</code> API provides facilities for working with scripts.
 *
 * @namespace Script
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {string} context - The context that the script is running in:
 *     <ul>
 *       <li><code>"client"</code>: An Interface or avatar script.</li>
 *       <li><code>"entity_client"</code>: A client entity script.</li>
 *       <li><code>"entity_server"</code>: A server entity script.</li>
 *       <li><code>"agent"</code>: An assignment client script.</li>
 *     </ul>
 *     <em>Read-only.</em>
 * @property {string} type - The type of script that is running:
 *     <ul>
 *       <li><code>"client"</code>: An Interface script.</li>
 *       <li><code>"entity_client"</code>: A client entity script.</li>
 *       <li><code>"avatar"</code>: An avatar script.</li>
 *       <li><code>"entity_server"</code>: A server entity script.</li>
 *       <li><code>"agent"</code>: An assignment client script.</li>
 *     </ul>
 *     <em>Read-only.</em>
 * @property {string} filename - The filename of the script file.
 *     <em>Read-only.</em>
 * @property {Script.ResourceBuckets} ExternalPaths - External resource buckets.
 */
class ScriptManager : public QObject, public EntitiesScriptEngineProvider, public QEnableSharedFromThis<ScriptManager> {
    Q_OBJECT
    Q_PROPERTY(QString context READ getContext)
    Q_PROPERTY(QString type READ getTypeAsString)
    Q_PROPERTY(QString fileName MEMBER _fileNameString CONSTANT)
public:
    static const QString SCRIPT_EXCEPTION_FORMAT;
    static const QString SCRIPT_BACKTRACE_SEP;

    enum Context {
        CLIENT_SCRIPT,
        ENTITY_CLIENT_SCRIPT,
        ENTITY_SERVER_SCRIPT,
        AGENT_SCRIPT
    };

    enum Type {
        CLIENT,
        ENTITY_CLIENT,
        ENTITY_SERVER,
        AGENT,
        AVATAR
    };
    Q_ENUM(Type)

    static int processLevelMaxRetries;
    ScriptManager(Context context, const QString& scriptContents = NO_SCRIPT, const QString& fileNameString = QString("about:ScriptEngine"));
    ~ScriptManager();

    /// run the script in a dedicated thread. This will have the side effect of evalulating
    /// the current script contents and calling run(). Callers will likely want to register the script with external
    /// services before calling this.
    void runInThread();

    void runDebuggable();

    /// run the script in the callers thread, exit when stop() is called.
    void run();

    QString getFilename() const;

    inline ScriptEnginePointer engine() { return _engine; }

    QList<EntityItemID> getListOfEntityScriptIDs();

    bool isStopped() const;

    /*@jsdoc
     * Stops and unloads the current script.
     * <p><strong>Warning:</strong> If an assignment client script, the script gets restarted after stopping.</p>
     * @function Script.stop
     * @param {boolean} [marshal=false] - Marshal.
     *     <p class="important">Deprecated: This parameter is deprecated and will be removed.</p>
     * @example <caption>Stop a script after 5s.</caption>
     * Script.setInterval(function () {
     *     print("Hello");
     * }, 1000);
     * 
     * Script.setTimeout(function () {
     *     Script.stop(true);
     * }, 5000);
     */
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOTE - this is intended to be a public interface for Agent scripts, and local scripts, but not for EntityScripts
    Q_INVOKABLE void stop(bool marshal = false);

    // Stop any evaluating scripts and wait for the scripting thread to finish.
    void waitTillDoneRunning(bool shutdown = false);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOTE - these are NOT intended to be public interfaces available to scripts, the are only Q_INVOKABLE so we can
    //        properly ensure they are only called on the correct thread

    /// if the script engine is not already running, this will download the URL and start the process of seting it up
    /// to run... NOTE - this is used by Application currently to load the url. We don't really want it to be exposed
    /// to scripts. we may not need this to be invokable
    void loadURL(const QUrl& scriptURL, bool reload);
    bool hasValidScriptSuffix(const QString& scriptFileName);

    /*@jsdoc
     * Gets the context that the script is running in: Interface/avatar, client entity, server entity, or assignment client.
     * @function Script.getContext
     * @returns {string} The context that the script is running in:
     * <ul>
     *   <li><code>"client"</code>: An Interface or avatar script.</li>
     *   <li><code>"entity_client"</code>: A client entity script.</li>
     *   <li><code>"entity_server"</code>: A server entity script.</li>
     *   <li><code>"agent"</code>: An assignment client script.</li>
     * </ul>
     */
    Q_INVOKABLE QString getContext() const;

    /*@jsdoc
     * Checks whether the script is running as an Interface or avatar script.
     * @function Script.isClientScript
     * @returns {boolean} <code>true</code> if the script is running as an Interface or avatar script, <code>false</code> if it 
     *     isn't.
     */
    Q_INVOKABLE bool isClientScript() const { return _context == CLIENT_SCRIPT; }

    /*@jsdoc
     * Checks whether the application was compiled as a debug build.
     * @function Script.isDebugMode
     * @returns {boolean} <code>true</code> if the application was compiled as a debug build, <code>false</code> if it was 
     *     compiled as a release build.
     */
    Q_INVOKABLE bool isDebugMode() const;

    /*@jsdoc
     * Checks whether the script is running as a client entity script.
     * @function Script.isEntityClientScript
     * @returns {boolean} <code>true</code> if the script is running as a client entity script, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isEntityClientScript() const { return _context == ENTITY_CLIENT_SCRIPT; }

    /*@jsdoc
     * Checks whether the script is running as a server entity script.
     * @function Script.isEntityServerScript
     * @returns {boolean} <code>true</code> if the script is running as a server entity script, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isEntityServerScript() const { return _context == ENTITY_SERVER_SCRIPT; }

    /*@jsdoc
     * Checks whether the script is running as an assignment client script.
     * @function Script.isAgentScript
     * @returns {boolean} <code>true</code> if the script is running as an assignment client script, <code>false</code> if it 
     *     isn't.
     */
    Q_INVOKABLE bool isAgentScript() const { return _context == AGENT_SCRIPT; }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOTE - these are intended to be public interfaces available to scripts

    /*@jsdoc
     * @function Script.formatExecption
     * @param {object} exception - Exception.
     * @param {boolean} inludeExtendeDetails - Include extended details.
     * @returns {string} String.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE QString formatException(const ScriptValuePointer& exception, bool includeExtendedDetails);

    /*@jsdoc
     * Adds a function to the list of functions called when a particular event occurs on a particular entity.
     * <p>See also, the {@link Entities} API.</p>
     * @function Script.addEventHandler
     * @param {Uuid} entityID - The ID of the entity.
     * @param {Script.EntityEvent} eventName - The name of the event.
     * @param {Script~entityEventCallback|Script~pointerEventCallback|Script~collisionEventCallback} handler - The function to 
     *     call when the event occurs on the entity. It can be either the name of a function or an in-line definition.
     * @example <caption>Report when a mouse press occurs on a particular entity.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * function reportMousePress(entityID, event) {
     *     print("Mouse pressed on entity: " + JSON.stringify(event));
     * }
     * 
     * Script.addEventHandler(entityID, "mousePressOnEntity", reportMousePress);
     */
    Q_INVOKABLE void addEventHandler(const EntityItemID& entityID, const QString& eventName, ScriptValuePointer handler);

    /*@jsdoc
     * Removes a function from the list of functions called when an entity event occurs on a particular entity.
     * <p>See also, the {@link Entities} API.</p>
     * @function Script.removeEventHandler
     * @param {Uuid} entityID - The ID of the entity.
     * @param {Script.EntityEvent} eventName - The name of the entity event.
     * @param {function} handler - The name of the function to no longer call when the entity event occurs on the entity.
     */
    Q_INVOKABLE void removeEventHandler(const EntityItemID& entityID, const QString& eventName, ScriptValuePointer handler);

    /*@jsdoc
     * Starts running another script in Interface, if it isn't already running. The script is not automatically loaded next 
     * time Interface starts.
     * <p class="availableIn"><strong>Supported Script Types:</strong> Interface Scripts &bull; Avatar Scripts</p>
     * <p>See also, {@link ScriptDiscoveryService.loadScript}.</p>
     * @function Script.load
     * @param {string} filename - The URL of the script to load. This can be relative to the current script's URL.
     * @example <caption>Load a script from another script.</caption>
     * // First file: scriptA.js
     * print("This is script A");
     *
     * // Second file: scriptB.js
     * print("This is script B");
     * Script.load("scriptA.js");
     *
     * // If you run scriptB.js you should see both scripts in the Running Scripts dialog.
     * // And you should see the following output:
     * // This is script B
     * // This is script A
     */
    Q_INVOKABLE void load(const QString& loadfile);

    /*@jsdoc
     * Includes JavaScript from other files in the current script. If a callback is specified, the files are loaded and 
     * included asynchronously, otherwise they are included synchronously (i.e., script execution blocks while the files are 
     * included).
     * @function Script.include
     * @variation 0
     * @param {string[]} filenames - The URLs of the scripts to include. Each can be relative to the current script.
     * @param {function} [callback=null] - The function to call back when the scripts have been included. It can be either the 
     *     name of a function or an in-line definition.
     */
    Q_INVOKABLE void include(const QStringList& includeFiles, ScriptValuePointer callback = ScriptValuePointer());

    /*@jsdoc
     * Includes JavaScript from another file in the current script. If a callback is specified, the file is loaded and included
     * asynchronously, otherwise it is included synchronously (i.e., script execution blocks while the file is included).
     * @function Script.include
     * @param {string} filename - The URL of the script to include. It can be relative to the current script.
     * @param {function} [callback=null] - The function to call back when the script has been included. It can be either the 
     *     name of a function or an in-line definition.
     * @example <caption>Include a script file asynchronously.</caption>
     * // First file: scriptA.js
     * print("This is script A");
     *
     * // Second file: scriptB.js
     * print("This is script B");
     * Script.include("scriptA.js", function () {
     *     print("Script A has been included");
     * });
     *
     * // If you run scriptB.js you should see only scriptB.js in the running scripts list.
     * // And you should see the following output:
     * // This is script B
     * // This is script A
     * // Script A has been included
     */
    Q_INVOKABLE void include(const QString& includeFile, ScriptValuePointer callback = ScriptValuePointer());

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // MODULE related methods

    /*@jsdoc
     * Provides access to methods or objects provided in an external JavaScript or JSON file. 
     * See {@link https://docs.vircadia.dev/script/js-tips.html} for further details.
     * @function Script.require
     * @param {string} module - The module to use. May be a JavaScript file, a JSON file, or the name of a system module such 
     *     as <code>"appUi"</code> (i.e., the "appUi.js" system module JavaScript file).
     * @returns {object|array} The value assigned to <code>module.exports</code> in the JavaScript file, or the value defined 
     *     in the JSON file.
     */
    Q_INVOKABLE ScriptValuePointer require(const QString& moduleId);

    /*@jsdoc
     * @function Script.resetModuleCache
     * @param {boolean} [deleteScriptCache=false] - Delete script cache.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void resetModuleCache(bool deleteScriptCache = false);

    ScriptValuePointer currentModule();
    bool registerModuleWithParent(const ScriptValuePointer& module, const ScriptValuePointer& parent);
    ScriptValuePointer newModule(const QString& modulePath, const ScriptValuePointer& parent = ScriptValuePointer());
    QVariantMap fetchModuleSource(const QString& modulePath, const bool forceDownload = false);
    ScriptValuePointer instantiateModule(const ScriptValuePointer& module, const QString& sourceCode);

    Q_INVOKABLE ScriptValuePointer evaluate(const QString& program, const QString& fileName = QString());

    /*@jsdoc
     * Calls a function repeatedly, at a set interval.
     * @function Script.setInterval
     * @param {function} function - The function to call. This can be either the name of a function or an in-line definition.
     * @param {number} interval - The interval at which to call the function, in ms.
     * @returns {object} A handle to the interval timer. This can be used in {@link Script.clearInterval}.
     * @example <caption>Print a message every second.</caption>
     * Script.setInterval(function () {
     *     print("Interval timer fired");
     * }, 1000);
    */
    Q_INVOKABLE QObject* setInterval(const ScriptValuePointer& function, int intervalMS);

    /*@jsdoc
     * Calls a function once, after a delay.
     * @function Script.setTimeout
     * @param {function} function - The function to call. This can be either the name of a function or an in-line definition.
     * @param {number} timeout - The delay after which to call the function, in ms.
     * @returns {object} A handle to the timeout timer. This can be used in {@link Script.clearTimeout}.
     * @example <caption>Print a message once, after a second.</caption>
     * Script.setTimeout(function () {
     *     print("Timeout timer fired");
     * }, 1000);
     */
    Q_INVOKABLE QObject* setTimeout(const ScriptValuePointer& function, int timeoutMS);

    /*@jsdoc
     * Stops an interval timer set by {@link Script.setInterval|setInterval}.
     * @function Script.clearInterval
     * @param {object} timer - The interval timer to stop.
     * @example <caption>Stop an interval timer.</caption>
     * // Print a message every second.
     * var timer = Script.setInterval(function () {
     *     print("Interval timer fired");
     * }, 1000);
     *
     * // Stop the timer after 10 seconds.
     * Script.setTimeout(function () {
     *     print("Stop interval timer");
     *     Script.clearInterval(timer);
     * }, 10000);
     */
    Q_INVOKABLE void clearInterval(QObject* timer) { stopTimer(reinterpret_cast<QTimer*>(timer)); }

    /*@jsdoc
     * Stops a timeout timer set by {@link Script.setTimeout|setTimeout}.
     * @function Script.clearTimeout
     * @param {object} timer - The timeout timer to stop.
     * @example <caption>Stop a timeout timer.</caption>
     * // Print a message after two seconds.
     * var timer = Script.setTimeout(function () {
     *     print("Timer fired");
     * }, 2000);
     *
     * // Uncomment the following line to stop the timer from firing.
     * //Script.clearTimeout(timer);
     */
    Q_INVOKABLE void clearTimeout(QObject* timer) { stopTimer(reinterpret_cast<QTimer*>(timer)); }

    /*@jsdoc
     * Prints a message to the program log and emits {@link Script.printedMessage}.
     * <p>Alternatively, you can use {@link print} or one of the {@link console} API methods.</p>
     * @function Script.print
     * @param {string} message - The message to print.
     */
    Q_INVOKABLE void print(const QString& message);

    /*@jsdoc
     * Resolves a relative path to an absolute path. The relative path is relative to the script's location.
     * @function Script.resolvePath
     * @param {string} path - The relative path to resolve.
     * @returns {string} The absolute path.
     * @example <caption>Report the directory and filename of the running script.</caption>
     * print(Script.resolvePath(""));
     * @example <caption>Report the directory of the running script.</caption>
     * print(Script.resolvePath("."));
     * @example <caption>Report the path to a file located relative to the running script.</caption>
     * print(Script.resolvePath("../assets/sounds/hello.wav"));
     */
    Q_INVOKABLE QUrl resolvePath(const QString& path) const;

    /*@jsdoc
     * Gets the path to the resources directory for QML files.
     * @function Script.resourcesPath
     * @returns {string} The path to the resources directory for QML files.
     */
    Q_INVOKABLE QUrl resourcesPath() const;

    /*@jsdoc
     * Starts timing a section of code in order to send usage data about it to Vircadia. Shouldn't be used outside of the 
     * standard scripts.
     * @function Script.beginProfileRange
     * @param {string} label - A name that identifies the section of code.
     */
    Q_INVOKABLE void beginProfileRange(const QString& label) const;

    /*@jsdoc
     * Finishes timing a section of code in order to send usage data about it to Vircadia. Shouldn't be used outside of 
     * the standard scripts.
     * @function Script.endProfileRange
     * @param {string} label - A name that identifies the section of code.
     */
    Q_INVOKABLE void endProfileRange(const QString& label) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Entity Script Related methods

    /*@jsdoc
     * Checks whether an entity has an entity script running.
     * @function Script.isEntityScriptRunning
     * @param {Uuid} entityID - The ID of the entity.
     * @returns {boolean} <code>true</code> if the entity has an entity script running, <code>false</code> if it doesn't.
     */
    Q_INVOKABLE bool isEntityScriptRunning(const EntityItemID& entityID) {
        QReadLocker locker { &_entityScriptsLock };
        auto it = _entityScripts.constFind(entityID);
        return it != _entityScripts.constEnd() && it->status == EntityScriptStatus::RUNNING;
    }
    QVariant cloneEntityScriptDetails(const EntityItemID& entityID);
    QFuture<QVariant> getLocalEntityScriptDetails(const EntityItemID& entityID) override;

    /*@jsdoc
     * @function Script.loadEntityScript
     * @param {Uuid} entityID - Entity ID.
     * @param {string} script - Script.
     * @param {boolean} forceRedownload - Force re-download.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void loadEntityScript(const EntityItemID& entityID, const QString& entityScript, bool forceRedownload);

    /*@jsdoc
     * @function Script.unloadEntityScript
     * @param {Uuid} entityID - Entity ID.
     * @param {boolean} [shouldRemoveFromMap=false] - Should remove from map.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void unloadEntityScript(const EntityItemID& entityID, bool shouldRemoveFromMap = false); // will call unload method

    /*@jsdoc
     * @function Script.unloadAllEntityScripts
     * @param {boolean} [blockingCall=false] - Wait for completion if call moved to another thread.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void unloadAllEntityScripts(bool blockingCall = false);

    /*@jsdoc
     * Calls a method in an entity script.
     * @function Script.callEntityScriptMethod
     * @param {Uuid} entityID - The ID of the entity running the entity script.
     * @param {string} methodName - The name of the method to call.
     * @param {string[]} [parameters=[]] - The parameters to call the specified method with.
     * @param {Uuid} [remoteCallerID=Uuid.NULL] - An ID that identifies the caller.
     */
    Q_INVOKABLE void callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName,
                                            const QStringList& params = QStringList(),
                                            const QUuid& remoteCallerID = QUuid()) override;

    /*@jsdoc
     * Calls a method in an entity script.
     * @function Script.callEntityScriptMethod
     * @param {Uuid} entityID - Entity ID.
     * @param {string} methodName - Method name.
     * @param {PointerEvent} event - Pointer event.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName, const PointerEvent& event);

    /*@jsdoc
     * Calls a method in an entity script.
     * @function Script.callEntityScriptMethod
     * @param {Uuid} entityID - Entity ID.
     * @param {string} methodName - Method name.
     * @param {Uuid} otherID - Other entity ID.
     * @param {Collision} collision - Collision.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName, const EntityItemID& otherID, const Collision& collision);

    /*@jsdoc
     * @function Script.generateUUID
     * @returns {Uuid} A new UUID.
     * @deprecated This function is deprecated and will be removed. Use {@link Uuid(0).generate|Uuid.generate} instead.
     */
    Q_INVOKABLE QUuid generateUUID() { return QUuid::createUuid(); }

    void setType(Type type) { _type = type; };
    Type getType() { return _type; };
    QString getTypeAsString() const;

    bool isFinished() const { return _isFinished; } // used by Application and ScriptWidget
    bool isRunning() const { return _isRunning; } // used by ScriptWidget

    // this is used by code in ScriptEngines.cpp during the "reload all" operation
    bool isStopping() const { return _isStopping; }

    bool isDebuggable() const { return _debuggable; }

    void disconnectNonEssentialSignals();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // These are currently used by Application to track if a script is user loaded or not. Consider finding a solution
    // inside of Application so that the ScriptManager class is not polluted by this notion
    void setUserLoaded(bool isUserLoaded) { _isUserLoaded = isUserLoaded; }
    bool isUserLoaded() const { return _isUserLoaded; }

    void setQuitWhenFinished(const bool quitWhenFinished) { _quitWhenFinished = quitWhenFinished; }
    bool isQuitWhenFinished() const { return _quitWhenFinished; }

    void setEmitScriptUpdatesFunction(std::function<bool()> func) { _emitScriptUpdates = func; }

    void scriptErrorMessage(const QString& message);
    void scriptWarningMessage(const QString& message);
    void scriptInfoMessage(const QString& message);
    void scriptPrintedMessage(const QString& message);
    void clearDebugLogWindow();
    int getNumRunningEntityScripts() const;
    bool getEntityScriptDetails(const EntityItemID& entityID, EntityScriptDetails &details) const;
    bool hasEntityScriptDetails(const EntityItemID& entityID) const;

    void setScriptEngines(QSharedPointer<ScriptEngines>& scriptEngines) { _scriptEngines = scriptEngines; }

    /*@jsdoc
     * Gets the URL for an asset in an external resource bucket. (The location where the bucket is hosted may change over time
     * but this method will return the asset's current URL.)
     * @function Script.getExternalPath
     * @param {Script.ResourceBucket} bucket - The external resource bucket that the asset is in.
     * @param {string} path - The path within the external resource bucket where the asset is located. 
     *     <p>Normally, this should start with a path or filename to be appended to the bucket URL.
     *     Alternatively, it can be a relative path starting with <code>./</code> or <code>../</code>, to navigate within the 
     *     resource bucket's URL.</p>
     * @Returns {string} The URL of an external asset.
     * @example <caption>Report the URL of a default particle.</caption>
     * print(Script.getExternalPath(Script.ExternalPaths.Assets, "Bazaar/Assets/Textures/Defaults/Interface/default_particle.png"));
     * @example <caption>Report the root directory where the Vircadia assets are located.</caption>
     * print(Script.getExternalPath(Script.ExternalPaths.Assets, "."));
     */
    Q_INVOKABLE QString getExternalPath(ExternalResource::Bucket bucket, const QString& path);

public slots:

    /*@jsdoc
     * @function Script.callAnimationStateHandler
     * @param {function} callback - Callback function.
     * @param {object} parameters - Parameters.
     * @param {string[]} names - Names.
     * @param {boolean} useNames - Use names.
     * @param {function} resultHandler - Result handler.
     * @deprecated This function is deprecated and will be removed.
     */
    void callAnimationStateHandler(ScriptValuePointer callback, AnimVariantMap parameters, QStringList names, bool useNames, AnimVariantResultHandler resultHandler);

    /*@jsdoc
     * @function Script.updateMemoryCost
     * @param {number} deltaSize - Delta size.
     * @deprecated This function is deprecated and will be removed.
     */
    void updateMemoryCost(const qint64&);

signals:

    /*@jsdoc
     * @function Script.scriptLoaded
     * @param {string} filename - File name.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void scriptLoaded(const QString& scriptFilename);

    /*@jsdoc
     * @function Script.errorLoadingScript
     * @param {string} filename - File name.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void errorLoadingScript(const QString& scriptFilename);

    /*@jsdoc
     * Triggered frequently at a system-determined interval.
     * @function Script.update
     * @param {number} deltaTime - The time since the last update, in s.
     * @returns {Signal}
     * @example <caption>Report script update intervals.</caption>
     * Script.update.connect(function (deltaTime) {
     *     print("Update: " + deltaTime);
     * });
     */
    void update(float deltaTime);

    /*@jsdoc
     * Triggered when the script is stopping.
     * @function Script.scriptEnding
     * @returns {Signal}
     * @example <caption>Report when a script is stopping.</caption>
     * print("Script started");
     *
     * Script.scriptEnding.connect(function () {
     *     print("Script ending");
     * });
     *
     * Script.setTimeout(function () {
     *     print("Stopping script");
     *     Script.stop();
     * }, 1000);
     */
    void scriptEnding();

    /*@jsdoc
     * @function Script.finished
     * @param {string} filename - File name.
     * @param {object} engine - Engine.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void finished(const QString& fileNameString, ScriptManagerPointer);

    /*@jsdoc
     * @function Script.cleanupMenuItem
     * @param {string} menuItem - Menu item.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void cleanupMenuItem(const QString& menuItemString);

    /*@jsdoc
     * Triggered when the script prints a message to the program log via {@link  print}, {@link Script.print}, 
     * {@link console.log}, {@link console.debug}, {@link console.group}, {@link console.groupEnd}, {@link console.time}, or 
     * {@link console.timeEnd}.
     * @function Script.printedMessage
     * @param {string} message - The message.
     * @param {string} scriptName - The name of the script that generated the message.
     * @returns {Signal}
     */
    void printedMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * Triggered when the script generates an error, {@link console.error} or {@link console.exception} is called, or 
     * {@link console.assert} is called and fails.
     * @function Script.errorMessage
     * @param {string} message - The error message.
     * @param {string} scriptName - The name of the script that generated the error message.
     * @returns {Signal}
     */
    void errorMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * Triggered when the script generates a warning or {@link console.warn} is called.
     * @function Script.warningMessage
     * @param {string} message - The warning message.
     * @param {string} scriptName - The name of the script that generated the warning message.
     * @returns {Signal}
     */
    void warningMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * Triggered when the script generates an information message or {@link console.info} is called.
     * @function Script.infoMessage
     * @param {string} message - The information message.
     * @param {string} scriptName - The name of the script that generated the information message.
     * @returns {Signal}
     */
    void infoMessage(const QString& message, const QString& scriptName);

    /*@jsdoc
     * Triggered when the running state of the script changes, e.g., from running to stopping.
     * @function Script.runningStateChanged
     * @returns {Signal}
     */
    void runningStateChanged();

    /*@jsdoc
     * @function Script.clearDebugWindow
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void clearDebugWindow();

    /*@jsdoc
     * @function Script.loadScript
     * @param {string} scriptName - Script name.
     * @param {boolean} isUserLoaded - Is user loaded.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void loadScript(const QString& scriptName, bool isUserLoaded);

    /*@jsdoc
     * @function Script.reloadScript
     * @param {string} scriptName - Script name.
     * @param {boolean} isUserLoaded - Is user loaded.
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void reloadScript(const QString& scriptName, bool isUserLoaded);

    /*@jsdoc
     * Triggered when the script has stopped.
     * @function Script.doneRunning
     * @returns {Signal}
     */
    void doneRunning();

    /*@jsdoc
     * @function Script.entityScriptDetailsUpdated
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    // Emitted when an entity script is added or removed, or when the status of an entity
    // script is updated (goes from RUNNING to ERROR_RUNNING_SCRIPT, for example)
    void entityScriptDetailsUpdated();

    /*@jsdoc
     * Triggered when the script starts for the user. See also, {@link Entities.preload}.
     * <p class="availableIn"><strong>Supported Script Types:</strong> Client Entity Scripts &bull; Server Entity Scripts</p>
     * @function Script.entityScriptPreloadFinished
     * @param {Uuid} entityID - The ID of the entity that the script is running in.
     * @returns {Signal}
     * @example <caption>Get the ID of the entity that a client entity script is running in.</caption>
     * var entityScript = function () {
     *     this.entityID = Uuid.NULL;
     * };
     *
     * Script.entityScriptPreloadFinished.connect(function (entityID) {
     *     this.entityID = entityID;
     *     print("Entity ID: " + this.entityID);
     * });
     *
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     color: { red: 255, green: 0, blue: 0 },
     *     script: "(" + entityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     */
    // Emitted when an entity script has finished running preload
    void entityScriptPreloadFinished(const EntityItemID& entityID);

    /*@jsdoc
     * Triggered when a script generates an unhandled exception.
     * @function Script.unhandledException
     * @param {object} exception - The details of the exception.
     * @returns {Signal}
     * @example <caption>Report the details of an unhandled exception.</caption>
     * Script.unhandledException.connect(function (exception) {
     *     print("Unhandled exception: " + JSON.stringify(exception));
     * });
     * var properties = JSON.parse("{ x: 1"); // Invalid JSON string.
     */
    void unhandledException(const ScriptValuePointer& exception);

protected:
    void init();

    /*@jsdoc
     * @function Script.executeOnScriptThread
     * @param {function} function - Function.
     * @param {ConnectionType} [type=2] - Connection type.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void executeOnScriptThread(std::function<void()> function, const Qt::ConnectionType& type = Qt::QueuedConnection );

    /*@jsdoc
     * @function Script._requireResolve
     * @param {string} module - Module.
     * @param {string} [relativeTo=""] - Relative to.
     * @returns {string} Result.
     * @deprecated This function is deprecated and will be removed.
     */
    // note: this is not meant to be called directly, but just to have QMetaObject take care of wiring it up in general;
    //   then inside of init() we just have to do "Script.require.resolve = Script._requireResolve;"
    Q_INVOKABLE QString _requireResolve(const QString& moduleId, const QString& relativeTo = QString());

    QString logException(const ScriptValuePointer& exception);
    void timerFired();
    void stopAllTimers();
    void stopAllTimersForEntityScript(const EntityItemID& entityID);
    void refreshFileScript(const EntityItemID& entityID);
    void updateEntityScriptStatus(const EntityItemID& entityID, const EntityScriptStatus& status, const QString& errorInfo = QString());
    void setEntityScriptDetails(const EntityItemID& entityID, const EntityScriptDetails& details);
    void setParentURL(const QString& parentURL) { _parentURL = parentURL; }

    QObject* setupTimerWithInterval(const ScriptValuePointer& function, int intervalMS, bool isSingleShot);
    void stopTimer(QTimer* timer);

    QHash<EntityItemID, RegisteredEventHandlers> _registeredHandlers;
    void forwardHandlerCall(const EntityItemID& entityID, const QString& eventName, ScriptValueList eventHanderArgs);

    /*@jsdoc
     * @function Script.entityScriptContentAvailable
     * @param {Uuid} entityID - Entity ID.
     * @param {string} scriptOrURL - Path.
     * @param {string} contents - Contents.
     * @param {boolean} isURL - Is a URL.
     * @param {boolean} success - Success.
     * @param {string} status - Status.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void entityScriptContentAvailable(const EntityItemID& entityID, const QString& scriptOrURL, const QString& contents, bool isURL, bool success, const QString& status);

    EntityItemID currentEntityIdentifier; // Contains the defining entity script entity id during execution, if any. Empty for interface script execution.
    QUrl currentSandboxURL; // The toplevel url string for the entity script that loaded the code being executed, else empty.
    void doWithEnvironment(const EntityItemID& entityID, const QUrl& sandboxURL, std::function<void()> operation);
    void callWithEnvironment(const EntityItemID& entityID, const QUrl& sandboxURL, ScriptValuePointer function, ScriptValuePointer thisObject, ScriptValueList args);

    Context _context;
    Type _type;
    ScriptEnginePointer _engine;
    QString _scriptContents;
    QString _parentURL;
    std::atomic<bool> _isFinished { false };
    std::atomic<bool> _isRunning { false };
    std::atomic<bool> _isStopping { false };
    bool _isInitialized { false };
    QHash<QTimer*, CallbackData> _timerFunctionMap;
    QSet<QUrl> _includedURLs;
    mutable QReadWriteLock _entityScriptsLock { QReadWriteLock::Recursive };
    QHash<EntityItemID, EntityScriptDetails> _entityScripts;
    EntityScriptContentAvailableMap _contentAvailableQueue;

    bool _isThreaded { false };
    QScriptEngineDebugger* _debugger { nullptr };
    bool _debuggable { false };
    qint64 _lastUpdate;

    QString _fileNameString;
    Quat _quatLibrary;
    Vec3 _vec3Library;
    Mat4 _mat4Library;
    ScriptUUID _uuidLibrary;
    ConsoleScriptingInterface _consoleScriptingInterface;
    std::atomic<bool> _isUserLoaded { false };
    bool _isReloading { false };

    std::atomic<bool> _quitWhenFinished;

    AssetScriptingInterface* _assetScriptingInterface;

    std::function<bool()> _emitScriptUpdates{ []() { return true; }  };

    std::recursive_mutex _lock;

    std::chrono::microseconds _totalTimerExecution { 0 };

    static const QString _SETTINGS_ENABLE_EXTENDED_EXCEPTIONS;

    Setting::Handle<bool> _enableExtendedJSExceptions { _SETTINGS_ENABLE_EXTENDED_EXCEPTIONS, true };

    QWeakPointer<ScriptEngines> _scriptEngines;

    friend ScriptManagerPointer newScriptManager(Context context, const QString& scriptContents, const QString& fileNameString);
};

ScriptManagerPointer newScriptManager(ScriptManager::Context context,
                                      const QString& scriptContents,
                                      const QString& fileNameString);
ScriptManagerPointer scriptManagerFactory(ScriptManager::Context context,
                                        const QString& scriptContents,
                                        const QString& fileNameString);

#endif // hifi_ScriptManager_h
