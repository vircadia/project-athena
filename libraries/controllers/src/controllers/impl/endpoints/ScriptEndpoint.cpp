//
//  Created by Bradley Austin Davis 2015/10/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptEndpoint.h"
#include "../../Logging.h"

#include <QtCore/QThread>

#include <ScriptEngine.h>
#include <ScriptValue.h>
#include <StreamUtils.h>

using namespace controller;

QString formatException(const ScriptValuePointer& exception) {
    QString note { "UncaughtException" };
    QString result;

    const auto message = exception->toString();
    const auto fileName = exception->property("fileName")->toString();
    const auto lineNumber = exception->property("lineNumber")->toString();
    const auto stacktrace = exception->property("stack")->toString();

    const QString SCRIPT_EXCEPTION_FORMAT = "[%0] %1 in %2:%3";
    const QString SCRIPT_BACKTRACE_SEP = "\n    ";

    result = QString(SCRIPT_EXCEPTION_FORMAT).arg(note, message, fileName, lineNumber);
    if (!stacktrace.isEmpty()) {
        result += QString("\n[Backtrace]%1%2").arg(SCRIPT_BACKTRACE_SEP).arg(stacktrace);
    }
    return result;
}

AxisValue ScriptEndpoint::peek() const {
    const_cast<ScriptEndpoint*>(this)->updateValue();
    return AxisValue(_lastValueRead, 0);
}

void ScriptEndpoint::updateValue() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updateValue", Qt::QueuedConnection);
        return;
    }

    ScriptValuePointer result = _callable->call();
    if (result->isError()) {
        // print JavaScript exception
        qCDebug(controllers).noquote() << formatException(result);
        _lastValueRead = 0.0f;
    } else if (result->isNumber()) {
        _lastValueRead = (float)_callable->call()->toNumber();
    } else {
        Pose::fromScriptValue(result, _lastPoseRead);
        _returnPose = true;
    }
}

void ScriptEndpoint::apply(AxisValue value, const Pointer& source) {
    if (value == _lastValueWritten) {
        return;
    }
    _lastValueWritten = value;
    internalApply(value.value, source->getInput().getID());
}

void ScriptEndpoint::internalApply(float value, int sourceID) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "internalApply", Qt::QueuedConnection,
            Q_ARG(float, value),
            Q_ARG(int, sourceID));
        return;
    }
    ScriptEnginePointer engine = _callable->engine();
    ScriptValuePointer result = _callable->call(ScriptValuePointer(),
        ScriptValueList({ engine->newValue(value), engine->newValue(sourceID) }));
    if (result->isError()) {
        // print JavaScript exception
        qCDebug(controllers).noquote() << formatException(result);
    }
}

Pose ScriptEndpoint::peekPose() const {
    const_cast<ScriptEndpoint*>(this)->updatePose();
    return _lastPoseRead;
}

void ScriptEndpoint::updatePose() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updatePose", Qt::QueuedConnection);
        return;
    }
    ScriptValuePointer result = _callable->call();
    if (result->isError()) {
        // print JavaScript exception
        qCDebug(controllers).noquote() << formatException(result);
    }
    Pose::fromScriptValue(result, _lastPoseRead);
}

void ScriptEndpoint::apply(const Pose& newPose, const Pointer& source) {
    if (newPose == _lastPoseWritten) {
        return;
    }
    internalApply(newPose, source->getInput().getID());
}

void ScriptEndpoint::internalApply(const Pose& newPose, int sourceID) {
    _lastPoseWritten = newPose;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "internalApply", Qt::QueuedConnection,
            Q_ARG(const Pose&, newPose),
            Q_ARG(int, sourceID));
        return;
    }
    ScriptEnginePointer engine = _callable->engine();
    ScriptValuePointer result = _callable->call(ScriptValuePointer(),
        ScriptValueList({ Pose::toScriptValue(engine.get(), newPose), engine->newValue(sourceID) }));
    if (result->isError()) {
        // print JavaScript exception
        qCDebug(controllers).noquote() << formatException(result);
    }
}
