//
//  ConsoleScriptingInterface.h
//  libraries/script-engine/src
//
//  Created by NeetBhagat on 6/1/17.
//  Copyright 2014 High Fidelity, Inc.
//
//  ConsoleScriptingInterface is responsible for following functionality
//  Printing logs with various tags and grouping on debug Window and Logs/log file.
//  Debugging functionalities like Timer start-end, assertion, trace.
//  To use these functionalities, use "console" object in Javascript files.
//  For examples please refer "scripts/developer/tests/consoleObjectTest.js"
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_ConsoleScriptingInterface_h
#define hifi_ConsoleScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QSharedPointer>

#include "Scriptable.h"

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>console</code> API provides program logging facilities.
 *
 * @namespace console
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 */
// Scriptable interface of "console" object. Used exclusively in the JavaScript API
class ConsoleScriptingInterface : public QObject, protected Scriptable {
    Q_OBJECT
public:

    /*@jsdoc
     * Logs an "INFO" message to the program log and triggers {@link Script.infoMessage}. 
     * The message logged is "INFO -" followed by the message values separated by spaces.
     * @function console.info
     * @param {...*} [message] - The message values to log.
     */
    static ScriptValuePointer info(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs a message to the program log and triggers {@link Script.printedMessage}.
     * The message logged is the message values separated by spaces.
     * <p>If a {@link console.group} is in effect, the message is indented by an amount proportional to the group level.</p>
     * @function console.log
     * @param {...*} [message] - The message values to log.
     * @example <caption>Log some values.</caption>
     * Script.printedMessage.connect(function (message, scriptName) {
     *     console.info("Console.log message:", "\"" + message + "\"", "in", "[" + scriptName + "]");
     * });
     * 
     * console.log("string", 7, true);
     *
     * // string 7 true
     * // INFO - Console.log message: "string 7 true" in [console.js]
     */
    static ScriptValuePointer log(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs a message to the program log and triggers {@link Script.printedMessage}.
     * The message logged is the message values separated by spaces.
     * @function console.debug
     * @param {...*} [message] - The message values to log.
     */
    static ScriptValuePointer debug(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs a "WARNING" message to the program log and triggers {@link Script.warningMessage}.
     * The message logged is "WARNING - " followed by the message values separated by spaces.
     * @function console.warn
     * @param {...*} [message] - The message values to log.
     */
    static ScriptValuePointer warn(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs an "ERROR" message to the program log and triggers {@link Script.errorMessage}.
     * The message logged is "ERROR - " followed by the message values separated by spaces.
     * @function console.error
     * @param {...*} [message] - The message values to log.
     */
    static ScriptValuePointer error(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * A synonym of {@link console.error}.
     * Logs an "ERROR" message to the program log and triggers {@link Script.errorMessage}.
     * The message logged is "ERROR - " followed by the message values separated by spaces.
     * @function console.exception
     * @param {...*} [message] - The message values to log.
     */
    static ScriptValuePointer exception(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs an "ERROR" message to the program log and triggers {@link Script.errorMessage}, if a test condition fails.
     * The message logged is "ERROR - Assertion failed : " followed by the message values separated by spaces.
     * <p>Note: Script execution continues whether or not the test condition fails.</p>
     * @function console.assert
     * @param {boolean} assertion - The test condition value.
     * @param {...*} [message] - The message values to log if the assertion evaluates to <code>false</code>.
     * @example <caption>Demonstrate assertion behavior.</caption>
     * Script.errorMessage.connect(function (message, scriptName) {
     *     console.info("Error message logged:", "\"" + message + "\"", "in", "[" + scriptName + "]");
     * });
     * 
     * console.assert(5 === 5, "5 equals 5.", "This assertion will succeed.");  // No log output.
     * console.assert(5 > 6, "5 is not greater than 6.", "This assertion will fail.");  // Log output is generated.
     * console.info("Script continues running.");
     *
     * // ERROR - Assertion failed :  5 is not greater than 6. This assertion will fail.
     * // INFO - Error message logged: "Assertion failed :  5 is not greater than 6. This assertion will fail." in [console.js]
     * // INFO - Script continues running.
     */
    // Note: Is registered in the script engine as "assert"
    static ScriptValuePointer assertion(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Logs a message to the program log and triggers {@link Script.printedMessage}, then starts indenting subsequent 
     * {@link console.log} messages until a {@link console.groupEnd}. Groups may be nested.
     * @function console.group
     * @param {*} message - The message value to log.
     * @example <caption>Nested groups.</caption>
     * console.group("Group 1");
     * console.log("Sentence 1");
     * console.log("Sentence 2");
     * console.group("Group 2");
     * console.log("Sentence 3");
     * console.log("Sentence 4");
     * console.groupEnd();
     * console.log("Sentence 5");
     * console.groupEnd();
     * console.log("Sentence 6");
     *
     * //Group 1
     * //   Sentence 1
     * //   Sentence 2
     * //   Group 2
     * //      Sentence 3
     * //      Sentence 4
     * //   Sentence 5
     * //Sentence 6
     */
    static ScriptValuePointer group(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Has the same behavior as {@link console.group}.
     * Logs a message to the program log and triggers {@link Script.printedMessage}, then starts indenting subsequent
     * {@link console.log} messages until a {@link console.groupEnd}. Groups may be nested.
     * @function console.groupCollapsed
     * @param {*} message - The message value to log.
     */
    static ScriptValuePointer groupCollapsed(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Finishes a group of indented {@link console.log} messages.
     * @function console.groupEnd
     */
    static ScriptValuePointer groupEnd(ScriptContext* context, ScriptEngine* engine);

public slots:
    
    /*@jsdoc
     * Starts a timer, logs a message to the program log, and triggers {@link Script.printedMessage}. The message logged has 
     * the timer name and "Timer started".
     * @function console.time
     * @param {string} name - The name of the timer.
     * @example <caption>Time some processing.</caption>
     * function sleep(milliseconds) {
     *     var start = new Date().getTime();
     *     for (var i = 0; i < 1e7; i++) {
     *         if ((new Date().getTime() - start) > milliseconds) {
     *             break;
     *         }
     *     }
     * }
     * 
     * console.time("MyTimer");
     * sleep(1000);  // Do some processing.
     * console.timeEnd("MyTimer");
     *
     * // MyTimer: Timer started
     * // MyTimer: 1001ms     */
    void time(QString labelName);

    /*@jsdoc
     * Finishes a timer, logs a message to the program log, and triggers {@link Script.printedMessage}. The message logged has 
     * the timer name and the number of milliseconds since the timer was started.
     * @function console.timeEnd
     * @param {string} name - The name of the timer.
     */
    void timeEnd(QString labelName);

    /*@jsdoc
     * Logs the JavaScript call stack at the point of call to the program log.
     * @function console.trace
     */
    void trace();

    /*@jsdoc
     * Clears the Developer &gt; Scripting &gt; Script Log debug window.
     * @function console.clear
     */
    void clear();

private:    
    QHash<QString, QDateTime> _timerDetails;
    static QList<QString> _groupDetails;
    static void logGroupMessage(QString message, ScriptEngine* engine);
    static QString appendArguments(ScriptContext* context);
};

#endif // hifi_ConsoleScriptingInterface_h
