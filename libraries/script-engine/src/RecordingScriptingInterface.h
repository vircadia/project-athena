//
//  Created by Bradley Austin Davis on 2015/11/13
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RecordingScriptingInterface_h
#define hifi_RecordingScriptingInterface_h

#include <atomic>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <DependencyManager.h>
#include <recording/ClipCache.h>
#include <recording/Forward.h>
#include <recording/Frame.h>

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Recording</code> API makes and plays back recordings of voice and avatar movements. Playback may be done on a 
 * user's avatar or an assignment client agent (see the {@link Agent} API).
 *
 * @namespace Recording
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-assignment-client
 */
class RecordingScriptingInterface : public QObject, public Dependency {
    Q_OBJECT

public:
    RecordingScriptingInterface();

public slots:

    /*@jsdoc
     * Called when a {@link Recording.loadRecording} call is complete.
     * @callback Recording~loadRecordingCallback
     * @param {boolean} success - <code>true</code> if the recording has successfully been loaded, <code>false</code> if it 
     *     hasn't.
     * @param {string} url - The URL of the recording that was requested to be loaded.
     */
    /*@jsdoc
     * Loads a recording so that it is ready for playing.
     * @function Recording.loadRecording
     * @param {string} url - The ATP, HTTP, or file system URL of the recording to load.
     * @param {Recording~loadRecordingCallback} [callback=null] - The function to call upon completion.
     * @example <caption>Load and play back a recording from the asset server.</caption>
     * var assetPath = Window.browseAssets();
     * print("Asset path: " + assetPath);
     * 
     * if (assetPath.slice(-4) === ".hfr") {
     *     Recording.loadRecording("atp:" + assetPath, function (success, url) {
     *         if (!success) {
     *             print("Error loading recording.");
     *             return;
     *         }
     *         Recording.startPlaying();
     *     });
     * }
     */
    void loadRecording(const QString& url, ScriptValuePointer callback = ScriptValuePointer());


    /*@jsdoc
     * Starts playing the recording currently loaded or paused.
     * @function Recording.startPlaying
     */
    void startPlaying();

    /*@jsdoc
     * Pauses playback of the recording currently playing. Use {@link Recording.startPlaying|startPlaying} to resume playback 
     * or {@link Recording.stopPlaying|stopPlaying} to stop playback.
     * @function Recording.pausePlayer
     */
    void pausePlayer();

    /*@jsdoc
     * Stops playing the recording currently playing or paused.
     * @function Recording.stopPlaying
     */
    void stopPlaying();

    /*@jsdoc
     * Gets whether a recording is currently playing.
     * @function Recording.isPlaying
     * @returns {boolean} <code>true</code> if a recording is being played, <code>false</code> if one isn't.
     */
    bool isPlaying() const;

    /*@jsdoc
     * Gets whether recording playback is currently paused.
     * @function Recording.isPaused
     * @returns {boolean} <code>true</code> if recording playback is currently paused, <code>false</code> if it isn't.
     */
    bool isPaused() const;


    /*@jsdoc
     * Gets the current playback time in the loaded recording, in seconds.
     * @function Recording.playerElapsed
     * @returns {number} The current playback time in the loaded recording, in seconds.
     */
    float playerElapsed() const;

    /*@jsdoc
     * Gets the length of the loaded recording, in seconds.
     * @function Recording.playerLength
     * @returns {number} The length of the recording currently loaded, in seconds
     */
    float playerLength() const;


    /*@jsdoc
     * Sets the playback audio volume.
     * @function Recording.setPlayerVolume
     * @param {number} volume - The playback audio volume, range <code>0.0</code> &ndash; <code>1.0</code>.
     */
    void setPlayerVolume(float volume);

    /*@jsdoc
     * <p class="important">Not implemented: This method is not implemented yet.</p>
     * @function Recording.setPlayerAudioOffset
     * @param {number} audioOffset - Audio offset.
     */
    void setPlayerAudioOffset(float audioOffset);

    /*@jsdoc
     * Sets the current playback time in the loaded recording.
     * @function Recording.setPlayerTime
     * @param {number} time - The current playback time, in seconds.
     */
    void setPlayerTime(float time);

    /*@jsdoc
     * Sets whether playback should repeat in a loop.
     * @function Recording.setPlayerLoop
     * @param {boolean} loop - <code>true</code> if playback should repeat, <code>false</code> if it shouldn't.
     */
    void setPlayerLoop(bool loop);


    /*@jsdoc
     * Sets whether recording playback will use the display name that the recording was made with.
     * @function Recording.setPlayerUseDisplayName
     * @param {boolean} useDisplayName - <code>true</code> to have recording playback use the display name that the recording 
     *     was made with, <code>false</code> to have recording playback keep the current display name.
     */
    void setPlayerUseDisplayName(bool useDisplayName);

    /*@jsdoc
     * <p><em>Not used.</em></p>
     * @function Recording.setPlayerUseAttachments
     * @param {boolean} useAttachments - Use attachments.
     * @deprecated This method is deprecated and will be removed.
     */
    void setPlayerUseAttachments(bool useAttachments);

    /*@jsdoc
     * <p><em>Not used.</em></p>
     * @function Recording.setPlayerUseHeadModel
     * @param {boolean} useHeadModel - Use head model.
     * @deprecated This method is deprecated and will be removed.
     */
    void setPlayerUseHeadModel(bool useHeadModel);

    /*@jsdoc
     * Sets whether recording playback will use the avatar model that the recording was made with.
     * @function Recording.setPlayerUseSkeletonModel
     * @param {boolean} useSkeletonModel - <code>true</code> to have recording playback use the avatar model that the recording 
     *     was made with, <code>false</code> to have playback use the current avatar model.
     */
    void setPlayerUseSkeletonModel(bool useSkeletonModel);

    /*@jsdoc
     * Sets whether recordings are played at the current avatar location or the recorded location.
     * @function Recording.setPlayFromCurrentLocation
     * @param {boolean} playFromCurrentLocation - <code>true</code> to play recordings at the current avatar location, 
     *     <code>false</code> to play recordings at the recorded location.
     */
    void setPlayFromCurrentLocation(bool playFromCurrentLocation);


    /*@jsdoc
     * Gets whether recording playback will use the display name that the recording was made with.
     * @function Recording.getPlayerUseDisplayName
     * @returns {boolean} <code>true</code> if recording playback will use the display name that the recording was made with, 
     *     <code>false</code> if playback will keep the current display name.
     */
    bool getPlayerUseDisplayName() { return _useDisplayName; }

    /*@jsdoc
     * <p><em>Not used.</em></p>
     * @function Recording.getPlayerUseAttachments
     * @returns {boolean} Use attachments.
     * @deprecated This method is deprecated and will be removed.
     */
    bool getPlayerUseAttachments() { return _useAttachments; }

    /*@jsdoc
     * <p><em>Not used.</em></p>
     * @function Recording.getPlayerUseHeadModel
     * @returns {boolean} Use head model.
     * @deprecated This method is deprecated and will be removed.
     */
    bool getPlayerUseHeadModel() { return _useHeadModel; }

    /*@jsdoc
     * Gets whether recording playback will use the avatar model that the recording was made with.
     * @function Recording.getPlayerUseSkeletonModel
     * @returns {boolean} <code>true</code> if recording playback will use the avatar model that the recording was made with, 
     *     <code>false</code> if playback will use the current avatar model.
     */
    bool getPlayerUseSkeletonModel() { return _useSkeletonModel; }

    /*@jsdoc
     * Gets whether recordings are played at the current avatar location or the recorded location.
     * @function Recording.getPlayFromCurrentLocation
     * @returns {boolean} <code>true</code> if recordings are played at the current avatar location, <code>false</code> if 
     *     played at the recorded location.
     */
    bool getPlayFromCurrentLocation() { return _playFromCurrentLocation; }


    /*@jsdoc
     * Starts making a recording.
     * @function Recording.startRecording
     */
    void startRecording();

    /*@jsdoc
     * Stops making a recording. The recording may be saved using {@link Recording.saveRecording|saveRecording} or 
     * {@link Recording.saveRecordingToAsset|saveRecordingToAsset}, or immediately played back with 
     * {@link Recording.loadLastRecording|loadLastRecording}.
     * @function Recording.stopRecording
     */
    void stopRecording();

    /*@jsdoc
     * Gets whether a recording is currently being made.
     * @function Recording.isRecording
     * @returns {boolean} <code>true</code> if a recording is currently being made, <code>false</code> if one isn't.
     */
    bool isRecording() const;


    /*@jsdoc
     * Gets the duration of the recording currently being made or recently made, in seconds.
     * @function Recording.recorderElapsed
     * @returns {number} The duration of the recording currently being made or recently made, in seconds.
     */
    float recorderElapsed() const;


    /*@jsdoc
     * Gets the default directory that recordings are saved in.
     * @function Recording.getDefaultRecordingSaveDirectory
     * @returns {string} The default recording save directory.
     * @example <caption>Report the default save directory.</caption>
     * print("Default save directory: " + Recording.getDefaultRecordingSaveDirectory());
     */
    QString getDefaultRecordingSaveDirectory();

    /*@jsdoc
     * Saves the most recently made recording to a file.
     * @function Recording.saveRecording
     * @param {string} filename - The path and name of the file to save the recording to.
     * @example <caption>Save a 5 second recording to a file.</caption>
     * Recording.startRecording();
     * 
     * Script.setTimeout(function () {
     *     Recording.stopRecording();
     *     var filename = (new Date()).toISOString();  // yyyy-mm-ddThh:mm:ss.sssZ
     *     filename = filename.slice(0, -5).replace(/:/g, "").replace("T", "-") 
     *         + ".hfr";  // yyyymmmdd-hhmmss.hfr
     *     filename = Recording.getDefaultRecordingSaveDirectory() + filename;
     *     Recording.saveRecording(filename);
     *     print("Saved recording: " + filename);
     * }, 5000);
     */
    void saveRecording(const QString& filename);

    /*@jsdoc
     * Called when a {@link Recording.saveRecordingToAsset} call is complete.
     * @callback Recording~saveRecordingToAssetCallback
     * @param {string} url - The URL of the recording stored in the asset server if successful, <code>""</code> if 
     *     unsuccessful. The URL has <code>atp:</code> as the scheme and the SHA256 hash as the filename (with no extension).
     */
    /*@jsdoc
     * Saves the most recently made recording to the domain's asset server.
     * @function Recording.saveRecordingToAsset
     * @param {Recording~saveRecordingToAssetCallback} callback - The function to call upon completion.
     * @returns {boolean} <code>true</code> if the recording is successfully being saved, <code>false</code> if not.
     * @example <caption>Save a 5 second recording to the asset server.</caption>
     * function onSavedRecordingToAsset(url) {
     *     if (url === "") {
     *         print("Couldn't save recording.");
     *         return;
     *     }
     * 
     *     print("Saved recording: " + url);  // atp:SHA256
     * 
     *     var filename = (new Date()).toISOString();  // yyyy-mm-ddThh:mm:ss.sssZ
     *     filename = filename.slice(0, -5).replace(/:/g, "").replace("T", "-")
     *         + ".hfr";  // yyyymmmdd-hhmmss.hfr
     *     var hash = url.slice(4);  // Remove leading "atp:" from url.
     *     mappingPath = "/recordings/" + filename;
     *     Assets.setMapping(mappingPath, hash, function (error) {
     *         if (error) {
     *             print("Mapping error: " + error);
     *         }
     *     });
     *     print("Mapped recording: " + mappingPath);  // /recordings/filename
     * }
     * 
     * Recording.startRecording();
     * 
     * Script.setTimeout(function () {
     *     Recording.stopRecording();
     *     var success = Recording.saveRecordingToAsset(onSavedRecordingToAsset);
     *     if (!success) {
     *         print("Couldn't save recording.");
     *     }
     * }, 5000);
     */
    bool saveRecordingToAsset(ScriptValuePointer getClipAtpUrl);

    /*@jsdoc
     * Loads the most recently made recording and plays it back on your avatar.
     * @function Recording.loadLastRecording
     * @example <caption>Make a 5 second recording and immediately play it back on your avatar.</caption>
     * Recording.startRecording();
     * 
     * Script.setTimeout(function () {
     *     Recording.stopRecording();
     *     Recording.loadLastRecording();
     * }, 5000);
     */
    void loadLastRecording();

protected:
    using Mutex = std::recursive_mutex;
    using Locker = std::unique_lock<Mutex>;
    using Flag = std::atomic<bool>;

    QSharedPointer<recording::Deck> _player;
    QSharedPointer<recording::Recorder> _recorder;
    
    Flag _playFromCurrentLocation { true };
    Flag _useDisplayName { false };
    Flag _useHeadModel { false };
    Flag _useAttachments { false };
    Flag _useSkeletonModel { false };
    recording::ClipPointer _lastClip;

    QSet<recording::NetworkClipLoaderPointer> _clipLoaders;

private:
    void playClip(recording::NetworkClipLoaderPointer clipLoader, const QString& url, ScriptValuePointer callback);
};

#endif // hifi_RecordingScriptingInterface_h
