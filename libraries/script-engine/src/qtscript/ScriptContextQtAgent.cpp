//
//  ScriptContextQtAgent.cpp
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/22/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptContextQtAgent.h"

#include <QtScript/QScriptEngine>

#include "../Scriptable.h"
#include "ScriptContextQtWrapper.h"
#include "ScriptEngineQtScript.h"

void ScriptContextQtAgent::contextPop() {
    if (_prevAgent) {
        _prevAgent->contextPop();
    }
    if (_engine->currentContext() == _currContext) {
        _currContext.reset();
        if (!_contextActive && !_contextStack.empty()) {
            _currContext = _contextStack.back();
            _contextStack.pop_back();
            _contextActive = true;
        }
    }
}

void ScriptContextQtAgent::contextPush() {
    if (_prevAgent) {
        _prevAgent->contextPush();
    }
    if (_contextActive && _currContext) {
        _contextStack.push_back(_currContext);
        _contextActive = false;
    }
    _currContext.reset();
}

void ScriptContextQtAgent::functionEntry(qint64 scriptId) {
    if (_prevAgent) {
        _prevAgent->functionEntry(scriptId);
    }
    if (scriptId != -1) {
        return;
    }
    if (!_currContext) {
        _currContext = ScriptContextQtPointer(new ScriptContextQtWrapper(_engine, static_cast<QScriptEngine*>(_engine)->currentContext()));
    }
    Scriptable::setContext(_currContext.get());
    _contextActive = true;
}

void ScriptContextQtAgent::functionExit(qint64 scriptId, const QScriptValue& returnValue) {
    if (_prevAgent) {
        _prevAgent->functionExit(scriptId, returnValue);
    }
    if (scriptId != -1) {
        return;
    }
    _contextActive = false;
    if (!_contextActive && !_contextStack.empty()) {
        _currContext = _contextStack.back();
        _contextStack.pop_back();
        Scriptable::setContext(_currContext.get());
        _contextActive = true;
    }
}
