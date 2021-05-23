//
//  LocationScriptingInterface.h
//  libraries/networking/src
//
//  Created by Ryan Huffman on 4/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_LocationScriptingInterface_h
#define hifi_LocationScriptingInterface_h

#include <QtCore/QSharedPointer>

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class LocationScriptingInterface : public QObject {
    Q_OBJECT
public:
    static LocationScriptingInterface* getInstance();

    static ScriptValuePointer locationGetter(ScriptContext* context, ScriptEngine* engine);
    static ScriptValuePointer locationSetter(ScriptContext* context, ScriptEngine* engine);
private:
    LocationScriptingInterface() {};
};

#endif // hifi_LocationScriptingInterface_h
