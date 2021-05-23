//
//  WindowScriptingInterface.h
//  interface/src/scripting
//
//  Created by Ryan Huffman on 4/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WindowScriptingInterface_h
#define hifi_WindowScriptingInterface_h

#include <glm/glm.hpp>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QMessageBox>
#include <QtCore/QSharedPointer>

#include <DependencyManager.h>
#include <Scriptable.h>


class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Window</code> API provides various facilities not covered elsewhere, including: window dimensions, window focus, 
 * camera view, announcements, user connections, common dialog boxes, snapshots, file import, domain navigation, domain changes, 
 * domain physics, OS clipboard, build number.
 *
 * @namespace Window
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {number} innerWidth - The width of the drawable area of the Interface window (i.e., without borders or other
 *     chrome), in pixels. <em>Read-only.</em>
 * @property {number} innerHeight - The height of the drawable area of the Interface window (i.e., without borders or other
 *     chrome), in pixels. <em>Read-only.</em>
 * @property {number} x - The x display coordinate of the top left corner of the drawable area of the Interface window. 
 *     <em>Read-only.</em>
 * @property {number} y - The y display coordinate of the top left corner of the drawable area of the Interface window. 
 *     <em>Read-only.</em>
 * @property {boolean} interstitialModeEnabled=false - <code>true</code> if the interstitial graphics are displayed when a 
 *     domain is loading, otherwise <code>false</code>.
 * @property {location} location - Provides facilities for working with your current metaverse location.
 */

class WindowScriptingInterface : public QObject, protected Scriptable, public Dependency {
    Q_OBJECT
    Q_PROPERTY(int innerWidth READ getInnerWidth)
    Q_PROPERTY(int innerHeight READ getInnerHeight)
    Q_PROPERTY(int x READ getX)
    Q_PROPERTY(int y READ getY)
    Q_PROPERTY(bool interstitialModeEnabled READ getInterstitialModeEnabled WRITE setInterstitialModeEnabled)

public:
    WindowScriptingInterface();
    ~WindowScriptingInterface();
    int getInnerWidth();
    int getInnerHeight();
    int getX();
    int getY();

public slots:

    /*@jsdoc
     * Checks whether the Interface window has focus.
     * @function Window.hasFocus
     * @returns {boolean} <code>true</code> if the Interface window has focus, <code>false</code> if it doesn't.
     */
    ScriptValuePointer hasFocus();

    /*@jsdoc
     * Makes the Interface window have focus. On Windows, if Interface doesn't already have focus, the task bar icon flashes to 
     * indicate that Interface wants attention but focus isn't taken away from the application that the user is using.
     * @function Window.setFocus
     */
    void setFocus();

    /*@jsdoc
     * Raises the Interface window if it is minimized. If raised, the window gains focus.
     * @function Window.raise
     */
    void raise();

    /*@jsdoc
     * Displays a dialog with the specified message and an "OK" button. The dialog is non-modal; the script continues without
     * waiting for a user response.
     * @function Window.alert
     * @param {string} [message=""] - The message to display.
     * @example <caption>Display a friendly greeting.</caption>
     * Window.alert("Welcome!");
     * print("Script continues without waiting");
     */
    void alert(const QString& message = "");

    /*@jsdoc
     * Prompts the user to confirm something. Displays a modal dialog with a message plus "Yes" and "No" buttons.
     * @function Window.confirm
     * @param {string} [message=""] - The question to display.
     * @returns {boolean} <code>true</code> if the user selects "Yes", otherwise <code>false</code>.
     * @example <caption>Ask the user a question requiring a yes/no answer.</caption>
     * var answer = Window.confirm("Are you sure?");
     * print(answer);  // true or false
     */
    ScriptValuePointer confirm(const QString& message = "");

    /*@jsdoc
     * Prompts the user to enter some text. Displays a modal dialog with a message and a text box, plus "OK" and "Cancel" 
     * buttons.
     * @function Window.prompt
     * @param {string} message - The question to display.
     * @param {string} defaultText - The default answer text.
     * @returns {string} The text that the user entered if they select "OK", otherwise "".
     * @example <caption>Ask the user a question requiring a text answer.</caption>
     * var answer = Window.prompt("Question", "answer");
     * if (answer === "") {
     *     print("User canceled");
     * } else {
     *     print("User answer: " + answer);
     * }
     */
    ScriptValuePointer prompt(const QString& message, const QString& defaultText);

    /*@jsdoc
     * Prompts the user to enter some text. Displays a non-modal dialog with a message and a text box, plus "OK" and "Cancel" 
     * buttons. A {@link Window.promptTextChanged|promptTextChanged} signal is emitted when the user OKs the dialog; no signal 
     * is emitted if the user cancels the dialog.
     * @function Window.promptAsync
     * @param {string} [message=""] - The question to display.
     * @param {string} [defaultText=""] - The default answer text.
     * @example <caption>Ask the user a question requiring a text answer without waiting for the answer.</caption>
     * function onPromptTextChanged(text) {
     *     print("User answer: " + text);
     * }
     * Window.promptTextChanged.connect(onPromptTextChanged);
     *
     * Window.promptAsync("Question", "answer");
     * print("Script continues without waiting");
     */
    void promptAsync(const QString& message = "", const QString& defaultText = "");

    /*@jsdoc
     * Prompts the user to choose a directory. Displays a modal dialog that navigates the directory tree.
     * @function Window.browseDir
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @returns {string} The path of the directory if one is chosen, otherwise <code>null</code>.
     * @example <caption>Ask the user to choose a directory.</caption>
     * var directory = Window.browseDir("Select Directory", Paths.resources);
     * print("Directory: " + directory);
     */
    ScriptValuePointer browseDir(const QString& title = "", const QString& directory = "");
    
    /*@jsdoc
     * Prompts the user to choose a directory. Displays a non-modal dialog that navigates the directory tree. A
     * {@link Window.browseDirChanged|browseDirChanged} signal is emitted when a directory is chosen; no signal is emitted if
     * the user cancels the dialog.
     * @function Window.browseDirAsync
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @example <caption>Ask the user to choose a directory without waiting for the answer.</caption>
     * function onBrowseDirChanged(directory) {
     *     print("Directory: " + directory);
     * }
     * Window.browseDirChanged.connect(onBrowseDirChanged);
     *
     * Window.browseDirAsync("Select Directory", Paths.resources);
     * print("Script continues without waiting");
     */
    void browseDirAsync(const QString& title = "", const QString& directory = "");

    /*@jsdoc
     * Prompts the user to choose a file. Displays a modal dialog that navigates the directory tree.
     * @function Window.browse
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and 
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @returns {string} The path and name of the file if one is chosen, otherwise <code>null</code>.
     * @example <caption>Ask the user to choose an image file.</caption>
     * var filename = Window.browse("Select Image File", Paths.resources, "Images (*.png *.jpg *.svg)");
     * print("File: " + filename);
     */
    ScriptValuePointer browse(const QString& title = "", const QString& directory = "",  const QString& nameFilter = "");

    /*@jsdoc
     * Prompts the user to choose a file. Displays a non-modal dialog that navigates the directory tree. A
     * {@link Window.browseChanged|browseChanged} signal is emitted when a file is chosen; no signal is emitted if the user
     * cancels the dialog.
     * @function Window.browseAsync
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @example <caption>Ask the user to choose an image file without waiting for the answer.</caption>
     * function onBrowseChanged(filename) {
     *     print("File: " + filename);
     * }
     * Window.browseChanged.connect(onBrowseChanged);
     *
     * Window.browseAsync("Select Image File", Paths.resources, "Images (*.png *.jpg *.svg)");
     * print("Script continues without waiting");
     */
    void browseAsync(const QString& title = "", const QString& directory = "",  const QString& nameFilter = "");

    /*@jsdoc
     * Prompts the user to specify the path and name of a file to save to. Displays a modal dialog that navigates the directory
     * tree and allows the user to type in a file name.
     * @function Window.save
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @returns {string} The path and name of the file if one is specified, otherwise <code>null</code>. If a single file type
     *     is specified in the nameFilter, that file type extension is automatically appended to the result when appropriate.
     * @example <caption>Ask the user to specify a file to save to.</caption>
     * var filename = Window.save("Save to JSON file", Paths.resources, "*.json");
     * print("File: " + filename);
     */
    ScriptValuePointer save(const QString& title = "", const QString& directory = "",  const QString& nameFilter = "");

    /*@jsdoc
     * Prompts the user to specify the path and name of a file to save to. Displays a non-modal dialog that navigates the
     * directory tree and allows the user to type in a file name. A {@link Window.saveFileChanged|saveFileChanged} signal is
     * emitted when a file is specified; no signal is emitted if the user cancels the dialog.
     * @function Window.saveAsync
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @example <caption>Ask the user to specify a file to save to without waiting for an answer.</caption>
     * function onSaveFileChanged(filename) {
     *     print("File: " + filename);
     * }
     * Window.saveFileChanged.connect(onSaveFileChanged);
     *
     * Window.saveAsync("Save to JSON file", Paths.resources, "*.json");
     * print("Script continues without waiting");
     */
    void saveAsync(const QString& title = "", const QString& directory = "",  const QString& nameFilter = "");

    /*@jsdoc
     * Prompts the user to choose an Asset Server item. Displays a modal dialog that navigates the tree of assets on the Asset
     * Server.
     * @function Window.browseAssets
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and 
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @returns {string} The path and name of the asset if one is chosen, otherwise <code>null</code>.
     * @example <caption>Ask the user to select an FBX asset.</caption>
     * var asset = Window.browseAssets("Select FBX File", "/", "*.fbx");
     * print("FBX file: " + asset);
     */
    ScriptValuePointer browseAssets(const QString& title = "", const QString& directory = "", const QString& nameFilter = "");

    /*@jsdoc
     * Prompts the user to choose an Asset Server item. Displays a non-modal dialog that navigates the tree of assets on the 
     * Asset Server. An {@link Window.assetsDirChanged|assetsDirChanged} signal is emitted when an asset is chosen; no signal is
     * emitted if the user cancels the dialog.
     * @function Window.browseAssetsAsync
     * @param {string} [title=""] - The title to display at the top of the dialog.
     * @param {string} [directory=""] - The initial directory to start browsing at.
     * @param {string} [nameFilter=""] - The types of files to display. Examples: <code>"*.json"</code> and
     *     <code>"Images (*.png *.jpg *.svg)"</code>. All files are displayed if a filter isn't specified.
     * @example
     * function onAssetsDirChanged(asset) {
     *     print("FBX file: " + asset);
     * }
     * Window.assetsDirChanged.connect(onAssetsDirChanged);
     *
     * Window.browseAssetsAsync("Select FBX File", "/", "*.fbx");
     * print("Script continues without waiting");
     */
    void browseAssetsAsync(const QString& title = "", const QString& directory = "", const QString& nameFilter = "");

    /*@jsdoc
     * Opens the Asset Browser dialog. If a file to upload is specified, the user is prompted to enter the folder and name to
     * map the file to on the asset server.
     * @function Window.showAssetServer
     * @param {string} [uploadFile=""] - The path and name of a file to upload to the asset server.
     * @example <caption>Upload a file to the asset server.</caption>
     * var filename = Window.browse("Select File to Add to Asset Server", Paths.resources);
     * print("File: " + filename);
     * Window.showAssetServer(filename);
     */
    void showAssetServer(const QString& upload = "");

    /*@jsdoc
     * Gets Interface's build number.
     * @function Window.checkVersion
     * @returns {string} Interface's build number.
     */
    QString checkVersion();

    /*@jsdoc
     * Gets Interface's user agent.
     * @function Window.getUserAgent
     * @returns {string} Interface's user agent.
     */
    QString getUserAgent();

    /*@jsdoc
     * Gets the signature for Interface's protocol version.
     * @function Window.protocolSignature
     * @returns {string} A string uniquely identifying the version of the metaverse protocol that Interface is using.
     */
    QString protocolSignature();

    /*@jsdoc
     * Copies text to the operating system's clipboard.
     * @function Window.copyToClipboard
     * @param {string} text - The text to copy to the operating system's clipboard.
     */
    void copyToClipboard(const QString& text);

    /*@jsdoc
     * Takes a snapshot of the current Interface view from the primary camera. When a still image only is captured, 
     * {@link Window.stillSnapshotTaken|stillSnapshotTaken} is emitted; when a still image plus moving images are captured, 
     * {@link Window.processingGifStarted|processingGifStarted} and {@link Window.processingGifCompleted|processingGifCompleted}
     * are emitted.
     * <p>Snapshots are saved to the path specified in Settings &gt; General &gt; Snapshots, which can be accessed via the 
     * {@link Snapshot} API.</p>
     *
     * @function Window.takeSnapshot
     * @param {boolean} [notify=true] - This value is passed on through the {@link Window.stillSnapshotTaken|stillSnapshotTaken}
     *     signal.
     * @param {boolean} [includeAnimated=false] - If <code>true</code>, a moving image is captured as an animated GIF in addition 
     *     to a still image.
     * @param {number} [aspectRatio=0] - The width/height ratio of the snapshot required. If the value is <code>0</code>, the
     *     full resolution is used (window dimensions in desktop mode; HMD display dimensions in HMD mode), otherwise one of the
     *     dimensions is adjusted in order to match the aspect ratio.
     * @param {string} [filename=""] - If a filename is not provided, the image is saved as "vircadia-snap-by-&lt;user 
     *     name&gt;-on-YYYY-MM-DD_HH-MM-SS".
     *     <p>Still images are saved in JPEG or PNG format according to the extension provided &mdash; <code>".jpg"</code>, 
     *     <code>".jpeg"</code>, or <code>".png"</code> &mdash; or if not provided then in JPEG format with an extension of 
     *     <code>".jpg"</code>. Animated images are saved in GIF format.</p>
     *
     * @example <caption>Using the snapshot function and signals.</caption>
     * function onStillSnapshotTaken(path, notify) {
     *     print("Still snapshot taken: " + path);
     *     print("Notify: " + notify);
     * }
     *
     * function onProcessingGifStarted(stillPath) {
     *     print("Still snapshot taken: " + stillPath);
     * }
     *
     * function onProcessingGifCompleted(animatedPath) {
     *     print("Animated snapshot taken: " + animatedPath);
     * }
     *
     * Window.stillSnapshotTaken.connect(onStillSnapshotTaken);
     * Window.processingGifStarted.connect(onProcessingGifStarted);
     * Window.processingGifCompleted.connect(onProcessingGifCompleted);
     *
     * var notify = true;
     * var animated = true;
     * var aspect = 1920 / 1080;
     * var filename = "example-snapshot";
     * Window.takeSnapshot(notify, animated, aspect, filename);
     */
    void takeSnapshot(bool notify = true, bool includeAnimated = false, float aspectRatio = 0.0f, const QString& filename = QString());

    /*@jsdoc
     * Takes a still snapshot of the current view from the secondary camera that can be set up through the {@link Render} API.
     * <p>Snapshots are saved to the path specified in Settings &gt; General &gt; Snapshots, which can be accessed via the
     * {@link Snapshot} API.</p>
     * @function Window.takeSecondaryCameraSnapshot
     * @param {boolean} [notify=true] - This value is passed on through the {@link Window.stillSnapshotTaken|stillSnapshotTaken}
     *     signal.
     * @param {string} [filename=""] - If a filename is not provided, the image is saved as "vircadia-snap-by-&lt;user
     *     name&gt;-on-YYYY-MM-DD_HH-MM-SS".
     *     <p>Images are saved in JPEG or PNG format according to the extension provided &mdash; <code>".jpg"</code>,
     *     <code>".jpeg"</code>, or <code>".png"</code> &mdash; or if not provided then in JPEG format with an extension of
     *     <code>".jpg"</code>.</p>
     */
    void takeSecondaryCameraSnapshot(const bool& notify = true, const QString& filename = QString());

    /*@jsdoc
     * Takes a 360&deg; snapshot at a given position for the secondary camera. The secondary camera does not need to have been 
     *     set up.
     * <p>Snapshots are saved to the path specified in Settings &gt; General &gt; Snapshots, which can be accessed via the
     * {@link Snapshot} API.</p>
     * @function Window.takeSecondaryCamera360Snapshot
     * @param {Vec3} cameraPosition - The position of the camera for the snapshot.
     * @param {boolean} [cubemapOutputFormat=false] - If <code>true</code> then the snapshot is saved as a cube map image, 
     *     otherwise it is saved as an equirectangular image.
     * @param {boolean} [notify=true] - This value is passed on through the {@link Window.stillSnapshotTaken|stillSnapshotTaken}
     *     signal.
     * @param {string} [filename=""] - If a filename is not provided, the image is saved as "vircadia-snap-by-&lt;user
     *     name&gt;-on-YYYY-MM-DD_HH-MM-SS".
     *     <p>Images are saved in JPEG or PNG format according to the extension provided &mdash; <code>".jpg"</code>,
     *     <code>".jpeg"</code>, or <code>".png"</code> &mdash; or if not provided then in JPEG format with an extension of
     *     <code>".jpg"</code>.</p>
     */
    void takeSecondaryCamera360Snapshot(const glm::vec3& cameraPosition, const bool& cubemapOutputFormat = false, const bool& notify = true, const QString& filename = QString());

    /*@jsdoc
     * Emits a {@link Window.connectionAdded|connectionAdded} or a {@link Window.connectionError|connectionError} signal that
     * indicates whether or not a user connection was successfully made using the Web API.
     * @function Window.makeConnection
     * @param {boolean} success - If <code>true</code> then {@link Window.connectionAdded|connectionAdded} is emitted, otherwise
     *     {@link Window.connectionError|connectionError} is emitted.
     * @param {string} description - Descriptive text about the connection success or error. This is sent in the signal emitted.
     */
    void makeConnection(bool success, const QString& userNameOrError);

    /*@jsdoc
     * Displays a notification message. Notifications are displayed in panels by the default script, nofications.js. An
     * {@link Window.announcement|announcement} signal is emitted when this function is called.
     * @function Window.displayAnnouncement
     * @param {string} message - The announcement message.
     * @example <caption>Send and capture an announcement message.</caption>
     * function onAnnouncement(message) {
     *     // The message is also displayed as a notification by notifications.js.
     *     print("Announcement: " + message);
     * }
     * Window.announcement.connect(onAnnouncement);
     *
     * Window.displayAnnouncement("Hello");
     */
    void displayAnnouncement(const QString& message);

    /*@jsdoc
     * Prepares a snapshot ready for sharing. A {@link Window.snapshotShared|snapshotShared} signal is emitted when the snapshot
     * has been prepared.
     * @function Window.shareSnapshot
     * @param {string} path - The path and name of the image file to share.
     * @param {string} [href=""] - The metaverse location where the snapshot was taken.
     */
    void shareSnapshot(const QString& path, const QUrl& href = QUrl(""));

    /*@jsdoc
     * Checks to see if physics is active for you in the domain you're visiting - there is a delay between your arrival at a
     * domain and physics becoming active for you in that domain.
     * @function Window.isPhysicsEnabled
     * @returns {boolean} <code>true</code> if physics is currently active for you, otherwise <code>false</code>.
     * @example <caption>Wait for physics to be enabled when you change domains.</caption>
     * function checkForPhysics() {
     *     var isPhysicsEnabled = Window.isPhysicsEnabled();
     *     print("Physics enabled: " + isPhysicsEnabled);
     *     if (!isPhysicsEnabled) {
     *         Script.setTimeout(checkForPhysics, 1000);
     *     }
     * }
     *
     * function onDomainChanged(domain) {
     *     print("Domain changed: " + domain);
     *     Script.setTimeout(checkForPhysics, 1000);
     * }
     *
     * Window.domainChanged.connect(onDomainChanged);
     */
    bool isPhysicsEnabled();

    /*@jsdoc
     * Sets what to show on the PC display. For entity camera view, the entity camera is configured using 
     * {@link Camera.setCameraEntity} and {@link Camera|Camera.mode}.
     * @function Window.setDisplayTexture
     * @param {Window.DisplayTexture} texture - The view to display.
     * @returns {boolean} <code>true</code> if the display texture was successfully set, otherwise <code>false</code>.
     */
    bool setDisplayTexture(const QString& name);

    /*@jsdoc
     * Checks if a 2D point is within the desktop window if in desktop mode, or the drawable area of the HUD overlay if in HMD
     * mode.
     * @function Window.isPointOnDesktopWindow
     * @param {Vec2} point - The point to check.
     * @returns {boolean} <code>true</code> if the point is within the window or HUD, otherwise <code>false</code>.
     */
    bool isPointOnDesktopWindow(QVariant point);

    /*@jsdoc
     * Gets the size of the drawable area of the Interface window if in desktop mode or the HMD rendering surface if in HMD mode.
     * @function Window.getDeviceSize
     * @returns {Vec2} The width and height of the Interface window or HMD rendering surface, in pixels.
     */
    glm::vec2 getDeviceSize() const;

    /*@jsdoc
     * Gets the last domain connection error when a connection is refused.
     * @function Window.getLastDomainConnectionError
     * @returns {Window.ConnectionRefusedReason} Integer number that enumerates the last domain connection refused.
     */
    int getLastDomainConnectionError() const;

    /*@jsdoc
     * Opens a non-modal message box that can have a variety of button combinations. See also, 
     * {@link Window.updateMessageBox|updateMessageBox} and {@link Window.closeMessageBox|closeMessageBox}.
     * @function Window.openMessageBox
     * @param {string} title - The title to display for the message box.
     * @param {string} text - Text to display in the message box.
     * @param {Window.MessageBoxButton} buttons - The buttons to display on the message box; one or more button values added
     *     together.
     * @param {Window.MessageBoxButton} defaultButton - The button that has focus when the message box is opened.
     * @returns {number} The ID of the message box created.
     * @example <caption>Ask the user whether that want to reset something.</caption>
     * var messageBox;
     * var resetButton = 0x4000000;
     * var cancelButton = 0x400000;
     *
     * function onMessageBoxClosed(id, button) {
     *     if (id === messageBox) {
     *         if (button === resetButton) {
     *             print("Reset");
     *         } else {
     *             print("Don't reset");
     *         }
     *     }
     * }
     * Window.messageBoxClosed.connect(onMessageBoxClosed);
     *
     * messageBox = Window.openMessageBox("Reset Something", 
     *     "Do you want to reset something?",
     *     resetButton + cancelButton, cancelButton);
     */
    int openMessageBox(QString title, QString text, int buttons, int defaultButton);

    /*@jsdoc
     * Opens a URL in the Interface window or other application, depending on the URL's scheme. The following schemes are 
     * supported:
     * <ul>
     *   <li><code>hifi</code>: Navigate to the URL in Interface.</li>
     *   <li><code>hifiapp</code>: Open a system app in Interface.</li>
     * </ul>
     * <p>Other schemes will either be handled by the OS (e.g. <code>http</code>, <code>https</code>, or <code>mailto</code>) or 
     * will display a dialog asking the user to confirm that they want to try to open the URL.</p>
     * @function Window.openUrl
     * @param {string} url - The URL to open.
     */
    void openUrl(const QUrl& url);

    /*@jsdoc
     * Opens an Android activity and optionally return back to the scene when the activity is completed. <em>Android only.</em>
     * @function Window.openAndroidActivity
     * @param {string} activityName - The name of the activity to open: one of <code>"Home"</code>, <code>"Login"</code>, or 
     *     <code>"Privacy Policy"</code>.
     * @param {boolean} backToScene - If <code>true</code>, the user is automatically returned back to the scene when the 
     *     activity is completed.
     */
    void openAndroidActivity(const QString& activityName, const bool backToScene);

    /*@jsdoc
     * Updates the content of a message box that was opened with {@link Window.openMessageBox|openMessageBox}.
     * @function Window.updateMessageBox
     * @param {number} id - The ID of the message box.
     * @param {string} title - The title to display for the message box.
     * @param {string} text - Text to display in the message box.
     * @param {Window.MessageBoxButton} buttons - The buttons to display on the message box; one or more button values added
     *     together.
     * @param {Window.MessageBoxButton} defaultButton - The button that has focus when the message box is opened.
     */
    void updateMessageBox(int id, QString title, QString text, int buttons, int defaultButton);

    /*@jsdoc
     * Closes a message box that was opened with {@link Window.openMessageBox|openMessageBox}.
     * @function Window.closeMessageBox
     * @param {number} id - The ID of the message box.
     */
    void closeMessageBox(int id);

    /*@jsdoc
     * @function Window.domainLoadingProgress
     * @returns {number} Progress.
     * @deprecated This function is deprecated and will be removed.
     */
    float domainLoadingProgress();

    /*@jsdoc
     * Gets the number of display plugins currently available.
     * @function Window.getDisplayPluginCount
     * @returns {number} The number of display plugins currently available.
     */
    int getDisplayPluginCount();

    /*@jsdoc
     * Gets the name of a display plugin.
     * @function Window.getDisplayPluginName
     * @param {number} index - The index of the display plugin. Must be less than the value returned by 
     *     {@link Window.getDisplayPluginCount|getDisplayPluginCount}. The first display plugin has an index of <code>0</code>.
     * @returns {string} The name of the display plugin.
     * @example <caption>Print the names of all available display plugins.</caption>
     * for (var i = 0, length = Window.getDisplayPluginCount(); i < length; i++) {
     *     print(Window.getDisplayPluginName(i));
     * }
     */
    QString getDisplayPluginName(int index);

    /*@jsdoc
     * Checks whether a display plugin is an HMD.
     * @function Window.isDisplayPluginHmd
     * @param {number} index - The index of the display plugin. Must be less than the value returned by 
     *     {@link Window.getDisplayPluginCount|getDisplayPluginCount}. The first display plugin has an index of <code>0</code>.
     * @returns {boolean} <code>true</code> if the display plugin is a HMD, <code>false</code> if it isn't.
     */
    bool isDisplayPluginHmd(int index);

    /*@jsdoc
     * Gets the index of the currently active display plugin.
     * @function Window.getActiveDisplayPlugin
     * @returns {number} The index of the currently active display plugin. The first display plugin has an index of 
     *     <code>0</code>.
     */
    int getActiveDisplayPlugin();

    /*@jsdoc
     * Sets the currently active display plugin.
     * @function Window.setActiveDisplayPlugin
     * @param {number} index - The index of the display plugin. Must be less than the value returned by 
     *     {@link Window.getDisplayPluginCount|getDisplayPluginCount}. The first display plugin has an index of <code>0</code>.
     */
    void setActiveDisplayPlugin(int index);

    /*@jsdoc
     * Opens an Interface web browser window.
     * @function Window.openWebBrowser
     * @param {string} [url=""] - The URL of the web page to display.
     */
    void openWebBrowser(const QString& url = "");


private slots:
    void onWindowGeometryChanged(const QRect& geometry);
    void onMessageBoxSelected(int button);
    void disconnectedFromDomain();

signals:

    /*@jsdoc
     * Triggered when you change the domain you're visiting.
     * <p><strong>Warning:</strong> Is not emitted if you go to a domain that isn't running.</p>
     * @function Window.domainChanged
     * @param {string} domainURL - The domain's URL.
     * @returns {Signal}
     * @example <caption>Report when you change domains.</caption>
     * function onDomainChanged(domain) {
     *     print("Domain changed: " + domain);
     * }
     *
     * Window.domainChanged.connect(onDomainChanged);
     */
    void domainChanged(QUrl domainURL);

    /*@jsdoc
     * Triggered when you try to navigate to a *.json, *.svo, or *.svo.json URL in a Web browser within Interface.
     * @function Window.svoImportRequested
     * @param {string} url - The URL of the file to import.
     * @returns {Signal}
     */
    void svoImportRequested(const QString& url);

    /*@jsdoc
     * Triggered when you try to visit a domain but are refused connection.
     * @function Window.domainConnectionRefused
     * @param {string} reasonMessage - A description of the refusal.
     * @param {Window.ConnectionRefusedReason} reasonCode - Integer number that enumerates the reason for the refusal.
     * @param {string} extraInfo - Extra information about the refusal.
     * @returns {Signal}
     */
    void domainConnectionRefused(const QString& reasonMessage, int reasonCode, const QString& extraInfo);

    /*@jsdoc
     * Triggered when you try to visit a domain but are redirected into the error state.
     * @function Window.redirectErrorStateChanged
     * @param {boolean} isInErrorState - <code>true</code> if the user has been redirected to the error URL, <code>false</code> 
     *     if they haven't.
     * @returns {Signal}
     */
    void redirectErrorStateChanged(bool isInErrorState);

    /*@jsdoc
     * Triggered when the interstitial mode changes.
     * @function Window.interstitialModeChanged
     * @param {boolean} interstitialMode - <code>true</code> if the interstitial graphics are displayed when the domain is 
     *     loading, <code>false</code> if they are not.
     * @returns {Signal}
     */
    void interstitialModeChanged(bool interstitialMode);

    /*@jsdoc
     * Triggered when a still snapshot has been taken by calling {@link Window.takeSnapshot|takeSnapshot} with 
     *     <code>includeAnimated = false</code> or {@link Window.takeSecondaryCameraSnapshot|takeSecondaryCameraSnapshot}.
     * @function Window.stillSnapshotTaken
     * @param {string} pathStillSnapshot - The path and name of the snapshot image file.
     * @param {boolean} notify - The value of the <code>notify</code> parameter that {@link Window.takeSnapshot|takeSnapshot}
     *     was called with.
     * @returns {Signal}
     */
    void stillSnapshotTaken(const QString& pathStillSnapshot, bool notify);

    /*@jsdoc
    * Triggered when a still 360&deg; snapshot has been taken by calling 
    *     {@link Window.takeSecondaryCamera360Snapshot|takeSecondaryCamera360Snapshot}.
    * @function Window.snapshot360Taken
    * @param {string} pathStillSnapshot - The path and name of the snapshot image file.
    * @param {boolean} notify - The value of the <code>notify</code> parameter that {@link Window.takeSecondaryCamera360Snapshot|takeSecondaryCamera360Snapshot}
    *     was called with.
    * @returns {Signal}
    */
    void snapshot360Taken(const QString& path360Snapshot, bool notify);

    /*@jsdoc
     * Triggered when a snapshot submitted via {@link Window.shareSnapshot|shareSnapshot} is ready for sharing. The snapshot
     * may then be shared via the {@link Account.metaverseServerURL} Web API.
     * @function Window.snapshotShared
     * @param {boolean} isError - <code>true</code> if an error was encountered preparing the snapshot for sharing, otherwise
     *     <code>false</code>.
     * @param {string} reply - JSON-formatted information about the snapshot.
     * @returns {Signal}
     */
    void snapshotShared(bool isError, const QString& reply);

    /*@jsdoc
     * Triggered when the snapshot images have been captured by {@link Window.takeSnapshot|takeSnapshot} and the GIF is
     *     starting to be processed.
     * @function Window.processingGifStarted
     * @param {string} pathStillSnapshot - The path and name of the still snapshot image file.
     * @returns {Signal}
     */
    void processingGifStarted(const QString& pathStillSnapshot);

    /*@jsdoc
     * Triggered when a GIF has been prepared of the snapshot images captured by {@link Window.takeSnapshot|takeSnapshot}.
     * @function Window.processingGifCompleted
     * @param {string} pathAnimatedSnapshot - The path and name of the moving snapshot GIF file.
     * @returns {Signal}
     */
    void processingGifCompleted(const QString& pathAnimatedSnapshot);


    /*@jsdoc
     * Triggered when you've successfully made a user connection.
     * @function Window.connectionAdded
     * @param {string} message - A description of the success.
     * @returns {Signal}
     */
    void connectionAdded(const QString& connectionName);

    /*@jsdoc
     * Triggered when you failed to make a user connection.
     * @function Window.connectionError
     * @param {string} message - A description of the error.
     * @returns {Signal}
     */
    void connectionError(const QString& errorString);

    /*@jsdoc
     * Triggered when a message is announced by {@link Window.displayAnnouncement|displayAnnouncement}.
     * @function Window.announcement
     * @param {string} message - The message text.
     * @returns {Signal}
     */
    void announcement(const QString& message);


    /*@jsdoc
     * Triggered when the user closes a message box that was opened with {@link Window.openMessageBox|openMessageBox}.
     * @function Window.messageBoxClosed
     * @param {number} id - The ID of the message box that was closed.
     * @param {number} button - The button that the user clicked. If the user presses Esc, the Cancel button value is returned,
     *    whether or not the Cancel button is displayed in the message box.
     * @returns {Signal}
     */
    void messageBoxClosed(int id, int button);

    /*@jsdoc
     * Triggered when the user chooses a directory in a {@link Window.browseDirAsync|browseDirAsync} dialog.
     * @function Window.browseDirChanged
     * @param {string} directory - The directory the user chose in the dialog.
     * @returns {Signal}
     */
    void browseDirChanged(QString browseDir);

    /*@jsdoc
     * Triggered when the user chooses an asset in a {@link Window.browseAssetsAsync|browseAssetsAsync} dialog.
     * @function Window.assetsDirChanged
     * @param {string} asset - The path and name of the asset the user chose in the dialog.
     * @returns {Signal}
     */
    void assetsDirChanged(QString assetsDir);

    /*@jsdoc
     * Triggered when the user specifies a file in a {@link Window.saveAsync|saveAsync} dialog.
     * @function Window.saveFileChanged
     * @param {string} filename - The path and name of the file that the user specified in the dialog.
     * @returns {Signal}
     */
    void saveFileChanged(QString filename);

    /*@jsdoc
     * Triggered when the user chooses a file in a {@link Window.browseAsync|browseAsync} dialog.
     * @function Window.browseChanged
     * @param {string} filename - The path and name of the file the user chose in the dialog.
     * @returns {Signal}
     */
    void browseChanged(QString filename);

    /*@jsdoc
     * Triggered when the user OKs a {@link Window.promptAsync|promptAsync} dialog.
     * @function Window.promptTextChanged
     * @param {string} text - The text the user entered in the dialog.
     * @returns {Signal}
     */
    void promptTextChanged(QString text);


    /*@jsdoc
     * Triggered when the position or size of the Interface window changes.
     * @function Window.geometryChanged
     * @param {Rect} geometry - The position and size of the drawable area of the Interface window.
     * @returns {Signal}
     * @example <caption>Report the position of size of the Interface window when it changes.</caption>
     * function onWindowGeometryChanged(rect) {
     *     print("Window geometry: " + JSON.stringify(rect));
     * }
     *
     * Window.geometryChanged.connect(onWindowGeometryChanged);
     */
    void geometryChanged(QRect geometry);


    /*@jsdoc
     * Triggered when "minimized" state of the Interface window changes.
     * @function Window.minimizedChanged
     * @param {boolean} isMinimized - <code>true</code> if the Interface window is minimized, <code>false</code> if it isn't.
     * @returns {Signal}
     * @example <caption>Report the "minimized" state of the Interface window when it changes.</caption>
     * function onWindowMinimizedChanged(minimized) {
     *     print("Window minimized: " + minimized);
     * }
     *
     * Window.minimizedChanged.connect(onWindowMinimizedChanged);
     */
    void minimizedChanged(bool isMinimized);

private:
    QString getPreviousBrowseLocation() const;
    void setPreviousBrowseLocation(const QString& location);

    QString getPreviousBrowseAssetLocation() const;
    void setPreviousBrowseAssetLocation(const QString& location);

    bool getInterstitialModeEnabled() const;
    void setInterstitialModeEnabled(bool enableInterstitialMode);

    void ensureReticleVisible() const;

    int createMessageBox(QString title, QString text, int buttons, int defaultButton);
    QHash<int, QQuickItem*> _messageBoxes;
    int _lastMessageBoxID{ -1 };
};

#endif // hifi_WindowScriptingInterface_h
