//
//  Created by Bradley Austin Davis on 2015-12-15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "QmlWebWindowClass.h"

#include <QtCore/QThread>

#include <ScriptContext.h>
#include <ScriptEngine.h>
#include <ScriptManager.h>

#include <shared/QtHelpers.h>

static const char* const URL_PROPERTY = "source";
static const char* const SCRIPT_PROPERTY = "scriptUrl";

// Method called by Qt scripts to create a new web window in the overlay
ScriptValuePointer QmlWebWindowClass::internal_constructor(ScriptContext* context, ScriptEngine* engine, bool restricted) {
    auto properties = parseArguments(context);
    QmlWebWindowClass* retVal = new QmlWebWindowClass(restricted);
    Q_ASSERT(retVal);
    if (QThread::currentThread() != qApp->thread()) {
        retVal->moveToThread(qApp->thread());
        QMetaObject::invokeMethod(retVal, "initQml", Q_ARG(QVariantMap, properties));
    } else {
        retVal->initQml(properties);
    }
    auto manager = engine->manager();
    connect(manager, &ScriptManager::destroyed, retVal, &QmlWindowClass::deleteLater);
    return engine->newQObject(retVal);
}

QString QmlWebWindowClass::getURL() {
    if (QThread::currentThread() != thread()) {
        QString result;
        BLOCKING_INVOKE_METHOD(this, "getURL", Q_RETURN_ARG(QString, result));
        return result;
    }

    if (_qmlWindow.isNull()) {
        return QString();
    }
        
    return _qmlWindow->property(URL_PROPERTY).toString();
}

void QmlWebWindowClass::setURL(const QString& urlString) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setURL", Q_ARG(QString, urlString));
        return;
    }

    if (!_qmlWindow.isNull()) {
        _qmlWindow->setProperty(URL_PROPERTY, urlString);
    }
}

void QmlWebWindowClass::setScriptURL(const QString& script) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setScriptURL", Q_ARG(QString, script));
        return;
    }

    if (!_qmlWindow.isNull()) {
        _qmlWindow->setProperty(SCRIPT_PROPERTY, script);
    }
}
