//
//  ScriptValueIterator.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/2/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptValueIterator_h
#define hifi_ScriptValueIterator_h

#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include "ScriptValue.h"

class ScriptValueIterator;
using ScriptValueIteratorPointer = QSharedPointer<ScriptValueIterator>;

class ScriptValueIterator {
public:
    virtual ScriptValue::PropertyFlags flags() const = 0;
    virtual bool hasNext() const = 0;
    virtual QString name() const = 0;
    virtual void next() = 0;
    virtual ScriptValuePointer value() const = 0;
};

#endif  // hifi_ScriptValueIterator_h