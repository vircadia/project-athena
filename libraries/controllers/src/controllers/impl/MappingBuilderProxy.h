//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_Controllers_Impl_MappingBuilderProxy_h
#define hifi_Controllers_Impl_MappingBuilderProxy_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QSharedPointer>

#include "Mapping.h"
#include "Endpoint.h"

class QJSValue;
class QJsonValue;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

namespace controller {

class ScriptingInterface;
class UserInputMapper;

/*@jsdoc
 * <p>A {@link Controller} mapping object that can contain a set of routes that map:</p>
 * <ul>
 *     <li>{@link Controller.Standard} outputs to {@link Controller.Actions} actions or script functions.</li>
 *     <li>{@link Controller.Hardware} outputs to {@link Controller.Standard} outputs, {@link Controller.Actions} actions, or 
 *     script functions.</li>
 * </ul>
 *
 * <p>Create by one of the following methods:</p>
 * <ul>
 *     <li>Use {@link Controller.newMapping} to create the mapping object, add routes using {@link MappingObject#from|from} or
 *     {@link MappingObject#makeAxis|makeAxis}, and map the routes to actions or functions using {@link RouteObject} 
 *     methods.</li>
 *     <li>Use {@link Controller.parseMapping} or {@link Controller.loadMapping} to load a {@link Controller.MappingJSON}.</li>
 * </ul>
 *
 * <p>Enable the mapping using {@link MappingObject#enable|enable} or {@link Controller.enableMapping} for it to take 
 * effect.</p>
 *
 * <p>Mappings and their routes are applied according to the following rules:</p>
 * <ul>
 *     <li>One read per output: after a controller output has been read, it can't be read again. Exception: You can use 
 *     {@link RouteObject#peek} to read a value without marking that output as having been read.</li>
 *     <li>Existing mapping routes take precedence over new mapping routes: within a mapping, if a route is added for a control 
 *     output that already has a route the new route is ignored.</li>
 *     <li>New mappings override previous mappings: each output is processed using the route in the most recently enabled 
 *     mapping that contains that output.</li>
 * </ul>
 *
 * @class MappingObject
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */

/*@jsdoc
 * A {@link MappingObject} can be specified in JSON format. A simple example is provided below. Full examples &mdash; the 
 * default mappings provided in Interface &mdash;  can be found at 
 * <a href="https://github.com/highfidelity/hifi/tree/master/interface/resources/controllers">
 * https://github.com/highfidelity/hifi/tree/master/interface/resources/controllers</a>.
 * @typedef {object} Controller.MappingJSON
 * @property {string} name - The name of the mapping.
 * @property {Controller.MappingJSONRoute[]} channels - An array of routes.
 * @example <caption>A simple mapping JSON that makes the right trigger move your avatar up after a dead zone.</caption>
 * {
 *     "name": "com.highfidelity.controllers.example.jsonMapping",
 *     "channels": [
 *         { 
 *             "from": "Standard.RT", 
 *             "filters": { "type": "deadZone", "min": 0.05 },
 *             "to": "Actions.TranslateY"
 *         }
 *     ]
 * }
 */

/*@jsdoc
 * A route in a {@link Controller.MappingJSON}.
 * @typedef {object} Controller.MappingJSONRoute
 * @property {string|Controller.MappingJSONAxis} from - The name of a {@link Controller.Hardware} property or an axis made from 
 *     them. If a property name, the leading <code>"Controller.Hardware."</code> can be omitted.
 * @property {boolean} [peek=false] - If <code>true</code>, then peeking is enabled per {@link RouteObject#peek}.
 * @property {boolean} [debug=false] - If <code>true</code>, then debug is enabled per {@link RouteObject#debug}.
 * @property {string|string[]} [when=[]] - One or more numeric {@link Controller.Hardware} property names which are evaluated 
 *     as booleans and ANDed together. Prepend a property name with a <code>!</code> to do a logical NOT. The leading 
 *     <code>"Controller.Hardware."</code> can be omitted from the property names.
 * @property {Controller.MappingJSONFilter|Controller.MappingJSONFilter[]} [filters=[]] - One or more filters in the route.
 * @property {string} to - The name of a {@link Controller.Actions} or {@link Controller.Standard} property. The leading 
 *     <code>"Controller."</code> can be omitted.
 */

/*@jsdoc
 * An axis pair in a {@link Controller.MappingJSONRoute}.
 * @typedef {object} Controller.MappingJSONAxis
 * @property {string[][]} makeAxis - A two-member array of single-member arrays of {@link Controller.Hardware} property names. 
 * The leading <code>"Controller.Hardware."</code> can be omitted from the property names.
 * @example <caption>An axis using the keyboard's left and right keys.</caption>
 * { "makeAxis" : [
 *         ["Keyboard.Left"],
 *         ["Keyboard.Right"]
 *     ]
 * }
 */

/*@jsdoc
 * A filter in a {@link Controller.MappingJSONRoute}.
 * @typedef {object} Controller.MappingJSONFilter
 * @property {string} type - The name of the filter, being the name of the one of the {@link RouteObject}'s filter methods.
 * @property {string} [?] - If the filter method has a first parameter, the property name is the name of that parameter and the 
 *     property value is the value to use.
 * @property {string} [?] - If the filter method has a second parameter, the property name  is the name of that parameter and 
 *     the property value is the value to use.
 * @example <caption>A hysteresis filter.</caption>
 * { 
 *     "type": "hysteresis", 
 *     "min": 0.85, 
 *     "max": 0.9
 * }
 */

// TODO migrate functionality to a MappingBuilder class and make the proxy defer to that 
// (for easier use in both C++ and JS)
class MappingBuilderProxy : public QObject {
    Q_OBJECT
public:
    MappingBuilderProxy(UserInputMapper& parent, Mapping::Pointer mapping)
        : _parent(parent), _mapping(mapping) { }

    /*@jsdoc
     * Creates a new {@link RouteObject} from a controller output, ready to be mapped to a standard control, action, or 
     * function.
     * <p>This is a QML-specific version of {@link MappingObject#from|from}: use this version in QML files.</p>
     * @function MappingObject#fromQml
     * @param {Controller.Standard|Controller.Hardware|function} source - The controller output or function that is the source
     *     of the route data. If a function, it must return a number or a {@link Pose} value as the route data.
     * @returns {RouteObject} A route ready for mapping to an action or function using {@link RouteObject} methods.
     */
    Q_INVOKABLE QObject* fromQml(const QJSValue& source);

    /*@jsdoc
     * Creates a new {@link RouteObject} from two numeric {@link Controller.Hardware} outputs, one applied in the negative 
     * direction and the other in the positive direction, ready to be mapped to a standard control, action, or function.
     * <p>This is a QML-specific version of {@link MappingObject#makeAxis|makeAxis}: use this version in QML files.</p>
     * @function MappingObject#makeAxisQml
     * @param {Controller.Hardware} source1 - The first, negative-direction controller output.
     * @param {Controller.Hardware} source2 - The second, positive-direction controller output.
     * @returns {RouteObject} A route ready for mapping to an action or function using {@link RouteObject} methods. The data 
     *     value passed to the route is the combined value of <code>source2 - source1</code>.
     */
    Q_INVOKABLE QObject* makeAxisQml(const QJSValue& source1, const QJSValue& source2);

    /*@jsdoc
     * Creates a new {@link RouteObject} from a controller output, ready to be mapped to a standard control, action, or 
     * function.
     * @function MappingObject#from
     * @param {Controller.Standard|Controller.Hardware|function} source - The controller output or function that is the source 
     *     of the route data. If a function, it must return a number or a {@link Pose} value as the route data.
     * @returns {RouteObject} A route ready for mapping to an action or function using {@link RouteObject} methods.
     */
    Q_INVOKABLE QObject* from(const ScriptValuePointer& source);

    /*@jsdoc
     * Creates a new {@link RouteObject} from two numeric {@link Controller.Hardware} outputs, one applied in the negative 
     * direction and the other in the positive direction, ready to be mapped to a standard control, action, or function.
     * @function MappingObject#makeAxis
     * @param {Controller.Hardware} source1 - The first, negative-direction controller output.
     * @param {Controller.Hardware} source2 - The second, positive-direction controller output.
     * @returns {RouteObject} A route ready for mapping to an action or function using {@link RouteObject} methods. The data
     *     value passed to the route is the combined value of <code>source2 - source1</code>. 
     * @example <caption>Make the Oculus Touch triggers move your avatar up and down.</caption>
     * var MAPPING_NAME = "com.highfidelity.controllers.example.newMapping";
     * var mapping = Controller.newMapping(MAPPING_NAME);
     * mapping
     *     .makeAxis(Controller.Hardware.OculusTouch.LT, Controller.Hardware.OculusTouch.RT)
     *     .to(Controller.Actions.Up);
     * Controller.enableMapping(MAPPING_NAME);
     *
     * Script.scriptEnding.connect(function () {
     *     Controller.disableMapping(MAPPING_NAME);
     * });
     */
    Q_INVOKABLE QObject* makeAxis(const ScriptValuePointer& source1, const ScriptValuePointer& source2);

    /*@jsdoc
     * Enables or disables the mapping. When enabled, the routes in the mapping take effect.
     * <p>Synonymous with {@link Controller.enableMapping}.</p>
     * @function MappingObject#enable
     * @param {boolean} enable=true - If <code>true</code> then the mapping is enabled, otherwise it is disabled.
     * @returns {MappingObject} The mapping object, so that further routes can be added.
     */
    Q_INVOKABLE QObject* enable(bool enable = true);

    /*@jsdoc
     * Disables the mapping. When disabled, the routes in the mapping have no effect.
     * <p>Synonymous with {@link Controller.disableMapping}.</p>
     * @function MappingObject#disable
     * @returns {MappingObject} The mapping object, so that further routes can be added.
     */
    Q_INVOKABLE QObject* disable() { return enable(false); }

protected:
    QObject* from(const Endpoint::Pointer& source);

    friend class RouteBuilderProxy;
    UserInputMapper& _parent;
    Mapping::Pointer _mapping;


    void parseRoute(const QJsonValue& json);

};

}

#endif
