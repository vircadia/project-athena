//
//  TestingDialog.h
//  interface/src/ui
//
//  Created by Ryan Jones on 12/3/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TestingDialog_h
#define hifi_TestingDialog_h

#include <QDialog>
#include <QtCore/QSharedPointer>
#include "JSConsole.h"

class ScriptManager;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;

const QString windowLabel = "Client Script Tests";
const QString testRunnerRelativePath = "/scripts/developer/tests/unit_tests/testRunner.js";
const unsigned int TESTING_CONSOLE_HEIGHT = 400;

class TestingDialog : public QDialog {
    Q_OBJECT
public:
    TestingDialog(QWidget* parent);

    void onTestingFinished(const QString& scriptPath);

private:
    std::unique_ptr<JSConsole> _console;
    ScriptManagerPointer _manager;
};

#endif
