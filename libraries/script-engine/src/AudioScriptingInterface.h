//
//  AudioScriptingInterface.h
//  libraries/audio/src
//
//  Created by Stephen Birarda on 1/2/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AudioScriptingInterface_h
#define hifi_AudioScriptingInterface_h

#include <AbstractAudioInterface.h>
#include <AudioInjector.h>
#include <DependencyManager.h>
#include <Sound.h>

class ScriptAudioInjector;
class ScriptEngine;

class AudioScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

    // JSDoc for property is in Audio.h.
    Q_PROPERTY(bool isStereoInput READ isStereoInput WRITE setStereoInput NOTIFY isStereoInputChanged)
    Q_PROPERTY(bool isSoloing READ isSoloing)
    Q_PROPERTY(QVector<QUuid> soloList READ getSoloList)

public:
    virtual ~AudioScriptingInterface() = default;
    void setLocalAudioInterface(AbstractAudioInterface* audioInterface);

    bool isSoloing() const {
        return _localAudioInterface->getAudioSolo().isSoloing();
    }

    QVector<QUuid> getSoloList() const {
        return _localAudioInterface->getAudioSolo().getUUIDs();
    }

    /*@jsdoc
     * Adds avatars to the audio solo list. If the audio solo list is not empty, only audio from the avatars in the list is 
     * played.
     * @function Audio.addToSoloList
     * @param {Uuid[]} ids - Avatar IDs to add to the solo list.

     * @example <caption>Listen to a single nearby avatar for a short while.</caption>
     * // Find nearby avatars.
     * var RANGE = 100; // m
     * var nearbyAvatars = AvatarList.getAvatarsInRange(MyAvatar.position, RANGE);
     * 
     * // Remove own avatar from list.
     * var myAvatarIndex = nearbyAvatars.indexOf(MyAvatar.sessionUUID);
     * if (myAvatarIndex !== -1) {
     *     nearbyAvatars.splice(myAvatarIndex, 1);
     * }
     * 
     * if (nearbyAvatars.length > 0) {
     *     // Listen to only one of the nearby avatars.
     *     var avatarName = AvatarList.getAvatar(nearbyAvatars[0]).displayName;
     *     print("Listening only to " + avatarName);
     *     Audio.addToSoloList([nearbyAvatars[0]]);
     * 
     *     // Stop listening to only the one avatar after a short while.
     *     Script.setTimeout(function () {
     *         print("Finished listening only to " + avatarName);
     *         Audio.resetSoloList();
     *     }, 10000); // 10s
     * 
     * } else {
     *     print("No nearby avatars");
     * }
     */
    Q_INVOKABLE void addToSoloList(QVector<QUuid> uuidList) {
        _localAudioInterface->getAudioSolo().addUUIDs(uuidList);
    }

    /*@jsdoc
     * Removes avatars from the audio solo list. If the audio solo list is not empty, only audio from the avatars in the list 
     * is played.
     * @function Audio.removeFromSoloList
     * @param {Uuid[]} ids - Avatar IDs to remove from the solo list.
     */
    Q_INVOKABLE void removeFromSoloList(QVector<QUuid> uuidList) {
        _localAudioInterface->getAudioSolo().removeUUIDs(uuidList);
    }

    /*@jsdoc
     * Clears the audio solo list.
     * @function Audio.resetSoloList
     */
    Q_INVOKABLE void resetSoloList() {
        _localAudioInterface->getAudioSolo().reset();
    }

    /*@jsdoc
     * Gets whether your microphone audio is echoed back to you from the server. When enabled, microphone audio is echoed only 
     * if you're unmuted or are using push-to-talk.
     * @function Audio.getServerEcho
     * @returns {boolean} <code>true</code> if echoing microphone audio back to you from the server is enabled, 
     *     <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool getServerEcho();

    /*@jsdoc
     * Sets whether your microphone audio is echoed back to you from the server. When enabled, microphone audio is echoed 
     * only if you're unmuted or are using push-to-talk.
     * @function Audio.setServerEcho
     * @param {boolean} serverEcho - <code>true</code> to enable echoing microphone back to you from the server, 
     *     <code>false</code> to disable.
     */
    Q_INVOKABLE void setServerEcho(bool serverEcho);

    /*@jsdoc
     * Toggles the echoing of microphone audio back to you from the server. When enabled, microphone audio is echoed only if 
     * you're unmuted or are using push-to-talk.
     * @function Audio.toggleServerEcho
     */
    Q_INVOKABLE void toggleServerEcho();

    /*@jsdoc
     * Gets whether your microphone audio is echoed back to you by the client. When enabled, microphone audio is echoed 
     * even if you're muted or not using push-to-talk.
     * @function Audio.getLocalEcho
     * @returns {boolean} <code>true</code> if echoing microphone audio back to you from the client is enabled, 
     *     <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool getLocalEcho();

    /*@jsdoc
     * Sets whether your microphone audio is echoed back to you by the client. When enabled, microphone audio is echoed 
     * even if you're muted or not using push-to-talk.
     * @function Audio.setLocalEcho
     * @parm {boolean} localEcho - <code>true</code> to enable echoing microphone audio back to you from the client, 
     *     <code>false</code> to disable.
     * @example <caption>Echo local audio for a few seconds.</caption>
     * Audio.setLocalEcho(true);
     * Script.setTimeout(function () {
     *     Audio.setLocalEcho(false);
     * }, 3000); // 3s
     */
    Q_INVOKABLE void setLocalEcho(bool localEcho);

    /*@jsdoc
     * Toggles the echoing of microphone audio back to you by the client. When enabled, microphone audio is echoed even if 
     * you're muted or not using push-to-talk.
     * @function Audio.toggleLocalEcho
     */
    Q_INVOKABLE void toggleLocalEcho();


protected:
    AudioScriptingInterface() = default;

    // these methods are protected to stop C++ callers from calling, but invokable from script

    /*@jsdoc
     * Starts playing or "injecting" the content of an audio file. The sound is played globally (sent to the audio 
     * mixer) so that everyone hears it, unless the <code>injectorOptions</code> has <code>localOnly</code> set to 
     * <code>true</code> in which case only the client hears the sound played. No sound is played if sent to the audio mixer 
     * but the client is not connected to an audio mixer. The {@link AudioInjector} object returned by the function can be used 
     * to control the playback and get information about its current state.
     * @function Audio.playSound
     * @param {SoundObject} sound - The content of an audio file, loaded using {@link SoundCache.getSound}. See 
     * {@link SoundObject} for supported formats.
     * @param {AudioInjector.AudioInjectorOptions} [injectorOptions={}] - Configures where and how the audio injector plays the 
     *     audio file.
     * @returns {AudioInjector} The audio injector that plays the audio file.
     * @example <caption>Play a sound.</caption>
     * var sound = SoundCache.getSound("https://cdn-1.vircadia.com/us-c-1/ken/samples/forest_ambiX.wav");
     * 
     * function playSound() {
     *     var injectorOptions = {
     *         position: MyAvatar.position
     *     };
     *     var injector = Audio.playSound(sound, injectorOptions);
     * }
     * 
     * function onSoundReady() {
     *     sound.ready.disconnect(onSoundReady);
     *     playSound();
     * }
     * 
     * if (sound.downloaded) {
     *     playSound();
     * } else {
     *     sound.ready.connect(onSoundReady);
     * }
     */
    Q_INVOKABLE ScriptAudioInjector* playSound(SharedSoundPointer sound, const AudioInjectorOptions& injectorOptions = AudioInjectorOptions());

    /*@jsdoc
     * Starts playing the content of an audio file locally (isn't sent to the audio mixer). This is the same as calling 
     * {@link Audio.playSound} with {@link AudioInjector.AudioInjectorOptions} <code>localOnly</code> set <code>true</code> and 
     * the specified <code>position</code>.
     * @function Audio.playSystemSound
     * @param {SoundObject} sound - The content of an audio file, which is loaded using {@link SoundCache.getSound}. See 
     * {@link SoundObject} for supported formats.
     * @returns {AudioInjector} The audio injector that plays the audio file.
     */
    Q_INVOKABLE ScriptAudioInjector* playSystemSound(SharedSoundPointer sound);

    /*@jsdoc
     * Sets whether the audio input should be used in stereo. If the audio input doesn't support stereo then setting a value 
     * of <code>true</code> has no effect.
     * @function Audio.setStereoInput
     * @param {boolean} stereo - <code>true</code> if the audio input should be used in stereo, otherwise <code>false</code>.
     */
    Q_INVOKABLE void setStereoInput(bool stereo);

    /*@jsdoc
     * Gets whether the audio input is used in stereo.
     * @function Audio.isStereoInput
     * @returns {boolean} <code>true</code> if the audio input is used in stereo, otherwise <code>false</code>. 
     */
    Q_INVOKABLE bool isStereoInput();

signals:

    /*@jsdoc
     * Triggered when the client is muted by the mixer because their loudness value for the noise background has reached the 
     * threshold set for the domain (in the server settings).
     * @function Audio.mutedByMixer
     * @returns {Signal} 
     */
    void mutedByMixer();

    /*@jsdoc
     * Triggered when the client is muted by the mixer because they're within a certain radius (50m) of someone who requested 
     * the mute through Developer &gt; Audio &gt; Mute Environment.
     * @function Audio.environmentMuted
     * @returns {Signal} 
     */
    void environmentMuted();

    /*@jsdoc
     * Triggered when the client receives its first packet from the audio mixer.
     * @function Audio.receivedFirstPacket
     * @returns {Signal} 
     */
    void receivedFirstPacket();

    /*@jsdoc
     * Triggered when the client is disconnected from the audio mixer.
     * @function Audio.disconnected
     * @returns {Signal} 
     */
    void disconnected();

    /*@jsdoc
     * Triggered when the noise gate is opened. The input audio signal is no longer blocked (fully attenuated) because it has 
     * risen above an adaptive threshold set just above the noise floor. Only occurs if <code>Audio.noiseReduction</code> is 
     * <code>true</code>.
     * @function Audio.noiseGateOpened
     * @returns {Signal} 
     */
    void noiseGateOpened();

    /*@jsdoc
     * Triggered when the noise gate is closed. The input audio signal is blocked (fully attenuated) because it has fallen 
     * below an adaptive threshold set just above the noise floor. Only occurs if <code>Audio.noiseReduction</code> is 
     * <code>true</code>.
     * @function Audio.noiseGateClosed
     * @returns {Signal} 
     */
    void noiseGateClosed();

    /*@jsdoc
     * Triggered when a frame of audio input is processed.
     * @function Audio.inputReceived
     * @param {Int16Array} inputSamples - The audio input processed.
     * @returns {Signal} 
     */
    void inputReceived(const QByteArray& inputSamples);

    /*@jsdoc
     * Triggered when the input audio use changes between mono and stereo.
     * @function Audio.isStereoInputChanged
     * @param {boolean} isStereo - <code>true</code> if the input audio is stereo, otherwise <code>false</code>.
     * @returns {Signal}
     */
    void isStereoInputChanged(bool isStereo);

private:
    AbstractAudioInterface* _localAudioInterface { nullptr };
};

void registerAudioMetaTypes(ScriptEngine* engine);

#endif // hifi_AudioScriptingInterface_h
