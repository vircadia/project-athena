//
//  AbstractScriptingServicesInterface.h
//  interface/src/script-engine
//
//  Created by Brad Hefta-Gaub on 12/16/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AbstractScriptingServicesInterface_h
#define hifi_AbstractScriptingServicesInterface_h

#include <QtCore/QSharedPointer>

class ScriptManager;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;

/// Interface provided by Application to other objects that need access to scripting services of the application
class AbstractScriptingServicesInterface {
public:
    /// Registers application specific services with a script engine.
    virtual void registerScriptEngineWithApplicationServices(const ScriptManagerPointer& scriptEngine) = 0;
};


#endif // hifi_AbstractScriptingServicesInterface_h
