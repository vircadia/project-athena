//
//  Created by Bradley Austin Davis 2015/10/20
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_Controllers_ScriptConditional_h
#define hifi_Controllers_ScriptConditional_h

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include "../Conditional.h"

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

namespace controller {

class ScriptConditional : public QObject, public Conditional {
    Q_OBJECT;
public:
    ScriptConditional(const ScriptValuePointer& callable) : _callable(callable) {}
    virtual bool satisfied() override;
protected:
    Q_INVOKABLE void updateValue();
private:
    ScriptValuePointer _callable;
    bool _lastValue { false };
};

}
#endif
