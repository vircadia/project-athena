//
//  ScriptingInterface.h
//  libraries/controllers/src/controllers
//
//  Created by Brad Hefta-Gaub on 12/17/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_AbstractControllerScriptingInterface_h
#define hifi_AbstractControllerScriptingInterface_h

#include <atomic>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <QCursor>
#include <QThread>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QMutex>

#include <QtQml/QJSValue>

#include <DependencyManager.h>
#include <StreamUtils.h>

#include "UserInputMapper.h"
#include "StandardControls.h"

namespace controller {
    class InputController : public QObject {
        Q_OBJECT

    public:
        using Key = unsigned int;
        using Pointer = std::shared_ptr<InputController>;

        virtual void update() = 0;
        virtual Key getKey() const = 0;

    public slots:
        virtual bool isActive() const = 0;
        virtual glm::vec3 getAbsTranslation() const = 0;
        virtual glm::quat getAbsRotation() const = 0;
        virtual glm::vec3 getLocTranslation() const = 0;
        virtual glm::quat getLocRotation() const = 0;

    signals:
        //void spatialEvent(const SpatialEvent& event);
    };

    /// handles scripting of input controller commands from JS
    class ScriptingInterface : public QObject, public Dependency {
        Q_OBJECT

        // These properties have JSDoc in ControllerScriptingInterface.h.
        Q_PROPERTY(QVariantMap Hardware READ getHardware CONSTANT FINAL)
        Q_PROPERTY(QVariantMap Actions READ getActions CONSTANT FINAL)
        Q_PROPERTY(QVariantMap Standard READ getStandard CONSTANT FINAL)

    public:
        ScriptingInterface();
        virtual ~ScriptingInterface() {};

        /*@jsdoc
         * Gets a list of all available actions.
         * @function Controller.getAllActions
         * @returns {Action[]} All available actions.
         * @deprecated This function is deprecated and will be removed. It no longer works.
         */
        // FIXME: This function causes a JavaScript crash: https://highfidelity.manuscript.com/f/cases/edit/13921
        Q_INVOKABLE QVector<Action> getAllActions();
        
        /*@jsdoc
         * Gets a list of all available inputs for a hardware device.
         * @function Controller.getAvailableInputs
         * @param {number} deviceID - Integer ID of the hardware device.
         * @returns {NamedPair[]} All available inputs for the device.
         * @deprecated This function is deprecated and will be removed. It no longer works.
         */
        // FIXME: This function causes a JavaScript crash: https://highfidelity.manuscript.com/f/cases/edit/13922
        Q_INVOKABLE QVector<Input::NamedPair> getAvailableInputs(unsigned int device);
        
        /*@jsdoc
         * Finds the name of a particular controller from its device ID.
         * @function Controller.getDeviceName
         * @param {number} deviceID - The integer ID of the device.
         * @returns {string} The name of the device if found, otherwise <code>"unknown"</code>.
         * @example <caption>Get the name of the Oculus Touch controller from its ID.</caption>
         * var deviceID = Controller.findDevice("OculusTouch");
         * print("Device ID = " + deviceID);
         *
         * var deviceName = Controller.getDeviceName(deviceID);
         * print("Device name = " + deviceName);
         */
        Q_INVOKABLE QString getDeviceName(unsigned int device);

        /*@jsdoc
         * Gets the current value of an action.
         * @function Controller.getActionValue
         * @param {number} actionID - The integer ID of the action.
         * @returns {number} The current value of the action.
         * @example <caption>Periodically report the value of the "TranslateX" action.</caption>
         * var actionID = Controller.findAction("TranslateX");
         *
         * function reportValue() {
         *     print(Controller.getActionValue(actionID));
         * }
         * reportTimer = Script.setInterval(reportValue, 1000);
         */
        Q_INVOKABLE float getActionValue(int action);

        /*@jsdoc
         * Finds the ID of a specific controller from its device name.
         * @function Controller.findDevice
         * @param {string} deviceName - The name of the device to find.
         * @returns {number} The integer ID of the device if available, otherwise <code>65535</code>.
         * @example <caption>Get the ID of the Oculus Touch.</caption>
         * var deviceID = Controller.findDevice("OculusTouch");
         * print("Device ID = " + deviceID);
         */
        Q_INVOKABLE int findDevice(QString name);

        /*@jsdoc
         * Gets the names of all currently available controller devices plus "Actions", "Application", and "Standard".
         * @function Controller.getDeviceNames
         * @returns {string[]} An array of device names.
         * @example <caption>Get the names of all currently available controller devices.</caption>
         * var deviceNames = Controller.getDeviceNames();
         * print(JSON.stringify(deviceNames));
         * // ["Standard","Keyboard","LeapMotion","OculusTouch","Application","Actions"] or similar.
         */
        Q_INVOKABLE QVector<QString> getDeviceNames();
        
        /*@jsdoc
         * Finds the ID of an action from its name.
         * @function Controller.findAction
         * @param {string} actionName - The name of the action: one of the {@link Controller.Actions} property names.
         * @returns {number} The integer ID of the action if found, otherwise <code>4095</code>. Note that this value is not 
         * the same as the value of the relevant {@link Controller.Actions} property.
         * @example <caption>Get the ID of the "TranslateY" action. Compare with the property value.</caption>
         * var actionID = Controller.findAction("TranslateY");
         * print("Action ID = " + actionID);  // 1
         * print("Property value = " + Controller.Actions.TranslateY);  // 537001728 or similar value.
         */
        Q_INVOKABLE int findAction(QString actionName);

        /*@jsdoc
         * Gets the names of all actions available as properties of {@link Controller.Actions}.
         * @function Controller.getActionNames
         * @returns {string[]} An array of action names.
         * @example <caption>Get the names of all actions.</caption>
         * var actionNames = Controller.getActionNames();
         * print("Action names: " + JSON.stringify(actionNames));
         * // ["TranslateX","TranslateY","TranslateZ","Roll", ...
         */
        Q_INVOKABLE QVector<QString> getActionNames() const;

        /*@jsdoc
         * Gets the value of a controller button or axis output. Note: Also gets the value of a controller axis output. 
         * @function Controller.getValue
         * @param {number} source - The {@link Controller.Standard} or {@link Controller.Hardware} item.
         * @returns {number} The current value of the controller item output if <code>source</code> is valid, otherwise 
         *     <code>0</code>.
         * @example <caption>Report the Standard and Vive right trigger values.</caption>
         * var triggerValue = Controller.getValue(Controller.Standard.RT);
         * print("Trigger value: " + triggerValue);
         *
         * if (Controller.Hardware.Vive) {
         *     triggerValue = Controller.getValue(Controller.Hardware.Vive.RT);
         *     print("Vive trigger value: " + triggerValue);
         * } else {
         *     print("No Vive present");
         * }
         */
        Q_INVOKABLE float getValue(const int& source) const;

        /*@jsdoc
         * Gets the value of a controller axis output. Note: Also gets the value of a controller button output.
         * @function Controller.getAxisValue
         * @param {number} source - The {@link Controller.Standard} or {@link Controller.Hardware} item.
         * @returns {number} The current value of the controller item output if <code>source</code> is valid, otherwise 
         *     <code>0</code>.
         */
        // TODO: getAxisValue() should use const int& parameter? Or others shouldn't?
        Q_INVOKABLE float getAxisValue(int source) const;

        /*@jsdoc
         * Gets the value of a controller pose output.
         * @function Controller.getPoseValue
         * @param {number} source - The {@link Controller.Standard} or {@link Controller.Hardware} pose output.
         * @returns {Pose} The current value of the controller pose output if <code>source</code> is a pose output, otherwise 
         *     an invalid pose with <code>Pose.valid == false</code>.
         * @exammple <caption>Report the right hand's pose.</caption>
         * var pose = Controller.getPoseValue(Controller.Standard.RightHand);
         * print("Pose: " + JSON.stringify(pose));
         */
        Q_INVOKABLE Pose getPoseValue(const int& source) const;

        /*@jsdoc
         * Triggers a haptic pulse on connected and enabled devices that have the capability.
         * @function Controller.triggerHapticPulse
         * @param {number} strength - The strength of the haptic pulse, range <code>0.0</code> &ndash; <code>1.0</code>.
         * @param {number} duration - The duration of the haptic pulse, in milliseconds.
         * @param {number} [index=2] - The index on devices on which to trigger the haptic pulse.  The meaning of each index
         *     will vary by device.  For example, for hand controllers, <code>index = 0</code> is the left hand,
         *     <code>index = 1</code> is the right hand, and <code>index = 2</code> is both hands.  For other devices,
         *     such as haptic vests, index will have a different meaning, defined by the input device.
         * @example <caption>Trigger a haptic pulse on the right hand.</caption>
         * var HAPTIC_STRENGTH = 0.5;
         * var HAPTIC_DURATION = 10;
         * var RIGHT_HAND = 1;
         * Controller.triggerHapticPulse(HAPTIC_STRENGTH, HAPTIC_DURATION, RIGHT_HAND);
         */
        Q_INVOKABLE bool triggerHapticPulse(float strength, float duration, uint16_t index = 2) const;

        /*@jsdoc
         * Triggers a 250ms haptic pulse on connected and enabled devices that have the capability.
         * @function Controller.triggerShortHapticPulse
         * @param {number} strength - The strength of the haptic pulse, range <code>0.0</code> &ndash; <code>1.0</code>.
         * @param {number} [index=2] - The index on devices on which to trigger the haptic pulse.  The meaning of each index
         *     will vary by device.  For example, for hand controllers, <code>index = 0</code> is the left hand,
         *     <code>index = 1</code> is the right hand, and <code>index = 2</code> is both hands.  For other devices,
         *     such as haptic vests, index will have a different meaning, defined by the input device.
         */
        Q_INVOKABLE bool triggerShortHapticPulse(float strength, uint16_t index = 2) const;

        /*@jsdoc
         * Triggers a haptic pulse on a particular device if connected and enabled and it has the capability.
         * @function Controller.triggerHapticPulseOnDevice
         * @param {number} deviceID - The ID of the device to trigger the haptic pulse on.
         * @param {number} strength - The strength of the haptic pulse, range <code>0.0</code> &ndash; <code>1.0</code>.
         * @param {number} duration - The duration of the haptic pulse, in milliseconds.
         * @param {number} [index=2] - The index on this device on which to trigger the haptic pulse.  The meaning of each index
         *     will vary by device.  For example, for hand controllers, <code>index = 0</code> is the left hand,
         *     <code>index = 1</code> is the right hand, and <code>index = 2</code> is both hands.  For other devices,
         *     such as haptic vests, index will have a different meaning, defined by the input device.
         * @example <caption>Trigger a haptic pulse on an Oculus Touch controller.</caption>
         * var HAPTIC_STRENGTH = 0.5;
         * var deviceID = Controller.findDevice("OculusTouch");
         * var HAPTIC_DURATION = 10;
         * var RIGHT_HAND = 1;
         * Controller.triggerHapticPulseOnDevice(deviceID, HAPTIC_STRENGTH, HAPTIC_DURATION, RIGHT_HAND);
         */
        Q_INVOKABLE bool triggerHapticPulseOnDevice(unsigned int device, float strength, float duration,
            uint16_t index = 2) const;

        /*@jsdoc
         * Triggers a 250ms haptic pulse on a particular device if connected and enabled and it has the capability.
         * @function Controller.triggerShortHapticPulseOnDevice
         * @param {number} deviceID - The ID of the device to trigger the haptic pulse on.
         * @param {number} strength - The strength of the haptic pulse, range <code>0.0</code> &ndash; <code>1.0</code>.
         * @param {number} [index=2] - The index on this device on which to trigger the haptic pulse.  The meaning of each index
         *     will vary by device.  For example, for hand controllers, <code>index = 0</code> is the left hand,
         *     <code>index = 1</code> is the right hand, and <code>index = 2</code> is both hands.  For other devices,
         *     such as haptic vests, index will have a different meaning, defined by the input device.
         */
        Q_INVOKABLE bool triggerShortHapticPulseOnDevice(unsigned int device, float strength, uint16_t index = 2) const;

        /*@jsdoc
         * Creates a new controller mapping. Routes can then be added to the mapping using {@link MappingObject} methods and 
         * routed to <code>Standard</code> controls, <code>Actions</code>, or script functions using {@link RouteObject} 
         * methods. The mapping can then be enabled using {@link Controller.enableMapping|enableMapping} for it to take effect.
         * @function Controller.newMapping
         * @param {string} [mappingName=Uuid.generate()] - A unique name for the mapping. If not specified a new UUID generated 
         *     by {@link Uuid(0).generate|Uuid.generate} is used.
         * @returns {MappingObject} A controller mapping object.
         * @example <caption>Create a simple mapping that makes the right trigger move your avatar up.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         *
         * mapping.from(Controller.Standard.RT).to(Controller.Actions.TranslateY);
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* newMapping(const QString& mappingName = QUuid::createUuid().toString());

        /*@jsdoc
         * Enables or disables a controller mapping. When enabled, the routes in the mapping have effect.
         * @function Controller.enableMapping
         * @param {string} mappingName - The name of the mapping.
         * @param {boolean} [enable=true] - If <code>true</code> then the mapping is enabled, otherwise it is disabled.
         */
        Q_INVOKABLE void enableMapping(const QString& mappingName, bool enable = true);

        /*@jsdoc
         * Disables a controller mapping. When disabled, the routes in the mapping have no effect.
         * @function Controller.disableMapping
         * @param {string} mappingName - The name of the mapping.
         */
        Q_INVOKABLE void disableMapping(const QString& mappingName) { enableMapping(mappingName, false); }

        /*@jsdoc
         * Creates a new controller mapping from a {@link Controller.MappingJSON|MappingJSON} string. Use 
         * {@link Controller.enableMapping|enableMapping} to enable the mapping for it to take effect.
         * @function Controller.parseMapping
         * @param {string} jsonString - A JSON string of the {@link Controller.MappingJSON|MappingJSON}.
         * @returns {MappingObject} A controller mapping object.
         * @example <caption>Create a simple mapping that makes the right trigger move your avatar up.</caption>
         * var mappingJSON = {
         *     "name": "com.highfidelity.controllers.example.jsonMapping",
         *     "channels": [
         *         { "from": "Standard.RT", "to": "Actions.TranslateY" }
         *     ]
         * };
         *
         * var mapping = Controller.parseMapping(JSON.stringify(mappingJSON));
         * mapping.enable();
         *
         * Script.scriptEnding.connect(function () {
         *     mapping.disable();
         * });
         */
        Q_INVOKABLE QObject* parseMapping(const QString& json);

        /*@jsdoc
         * Creates a new controller mapping from a {@link Controller.MappingJSON|MappingJSON} JSON file at a URL. Use 
         * {@link Controller.enableMapping|enableMapping} to enable the mapping for it to take effect.
         * <p><strong>Warning:</strong> This function is not yet implemented; it doesn't load a mapping and just returns 
         * <code>null</code>.
         * @function Controller.loadMapping
         * @param {string} jsonURL - The URL the {@link Controller.MappingJSON|MappingJSON} JSON file.
         * @returns {MappingObject} A controller mapping object.
         */
        Q_INVOKABLE QObject* loadMapping(const QString& jsonUrl);


        /*@jsdoc
         * Gets the {@link Controller.Hardware} property tree. Calling this function is the same as using the {@link Controller} 
         * property, <code>Controller.Hardware</code>.
         * @function Controller.getHardware
         * @returns {Controller.Hardware} The {@link Controller.Hardware} property tree.
         */
        Q_INVOKABLE const QVariantMap getHardware() { return _hardware; }

        /*@jsdoc
         * Gets the {@link Controller.Actions} property tree. Calling this function is the same as using the {@link Controller} 
         * property, <code>Controller.Actions</code>.
         * @function Controller.getActions
         * @returns {Controller.Actions} The {@link Controller.Actions} property tree.
         */
        Q_INVOKABLE const QVariantMap getActions() { return _actions; }  //undefined

        /*@jsdoc
         * Gets the {@link Controller.Standard} property tree. Calling this function is the same as using the {@link Controller} 
         * property, <code>Controller.Standard</code>.
         * @function Controller.getStandard
         * @returns {Controller.Standard} The {@link Controller.Standard} property tree.
         */
        Q_INVOKABLE const QVariantMap getStandard() { return _standard; }


        /*@jsdoc
         * Starts making a recording of currently active controllers.
         * @function Controller.startInputRecording
         * @example <caption>Make a controller recording.</caption>
         * // Delay start of recording for 2s.
         * Script.setTimeout(function () {
         *     print("Start input recording");
         *     Controller.startInputRecording();
         * }, 2000);
         *
         * // Make a 10s recording.
         * Script.setTimeout(function () {
         *     print("Stop input recording");
         *     Controller.stopInputRecording();
         *     Controller.saveInputRecording();
         *     print("Input recording saved in: " + Controller.getInputRecorderSaveDirectory());
         * }, 12000);
         */
        Q_INVOKABLE void startInputRecording();

        /*@jsdoc
         * Stops making a recording started by {@link Controller.startInputRecording|startInputRecording}.
         * @function Controller.stopInputRecording
         */
        Q_INVOKABLE void stopInputRecording();

        /*@jsdoc
         * Plays back the current recording from the beginning. The current recording may have been recorded by 
         * {@link Controller.startInputRecording|startInputRecording} and 
         * {@link Controller.stopInputRecording|stopInputRecording}, or loaded by 
         * {@link Controller.loadInputRecording|loadInputRecording}. Playback repeats in a loop until 
         * {@link Controller.stopInputPlayback|stopInputPlayback} is called.
         * @function Controller.startInputPlayback
         * @example <caption>Play back a controller recording.</caption>
         * var file = Window.browse("Select Recording", Controller.getInputRecorderSaveDirectory());
         * if (file !== null) {
         *     print("Play recording: " + file);
         *     Controller.loadInputRecording("file:///" + file);
         *     Controller.startInputPlayback();
         *
         *     // Stop playback after 20s.
         *     Script.setTimeout(function () {
         *         print("Stop playing recording");
         *         Controller.stopInputPlayback();
         *     }, 20000);
         * }
         */
        Q_INVOKABLE void startInputPlayback();

        /*@jsdoc
         * Stops play back of a recording started by {@link Controller.startInputPlayback|startInputPlayback}.
         * @function Controller.stopInputPlayback
         */
        Q_INVOKABLE void stopInputPlayback();

        /*@jsdoc
         * Saves the current recording to a file. The current recording may have been recorded by
         * {@link Controller.startInputRecording|startInputRecording} and
         * {@link Controller.stopInputRecording|stopInputRecording}, or loaded by
         * {@link Controller.loadInputRecording|loadInputRecording}. It is saved in the directory returned by 
         * {@link Controller.getInputRecorderSaveDirectory|getInputRecorderSaveDirectory}.
         * @function Controller.saveInputRecording
         */
        Q_INVOKABLE void saveInputRecording();

        /*@jsdoc
         * Loads an input recording, ready for play back.
         * @function Controller.loadInputRecording
         * @param {string} file - The path to the recording file, prefixed by <code>"file:///"</code>.
         */
        Q_INVOKABLE void loadInputRecording(const QString& file);

        /*@jsdoc
         * Gets the directory in which input recordings are saved.
         * @function Controller.getInputRecorderSaveDirectory
         * @returns {string} The directory in which input recordings are saved.
         */
        Q_INVOKABLE QString getInputRecorderSaveDirectory();

        /*@jsdoc
         * Gets the names of all the active and running (enabled) input devices.
         * @function Controller.getRunningInputDevices
         * @returns {string[]} The list of current active and running input devices.
         * @example <caption>List all active and running input devices.</caption>
         * print("Running devices: " + JSON.stringify(Controller.getRunningInputDeviceNames()));
         */
        Q_INVOKABLE QStringList getRunningInputDeviceNames();

        bool isMouseCaptured() const { return _mouseCaptured; }
        bool isTouchCaptured() const { return _touchCaptured; }
        bool isWheelCaptured() const { return _wheelCaptured; }
        bool areActionsCaptured() const { return _actionsCaptured; }

    public slots:

        /*@jsdoc
         * Disables processing of mouse "move", "press", "double-press", and "release" events into 
         * {@link Controller.Hardware|Controller.Hardware.Keyboard} outputs.
         * @function Controller.captureMouseEvents
         * @example <caption>Disable Controller.Hardware.Keyboard mouse events for a short period.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Hardware.Keyboard.MouseX).to(function (x) {
         *     print("Mouse x = " + x);
         * });
         * mapping.from(Controller.Hardware.Keyboard.MouseY).to(function (y) {
         *     print("Mouse y = " + y);
         * });
         * Controller.enableMapping(MAPPING_NAME);
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         *
         * Script.setTimeout(function () {
         *     Controller.captureMouseEvents();
         * }, 5000);
         *
         * Script.setTimeout(function () {
         *     Controller.releaseMouseEvents();
         * }, 10000);
         */
        virtual void captureMouseEvents() { _mouseCaptured = true; }

        /*@jsdoc
         * Enables processing of mouse "move", "press", "double-press", and "release" events into 
         * {@link Controller.Hardware-Keyboard|Controller.Hardware.Keyboard} outputs that were disabled using 
         * {@link Controller.captureMouseEvents|captureMouseEvents}.
         * @function Controller.releaseMouseEvents
         */
        virtual void releaseMouseEvents() { _mouseCaptured = false; }


        /*@jsdoc
         * Disables processing of touch "begin", "update", and "end" events into 
         * {@link Controller.Hardware|Controller.Hardware.Keyboard}, 
         * {@link Controller.Hardware|Controller.Hardware.Touchscreen}, and 
         * {@link Controller.Hardware|Controller.Hardware.TouchscreenVirtualPad} outputs.
         * @function Controller.captureTouchEvents
         */
        virtual void captureTouchEvents() { _touchCaptured = true; }

        /*@jsdoc
         * Enables processing of touch "begin", "update", and "end" events into 
         * {@link Controller.Hardware|Controller.Hardware.Keyboard}, 
         * {@link Controller.Hardware|Controller.Hardware.Touchscreen}, and 
         * {@link Controller.Hardware|Controller.Hardware.TouchscreenVirtualPad} outputs that were disabled using 
         * {@link Controller.captureTouchEvents|captureTouchEvents}.
         * @function Controller.releaseTouchEvents
         */
        virtual void releaseTouchEvents() { _touchCaptured = false; }


        /*@jsdoc
         * Disables processing of mouse wheel rotation events into {@link Controller.Hardware|Controller.Hardware.Keyboard} 
         * outputs.
         * @function Controller.captureWheelEvents
         */
        virtual void captureWheelEvents() { _wheelCaptured = true; }

        /*@jsdoc
         * Enables processing of mouse wheel rotation events into {@link Controller.Hardware|Controller.Hardware.Keyboard} 
         * outputs that wer disabled using {@link Controller.captureWheelEvents|captureWheelEvents}.
         * @function Controller.releaseWheelEvents
         */
        virtual void releaseWheelEvents() { _wheelCaptured = false; }


        /*@jsdoc
         * Disables translating and rotating the user's avatar in response to keyboard and controller controls.
         * @function Controller.captureActionEvents
         * @example <caption>Disable avatar translation and rotation for a short period.</caption>
         * Script.setTimeout(function () {
         *     Controller.captureActionEvents();
         * }, 5000);
         *
         * Script.setTimeout(function () {
         *     Controller.releaseActionEvents();
         * }, 10000);
         */
        virtual void captureActionEvents() { _actionsCaptured = true; }

        /*@jsdoc
         * Enables translating and rotating the user's avatar in response to keyboard and controller controls that were disabled 
         * using {@link Controller.captureActionEvents|captureActionEvents}.
         * @function Controller.releaseActionEvents
         */
        virtual void releaseActionEvents() { _actionsCaptured = false; }

        /*@jsdoc
         * @function Controller.updateRunningInputDevices
         * @param {string} deviceName - Device name.
         * @param {boolean} isRunning - Is running.
         * @param {string[]} runningDevices - Running devices.
         * @deprecated This function is deprecated and will be removed.
         */
        void updateRunningInputDevices(const QString& deviceName, bool isRunning, const QStringList& runningDevices);

    signals:
        /*@jsdoc
         * Triggered when an action occurs.
         * @function Controller.actionEvent
         * @param {number} actionID - The ID of the action, per {@link Controller.findAction|findAction}.
         * @param {number} value - The value associated with the action.
         * @returns {Signal}
         * @example <caption>Report action events as they occur.</caption>
         * var actionNamesForID = {};
         * var actionNames = Controller.getActionNames();
         * for (var i = 0, length = actionNames.length; i < length; i++) {
         *     actionNamesForID[Controller.findAction(actionNames[i])] = actionNames[i];
         * }
         *
         * function onActionEvent(action, value) {
         *     print("onActionEvent() : " + action + " ( " + actionNamesForID[action] + " ) ; " + value);
         * }
         *
         * Controller.actionEvent.connect(onActionEvent);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.actionEvent.disconnect(onActionEvent);
         * });
         */
        void actionEvent(int action, float state);

        /*@jsdoc
         * Triggered when there is a new controller input event.
         * @function Controller.inputEvent
         * @param {number} action - The input action, per {@link Controller.Standard}.
         * @param {number} value - The value associated with the input action.
         * @returns {Signal}
         * @example <caption>Report input events as they occur.</caption>
         * var inputNamesForID = {};
         * for (var property in Controller.Standard) {
         *     inputNamesForID[Controller.Standard[property]] = "Controller.Standard." + property;
         * }
         *
         * function onInputEvent(input, value) {
         *     print("onInputEvent() : " + input + " ( " + inputNamesForID[input] + " ) ; " + value);
         * }
         *
         * Controller.inputEvent.connect(onInputEvent);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.inputEvent.disconnect(onInputEvent);
         * });
         */
        void inputEvent(int action, float state);

        /*@jsdoc
         * Triggered when a device is registered or unregistered by a plugin. Not all plugins generate 
         * <code>hardwareChanged</code> events: for example, connecting or disconnecting a mouse will not generate an event but 
         * connecting or disconnecting an Xbox controller will.
         * @function Controller.hardwareChanged
         * @returns {Signal}
         */
        void hardwareChanged();

        /*@jsdoc
         * Triggered when an input device starts or stops being active and running (enabled). For example, enabling or 
         * disabling the LeapMotion in Settings &gt; Controls &gt; Calibration will trigger this signal.
         * @function Controller.inputDeviceRunningChanged
         * @param {string} deviceName - The name of the device.
         * @param {boolean} isRunning - <code>true</code> if the device is active and running, <code>false</code> if it isn't.
         * @returns {Signal}
         */
        void inputDeviceRunningChanged(QString deviceName, bool isRunning);


    private:
        // Update the exposed variant maps reporting active hardware
        void updateMaps();

        QVariantMap _hardware;
        QVariantMap _actions;
        QVariantMap _standard;

        QStringList _runningInputDeviceNames;

        std::atomic<bool> _mouseCaptured{ false };
        std::atomic<bool> _touchCaptured { false };
        std::atomic<bool> _wheelCaptured { false };
        std::atomic<bool> _actionsCaptured { false };

        QMutex _runningDevicesMutex;
    };

}

#endif // hifi_AbstractControllerScriptingInterface_h
