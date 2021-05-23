//
//  RecurseOctreeToJSONOperator.h
//  libraries/entities/src
//
//  Created by Simon Walton on Oct 11, 2018.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityTree.h"

#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class RecurseOctreeToJSONOperator : public RecurseOctreeOperator {
public:
    RecurseOctreeToJSONOperator(const OctreeElementPointer&, ScriptEngine* engine, QString jsonPrefix = QString(), bool skipDefaults = true,
        bool skipThoseWithBadParents = false);
    virtual bool preRecursion(const OctreeElementPointer& element) override { return true; };
    virtual bool postRecursion(const OctreeElementPointer& element) override;

    QString getJson() const { return _json; }

private:
    void processEntity(const EntityItemPointer& entity);

    ScriptEngine* _engine;
    ScriptValuePointer _toStringMethod;

    QString _json;
    const bool _skipDefaults;
    bool _skipThoseWithBadParents;
    bool _comma { false };
};
