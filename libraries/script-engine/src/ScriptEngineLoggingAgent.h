//
//  ScriptEngineLoggingAgent.h
//  script-engine/src
//
//  Created by Dale Glass on 04/04/2021.
//  Copyright 2021 Vircadia Contributors
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef vircadia_ScriptEngineLoggingAgent_h
#define vircadia_ScriptEngineLoggingAgent_h

#include <QObject>
#include <QStringList>
#include <QScriptEngineAgent>
#include <QThreadStorage>
#include <QLoggingCategory>
#include <mutex>

Q_DECLARE_LOGGING_CATEGORY(script_logger)



class ScriptEngineLoggingAgent : public QScriptEngineAgent {
public:
    ScriptEngineLoggingAgent(QScriptEngine *engine);
    virtual void functionEntry(qint64 scriptId) override;
    virtual void functionExit(qint64 scriptId, const QScriptValue &returnValue) override;
private:
    std::mutex _lock;
};

#endif