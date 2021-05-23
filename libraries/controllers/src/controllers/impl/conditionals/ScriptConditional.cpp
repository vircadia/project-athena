//
//  Created by Bradley Austin Davis 2015/10/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptConditional.h"

#include <QtCore/QThread>

#include <ScriptValue.h>

using namespace controller;

bool ScriptConditional::satisfied() {
    updateValue();
    return _lastValue;
}

void ScriptConditional::updateValue() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updateValue", Qt::QueuedConnection);
        return;
    }

    _lastValue = _callable->call()->toBool();
}
