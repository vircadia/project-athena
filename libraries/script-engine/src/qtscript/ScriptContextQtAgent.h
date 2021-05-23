//
//  ScriptContextQtAgent.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/22/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptContextQtAgent_h
#define hifi_ScriptContextQtAgent_h

#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtScript/QScriptEngineAgent>

#include "ScriptEngineQtScript.h"

class QScriptContext;
class QScriptEngine;
class QScriptValue;
class ScriptContextQtWrapper;
using ScriptContextQtPointer = QSharedPointer<ScriptContextQtWrapper>;

class ScriptContextQtAgent : public QScriptEngineAgent {
public: // construction
    inline ScriptContextQtAgent(ScriptEngineQtScript* engine, QScriptEngineAgent* prevAgent) :
        QScriptEngineAgent(engine), _engine(engine), _prevAgent(prevAgent) {}
    virtual ~ScriptContextQtAgent() {}

public: // QScriptEngineAgent implementation
    virtual void contextPop();
    virtual void contextPush();
    virtual void functionEntry(qint64 scriptId);
    virtual void functionExit(qint64 scriptId, const QScriptValue& returnValue);

private: // storage
    bool _contextActive = false;
    QList<ScriptContextQtPointer> _contextStack;
    ScriptContextQtPointer _currContext = nullptr;
    ScriptEngineQtScript* _engine;
    QScriptEngineAgent* _prevAgent;
};

#endif  // hifi_ScriptContextQtAgent_h