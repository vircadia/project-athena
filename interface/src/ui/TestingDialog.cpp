//
//  TestingDialog.cpp
//  interface/src/ui
//
//  Created by Ryan Jones on 12/3/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TestingDialog.h"

#include "Application.h"
#include "ScriptEngines.h"
#include <ScriptManager.h>

TestingDialog::TestingDialog(QWidget* parent) :
    QDialog(parent, Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint),
    _console(new JSConsole(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(windowLabel);

    _console->setFixedHeight(TESTING_CONSOLE_HEIGHT);

    _manager = DependencyManager::get<ScriptEngines>()->loadScript(qApp->applicationDirPath() + testRunnerRelativePath);
    _console->setScriptManager(_manager);
    connect(_manager.data(), &ScriptManager::finished, this, &TestingDialog::onTestingFinished);
}

void TestingDialog::onTestingFinished(const QString& scriptPath) {
    _manager.reset();
    _console->setScriptManager();
}
