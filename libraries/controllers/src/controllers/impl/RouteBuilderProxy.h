//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_Controllers_Impl_RouteBuilderProxy_h
#define hifi_Controllers_Impl_RouteBuilderProxy_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include "Filter.h"
#include "Route.h"
#include "Mapping.h"

#include "../UserInputMapper.h"

class QJSValue;
class QJsonValue;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

namespace controller {

class ScriptingInterface;

/*@jsdoc
 * <p>A route in a {@link MappingObject} used by the {@link Controller} API.</p>
 *
 * <p>Create a route using {@link MappingObject} methods and apply this object's methods to process it, terminating with 
 * {@link RouteObject#to} to apply it to a <code>Standard</code> control, action, or script function. Note: Loops are not 
 * permitted.</p>
 *
 * <p>Some methods apply to routes with number data, some apply routes with {@link Pose} data, and some apply to both route 
 * types.<p>
 *
 * @class RouteObject
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */

// TODO migrate functionality to a RouteBuilder class and make the proxy defer to that 
// (for easier use in both C++ and JS)
class RouteBuilderProxy : public QObject {
        Q_OBJECT
    public:
        RouteBuilderProxy(UserInputMapper& parent, Mapping::Pointer mapping, Route::Pointer route)
            : _parent(parent), _mapping(mapping), _route(route) { }

        /*@jsdoc
         * Terminates the route with a standard control, an action, or a script function. The output value from the route is 
         * sent to the specified destination.
         * <p>This is a QML-specific version of {@link MappingObject#to|to}: use this version in QML files.</p>
         * @function RouteObject#toQml
         * @param {Controller.Standard|Controller.Actions|function} destination - The standard control, action, or JavaScript
         * function that the route output is mapped to. For a function, the parameter can be either the name of the function or
         * an in-line function definition.
         */
        Q_INVOKABLE void toQml(const QJSValue& destination);

        /*@jsdoc
         * Processes the route only if a condition is satisfied. The condition is evaluated before the route input is read, and
         * the input is read only if the condition is <code>true</code>. Thus, if the condition is not met then subsequent
         * routes using the same input are processed.
         * <p>This is a QML-specific version of {@link MappingObject#when|when}: use this version in QML files.</p>
         * @function RouteObject#whenQml
         * @param {condition|condition[]} expression - <p>A <code>condition</code> may be a:</p>
         *     <ul>
         *         <li>A boolean or numeric {@link Controller.Hardware} property, which is evaluated as a boolean.</li>
         *         <li><code>!</code> followed by a {@link Controller.Hardware} property, indicating the logical NOT should be
         *         used.</li>
         *         <li>A script function returning a boolean value. This can be either the name of the function or an in-line
         *         definition.</li>
         *     </ul>
         * <p>If an array of conditions is provided, their values are ANDed together.</p>
         * @returns {RouteObject} The <code>RouteObject</code> with the condition added.
         */
        Q_INVOKABLE QObject* whenQml(const QJSValue& expression);

        /*@jsdoc
         * Terminates the route with a standard control, an action, or a script function. The output value from the route is 
         * sent to the specified destination.
         * @function RouteObject#to
         * @param {Controller.Standard|Controller.Actions|function} destination - The standard control, action, or JavaScript 
         * function that the route output is mapped to. For a function, the parameter can be either the name of the function or 
         * an in-line function definition.
         *
         * @example <caption>Make the right trigger move your avatar up.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         *
         * mapping.from(Controller.Standard.RT).to(Controller.Actions.TranslateY);
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         *
         * @example <caption>Make the right trigger call a function.</caption>
         * function onRightTrigger(value) {
         *     print("Trigger value: " + value);
         * }
         *
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         *
         * mapping.from(Controller.Standard.RT).to(onRightTrigger);
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE void to(const ScriptValuePointer& destination);

        /*@jsdoc
         * Enables or disables writing debug information for a route to the program log.
         * @function RouteObject#debug
         * @param {boolean} [enable=true] - If <code>true</code> then writing debug information is enabled for the route, 
         *     otherwise it is disabled.
         * @returns {RouteObject} The <code>RouteObject</code> with debug output enabled or disabled.
         * @example <caption>Write debug information to the program log for a right trigger mapping.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         *
         * mapping.from(Controller.Standard.RT).debug().to(function (value) {
         *     print("Value: " + value);
         * });
         *
         * // Information similar to the following is written each frame:
         * [DEBUG] [hifi.controllers] Beginning mapping frame
         * [DEBUG] [hifi.controllers] Processing device routes
         * [DEBUG] [hifi.controllers] Processing standard routes
         * [DEBUG] [hifi.controllers] Applying route  ""
         * [DEBUG] [hifi.controllers] Value was  5.96046e-07
         * [DEBUG] [hifi.controllers] Filtered value was  5.96046e-07
         *
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* debug(bool enable = true);

        /*@jsdoc
         * Processes the route without marking the controller output as having been read, so that other routes from the same 
         * controller output can also process.
         * @function RouteObject#peek
         * @param {boolean} [enable=true] - If <code>true</code> then the route is processed without marking the route's 
         *     controller source as having been read.
         * @returns {RouteObject} The <code>RouteObject</code> with the peek feature enabled.
         */
        Q_INVOKABLE QObject* peek(bool enable = true);

        /*@jsdoc
         * Processes the route only if a condition is satisfied. The condition is evaluated before the route input is read, and 
         * the input is read only if the condition is <code>true</code>. Thus, if the condition is not met then subsequent 
         * routes using the same input are processed.
         * @function RouteObject#when
         * @param {condition|condition[]} expression - <p>A <code>condition</code> may be a:</p>
         *     <ul>
         *         <li>A numeric {@link Controller.Hardware} property, which is evaluated as a boolean.</li>
         *         <li><code>!</code> followed by a {@link Controller.Hardware} property to use the logical NOT of the property 
         *         value.</li>
         *         <li>A script function returning a boolean value. This can be either the name of the function or an in-line 
         *         definition.</li>
         *     </ul>
         * <p>If an array of conditions is provided, their values are ANDed together.</p>
         * <p><strong>Warning:</strong> The use of <code>!</code> is not currently supported in JavaScript <code>.when()</code> 
         * calls.</p>
         * @returns {RouteObject} The <code>RouteObject</code> with the condition added.
         * @example <caption>Process the right trigger differently in HMD and desktop modes.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         *
         * // Processed only if in HMD mode.
         * mapping.from(Controller.Standard.RT)
         *     .when(Controller.Hardware.Application.InHMD)
         *     .to(function () { print("Trigger pressed in HMD mode."); });
         *
         * // Processed only if previous route not processed.
         * mapping.from(Controller.Standard.RT)
         *     .to(function () { print("Trigger pressed in desktop mode."); });
         *
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* when(const ScriptValuePointer& expression);

        /*@jsdoc
         * Filters numeric route values to lie between two values; values outside this range are not passed on through the 
         * route.
         * @function RouteObject#clamp
         * @param {number} min - The minimum value to pass through.
         * @param {number} max - The maximum value to pass through.
         * @returns {RouteObject} The route object with the clamp filter added.
         * @example <caption>Clamp right trigger values to between 0.3 and 0.7.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RT).clamp(0.3, 0.7).to(function (value) {
         *     print("Value: " + value);
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* clamp(float min, float max);

        /*@jsdoc
         * Filters numeric route values such that they are rounded to <code>0</code> or <code>1</code> without output values 
         * flickering when the input value hovers around <code>0.5</code>. For example, this enables you to use an analog input 
         * as if it were a toggle.
         * @function RouteObject#hysteresis
         * @param {number} min - When the input value drops below this value the output value changes to <code>0</code>.
         * @param {number} max - When the input value rises above this value the output value changes to <code>1</code>.
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Round the right joystick forward/back values to 0 or 1 with hysteresis.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RY).peek().to(function (value) {
         *     print("Raw value: " + value);  // 0.0 - 1.0.
         * });
         * mapping.from(Controller.Standard.RY).hysteresis(0.3, 0.7).to(function (value) {
         *     print("Hysteresis value: " + value);  // 0 or 1.
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* hysteresis(float min, float max);

        /*@jsdoc
         * Filters numeric route values to send at a specified interval.
         * @function RouteObject#pulse
         * @param {number} interval - The interval between sending values, in seconds.
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Send right trigger values every half second.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RT).pulse(0.5).to(function (value) {
         *     print("Value: " + value);
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* pulse(float interval);

        /*@jsdoc
         * Filters numeric and {@link Pose} route values to be scaled by a constant amount.
         * @function RouteObject#scale
         * @param {number} multiplier - The scale to multiply the value by.
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Scale the value of the right joystick forward/back values by 10.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.LY).to(function (value) {
         *     print("L value: " + value);  // -1.0 to 1.0 values.
         * });
         * mapping.from(Controller.Standard.RY).scale(10.0).to(function (value) {
         *     print("R value: " + value);  // -10.0 to -10.0 values.
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* scale(float multiplier);

        /*@jsdoc
         * Filters numeric and {@link Pose} route values to have the opposite sign, e.g., <code>0.5</code> is changed to 
         * <code>-0.5</code>.
         * @function RouteObject#invert
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Invert the value of the right joystick forward/back values.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.LY).to(function (value) {
         *     print("L value: " + value);  // -1.0 to 1.0 values, forward to back.
         * });
         * mapping.from(Controller.Standard.RY).invert().to(function (value) {
         *     print("R value: " + value);  // 1.0 to -1.0 values, forward to back.
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* invert();

        /*@jsdoc
         * Filters numeric route values such that they're sent only when the input value is outside a dead-zone. When the input 
         * passes the dead-zone value, output is sent starting at <code>0.0</code> and catching up with the input value. As the 
         * input returns toward the dead-zone value, output values reduce to <code>0.0</code> at the dead-zone value.
         * @function RouteObject#deadZone
         * @param {number} min - The minimum input value at which to start sending output. For negative input values, the 
         *    negative of this value is used.
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Apply a dead-zone to the right joystick forward/back values.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RY).deadZone(0.2).to(function (value) {
         *     print("Value: " + value);  // 0.0 - 1.0 values once outside the dead-zone.
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* deadZone(float min);

        /*@jsdoc
         * Filters numeric route values such that they are rounded to <code>-1</code>, <code>0</code>, or <code>1</code>.
         * For example, this enables you to use an analog input as if it were a toggle or, in the case of a bidirectional axis, 
         * a tri-state switch.
         * @function RouteObject#constrainToInteger
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Round the right joystick forward/back values to <code>-1</code>, <code>0</code>, or 
         *     <code>1</code>.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RY).constrainToInteger().to(function (value) {
         *     print("Value: " + value);  // -1, 0, or 1
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* constrainToInteger();

        /*@jsdoc
         * Filters numeric route values such that they are rounded to <code>0</code> or <code>1</code>. For example, this 
         * enables you to use an analog input as if it were a toggle.
         * @function RouteObject#constrainToPositiveInteger
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Round the right joystick forward/back values to <code>0</code> or <code>1</code>.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * mapping.from(Controller.Standard.RY).constrainToPositiveInteger().to(function (value) {
         *     print("Value: " + value);  // 0, or 1
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* constrainToPositiveInteger();

        /*@jsdoc
         * Filters {@link Pose} route values to have a pre-translation applied.
         * @function RouteObject#translate
         * @param {Vec3} translate - The pre-translation to add to the pose.
         * @returns {RouteObject} The <code>RouteObject</code> with the pre-translation applied.
         */
        // No JSDoc example because filter not currently used.
        Q_INVOKABLE QObject* translate(glm::vec3 translate);

        /*@jsdoc
         * Filters {@link Pose} route values to have a pre-transform applied.
         * @function RouteObject#transform
         * @param {Mat4} transform - The pre-transform to apply.
         * @returns {RouteObject} The <code>RouteObject</code> with the pre-transform applied.
         */
        // No JSDoc example because filter not currently used.
        Q_INVOKABLE QObject* transform(glm::mat4 transform);

        /*@jsdoc
         * Filters {@link Pose} route values to have a post-transform applied.
         * @function RouteObject#postTransform
         * @param {Mat4} transform - The post-transform to apply.
         * @returns {RouteObject} The <code>RouteObject</code> with the post-transform applied.
         */
        // No JSDoc example because filter not currently used.
        Q_INVOKABLE QObject* postTransform(glm::mat4 transform);

        /*@jsdoc
         * Filters {@link Pose} route values to have a pre-rotation applied.
         * @function RouteObject#rotate
         * @param {Quat} rotation - The pre-rotation to add to the pose.
         * @returns {RouteObject} The <code>RouteObject</code> with the pre-rotation applied.
         */
        // No JSDoc example because filter not currently used.
        Q_INVOKABLE QObject* rotate(glm::quat rotation);

        /*@jsdoc
         * Filters {@link Pose} route values to be smoothed by a low velocity filter. The filter's rotation and translation 
         * values are calculated as: <code>(1 - f) * currentValue + f * previousValue</code> where 
         * <code>f = currentVelocity / filterConstant</code>. At low velocities, the filter value is largely the previous 
         * value; at high velocities the value is wholly the current controller value.
         * @function RouteObject#lowVelocity
         * @param {number} rotationConstant - The rotational velocity, in rad/s, at which the filter value is wholly the latest 
         *     controller value.
         * @param {number} translationConstant - The linear velocity, in m/s, at which the filter value is wholly the latest 
         *     controller value.
         * @returns {RouteObject} The <code>RouteObject</code> smoothed by low velocity filtering.
         */
        // No JSDoc example because filter not currently used.
        Q_INVOKABLE QObject* lowVelocity(float rotationConstant, float translationConstant);

        /*@jsdoc
         * Filters {@link Pose} route values to be smoothed by an exponential decay filter. The filter's rotation and 
         * translation values are calculated as: <code>filterConstant * currentValue + (1 - filterConstant) * 
         * previousValue</code>. Values near 1 are less smooth with lower latency; values near 0 are more smooth with higher 
         * latency.
         * @function RouteObject#exponentialSmoothing
         * @param {number} rotationConstant - Rotation filter constant, <code>0.0&ndash;1.0</code>.
         * @param {number} translationConstant - Translation filter constant, <code>0.0&ndash;1.0</code>.
         * @returns {RouteObject} The <code>RouteObject</code> smoothed by an exponential filter.
         */
        // No JSDoc example because filter used only in Vive.json.
        Q_INVOKABLE QObject* exponentialSmoothing(float rotationConstant, float translationConstant);

        /*@jsdoc
         * Filters numeric route values such that a value of <code>0.0</code> is changed to <code>1.0</code>, and other values 
         * are changed to <code>0.0</code>.
         * @function RouteObject#logicalNot
         * @returns {RouteObject} The <code>RouteObject</code> with the filter applied.
         * @example <caption>Logical NOT of LSTouch value.</caption>
         * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
         * var mapping = Controller.newMapping(MAPPING_NAME);
         * 
         * mapping.from(Controller.Standard.RSTouch).peek().to(function (value) {
         *     print("RSTouch: " + value);
         * });
         * mapping.from(Controller.Standard.RSTouch).logicalNot().to(function (value) {
         *     print("localNot of RSTouch: " + value);
         * });
         * Controller.enableMapping(MAPPING_NAME);
         *
         * Script.scriptEnding.connect(function () {
         *     Controller.disableMapping(MAPPING_NAME);
         * });
         */
        Q_INVOKABLE QObject* logicalNot();

    private:
        void to(const Endpoint::Pointer& destination);
        void conditional(const Conditional::Pointer& conditional);
        void addFilter(Filter::Pointer filter);
        UserInputMapper& _parent;
        Mapping::Pointer _mapping;
        Route::Pointer _route;
    };

}
#endif
