//
//  ScriptAudioInjector.h
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2015-02-11.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptAudioInjector_h
#define hifi_ScriptAudioInjector_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <AudioInjectorManager.h>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * Plays or "injects" the content of an audio file.
 *
 * <p>Create using {@link Audio} API methods.</p>
 *
 * @class AudioInjector
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {boolean} playing - <code>true</code> if the audio is currently playing, otherwise <code>false</code>. 
 *     <em>Read-only.</em>
 * @property {number} loudness - The loudness in the last frame of audio, range <code>0.0</code> &ndash; <code>1.0</code>. 
 *     <em>Read-only.</em>
 * @property {AudioInjector.AudioInjectorOptions} options - Configures how the injector plays the audio.
 */
class ScriptAudioInjector : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool playing READ isPlaying)
    Q_PROPERTY(float loudness READ getLoudness)
    Q_PROPERTY(AudioInjectorOptions options WRITE setOptions READ getOptions)
public:
    ScriptAudioInjector(const AudioInjectorPointer& injector);
    ~ScriptAudioInjector();
public slots:

    /*@jsdoc
     * Stops current playback, if any, and starts playing from the beginning.
     * @function AudioInjector.restart
     */
    void restart() { DependencyManager::get<AudioInjectorManager>()->restart(_injector); }

    /*@jsdoc
     * Stops audio playback.
     * @function AudioInjector.stop
     * @example <caption>Stop playing a sound before it finishes.</caption>
     * var sound = SoundCache.getSound(Script.resourcesPath() + "sounds/sample.wav");
     * var injector;
     * var injectorOptions = {
     *     position: MyAvatar.position
     * };
     * 
     * Script.setTimeout(function () { // Give the sound time to load.
     *     injector = Audio.playSound(sound, injectorOptions);
     * }, 1000);
     * 
     * Script.setTimeout(function () {
     *     injector.stop();
     * }, 2000);
     */
    void stop() { DependencyManager::get<AudioInjectorManager>()->stop(_injector); }

    /*@jsdoc
     * Gets the current configuration of the audio injector.
     * @function AudioInjector.getOptions
     * @returns {AudioInjector.AudioInjectorOptions} Configuration of how the injector plays the audio.
     */
    AudioInjectorOptions getOptions() const { return DependencyManager::get<AudioInjectorManager>()->getOptions(_injector); }

    /*@jsdoc
     * Configures how the injector plays the audio.
     * @function AudioInjector.setOptions
     * @param {AudioInjector.AudioInjectorOptions} options - Configuration of how the injector plays the audio.
     */
    void setOptions(const AudioInjectorOptions& options) { DependencyManager::get<AudioInjectorManager>()->setOptions(_injector, options); }

    /*@jsdoc
     * Gets the loudness of the most recent frame of audio played.
     * @function AudioInjector.getLoudness
     * @returns {number} The loudness of the most recent frame of audio played, range <code>0.0</code> &ndash; <code>1.0</code>.
     */
    float getLoudness() const { return DependencyManager::get<AudioInjectorManager>()->getLoudness(_injector); }

    /*@jsdoc
     * Gets whether or not the audio is currently playing.
     * @function AudioInjector.isPlaying
     * @returns {boolean} <code>true</code> if the audio is currently playing, otherwise <code>false</code>.
     * @example <caption>See if a sound is playing.</caption>
     * var sound = SoundCache.getSound(Script.resourcesPath() + "sounds/sample.wav");
     * var injector;
     * var injectorOptions = {
     *     position: MyAvatar.position
     * };
     *
     * Script.setTimeout(function () { // Give the sound time to load.
     *     injector = Audio.playSound(sound, injectorOptions);
     * }, 1000);
     *
     * Script.setTimeout(function () {
     *     print("Sound is playing: " + injector.isPlaying());
     * }, 2000);
     */
    bool isPlaying() const { return DependencyManager::get<AudioInjectorManager>()->isPlaying(_injector); }

signals:

    /*@jsdoc
     * Triggered when the audio has finished playing.
     * @function AudioInjector.finished
     * @returns {Signal}
     * @example <caption>Report when a sound has finished playing.</caption>
     * var sound = SoundCache.getSound(Script.resourcesPath() + "sounds/sample.wav");
     * var injector;
     * var injectorOptions = {
     *     position: MyAvatar.position
     * };
     * 
     * Script.setTimeout(function () { // Give the sound time to load.
     *     injector = Audio.playSound(sound, injectorOptions);
     *     injector.finished.connect(function () {
     *         print("Finished playing sound");
     *     });
     * }, 1000);
     */
    void finished();

private:
    QWeakPointer<AudioInjector> _injector;

    friend ScriptValuePointer injectorToScriptValue(ScriptEngine* engine, ScriptAudioInjector* const& in);
};

Q_DECLARE_METATYPE(ScriptAudioInjector*)

ScriptValuePointer injectorToScriptValue(ScriptEngine* engine, ScriptAudioInjector* const& in);
void injectorFromScriptValue(const ScriptValuePointer& object, ScriptAudioInjector*& out);

#endif // hifi_ScriptAudioInjector_h
