//
//  RecurseOctreeToMapOperator.cpp
//  libraries/entities/src
//
//  Created by Seth Alves on 3/16/15.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RecurseOctreeToMapOperator.h"

#include "EntityItemProperties.h"
#include <ScriptValue.h>

RecurseOctreeToMapOperator::RecurseOctreeToMapOperator(QVariantMap& map,
                                                       const OctreeElementPointer& top,
                                                       ScriptEngine* engine,
                                                       bool skipDefaultValues,
                                                       bool skipThoseWithBadParents,
                                                       std::shared_ptr<AvatarData> myAvatar) :
        RecurseOctreeOperator(),
        _map(map),
        _top(top),
        _engine(engine),
        _skipDefaultValues(skipDefaultValues),
        _skipThoseWithBadParents(skipThoseWithBadParents),
        _myAvatar(myAvatar)
{
    // if some element "top" was given, only save information for that element and its children.
    if (_top) {
        _withinTop = false;
    } else {
        // top was NULL, export entire tree.
        _withinTop = true;
    }
};

bool RecurseOctreeToMapOperator::preRecursion(const OctreeElementPointer& element) {
    if (element == _top) {
        _withinTop = true;
    }
    return true;
}

bool RecurseOctreeToMapOperator::postRecursion(const OctreeElementPointer& element) {

    EntityItemProperties defaultProperties;

    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
    QVariantList entitiesQList = qvariant_cast<QVariantList>(_map["Entities"]);

    entityTreeElement->forEachEntity([&](EntityItemPointer entityItem) {
        if (_skipThoseWithBadParents && !entityItem->isParentIDValid()) {
            return;  // we weren't able to resolve a parent from _parentID, so don't save this entity.
        }

        EntityItemProperties properties = entityItem->getProperties();
        ScriptValuePointer qScriptValues;
        if (_skipDefaultValues) {
            qScriptValues = EntityItemNonDefaultPropertiesToScriptValue(_engine, properties);
        } else {
            qScriptValues = EntityItemPropertiesToScriptValue(_engine, properties);
        }

        // handle parentJointName for wearables
        if (_myAvatar && entityItem->getParentID() == AVATAR_SELF_ID &&
            entityItem->getParentJointIndex() != INVALID_JOINT_INDEX) {

            auto jointNames = _myAvatar->getJointNames();
            auto parentJointIndex = entityItem->getParentJointIndex();
        	if (parentJointIndex < jointNames.count()) {
                qScriptValues->setProperty("parentJointName", jointNames.at(parentJointIndex));
        	}
        }

        entitiesQList << qScriptValues->toVariant();
    });

    _map["Entities"] = entitiesQList;
    if (element == _top) {
        _withinTop = false;
    }
    return true;
}
