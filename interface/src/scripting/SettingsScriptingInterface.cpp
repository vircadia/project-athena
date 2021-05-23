//
//  SettingsScriptingInterface.cpp
//  interface/src/scripting
//
//  Created by Brad Hefta-Gaub on 2/25/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SettingsScriptingInterface.h"

#include <SettingHandle.h>

SettingsScriptingInterface* SettingsScriptingInterface::getInstance() {
    static SettingsScriptingInterface sharedInstance;
    return &sharedInstance;
}

QVariant SettingsScriptingInterface::getValue(const QString& setting) {
    QVariant value = Setting::Handle<QVariant>(setting).get();
    if (!value.isValid()) {
        value = "";
    }
    return value;
}

QVariant SettingsScriptingInterface::getValue(const QString& setting, const QVariant& defaultValue) {
    QVariant value = Setting::Handle<QVariant>(setting, defaultValue).get();
    if (!value.isValid()) {
        value = "";
    }
    return value;
}

void SettingsScriptingInterface::setValue(const QString& setting, const QVariant& value) {
    if (getValue(setting) == value) {
        return;
    }
    if (setting.startsWith("private/")) {
        if (_restrictPrivateValues) {
            qWarning() << "SettingsScriptingInterface::setValue -- restricted write: " << setting << value;
            return;
        } else {
            qInfo() << "SettingsScriptingInterface::setValue -- allowing restricted write: " << setting << value;
        }
    }
    // Make a deep-copy of the string.
    // Dangling pointers can occur with QStrings that are implicitly shared from a ScriptEngine.
    QString deepCopy = QString::fromUtf16(setting.utf16());
    Setting::Handle<QVariant>(deepCopy).set(value);
    emit valueChanged(setting, value);
}
