//
//  Created by Bradley Austin Davis on 2014/04/13.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "OculusDebugDisplayPlugin.h"
#include <QtCore/QProcessEnvironment>

const char* OculusDebugDisplayPlugin::NAME { "Oculus Rift (Simulator)" };

static const QString DEBUG_FLAG("VIRCADIA_DEBUG_OCULUS");
static bool enableDebugOculus = true || QProcessEnvironment::systemEnvironment().contains("VIRCADIA_DEBUG_OCULUS");

bool OculusDebugDisplayPlugin::isSupported() const {
    if (!enableDebugOculus) {
        return false;
    }
    return OculusBaseDisplayPlugin::isSupported();
}
