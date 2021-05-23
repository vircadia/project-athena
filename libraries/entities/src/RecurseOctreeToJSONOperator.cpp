//
//  RecurseOctreeToJSONOperator.cpp
//  libraries/entities/src
//
//  Created by Simon Walton on Oct 11, 2018.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RecurseOctreeToJSONOperator.h"
#include "EntityItemProperties.h"
#include <ScriptValue.h>

RecurseOctreeToJSONOperator::RecurseOctreeToJSONOperator(const OctreeElementPointer&, ScriptEngine* engine,
    QString jsonPrefix, bool skipDefaults, bool skipThoseWithBadParents):
    _engine(engine),
    _json(jsonPrefix),
    _skipDefaults(skipDefaults),
    _skipThoseWithBadParents(skipThoseWithBadParents)
{
    _toStringMethod = _engine->evaluate("(function() { return JSON.stringify(this, null, '    ') })");
}

bool RecurseOctreeToJSONOperator::postRecursion(const OctreeElementPointer& element) {
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);

    entityTreeElement->forEachEntity([&](const EntityItemPointer& entity) { processEntity(entity); } );
    return true;
}

void RecurseOctreeToJSONOperator::processEntity(const EntityItemPointer& entity) {
    if (_skipThoseWithBadParents && !entity->isParentIDValid()) {
        return;  // we weren't able to resolve a parent from _parentID, so don't save this entity.
    }

    ScriptValuePointer qScriptValues = _skipDefaults
        ? EntityItemNonDefaultPropertiesToScriptValue(_engine, entity->getProperties())
        : EntityItemPropertiesToScriptValue(_engine, entity->getProperties());

    if (_comma) {
        _json += ',';
    };
    _comma = true;
    _json += "\n    ";

    // Override default toString():
    qScriptValues->setProperty("toString", _toStringMethod);
    _json += qScriptValues->toString();
}
