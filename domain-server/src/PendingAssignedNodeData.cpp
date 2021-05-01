//
//  PendingAssignedNodeData.cpp
//  domain-server/src
//
//  Created by Stephen Birarda on 2014-05-20.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PendingAssignedNodeData.h"

PendingAssignedNodeData::PendingAssignedNodeData(const QUuid& assignmentUUID, const QString& nodeVersion) :
    _assignmentUUID(assignmentUUID),
    _nodeVersion(nodeVersion)
{

}
