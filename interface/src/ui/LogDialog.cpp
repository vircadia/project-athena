//
//  LogDialog.cpp
//  interface/src/ui
//
//  Created by Stojce Slavkovski on 12/12/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LogDialog.h"

#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>

#include <shared/AbstractLoggerInterface.h>

#include "Application.h"
#include "MainWindow.h"

const int REVEAL_BUTTON_WIDTH = 122;
const int ALL_LOGS_BUTTON_WIDTH = 90;
const int MARGIN_LEFT = 25;
const int DEBUG_CHECKBOX_WIDTH = 70;
const int INFO_CHECKBOX_WIDTH = 65;
const int CRITICAL_CHECKBOX_WIDTH = 85;
const int WARNING_CHECKBOX_WIDTH = 80;
const int SUPPRESS_CHECKBOX_WIDTH = 87;
const int FATAL_CHECKBOX_WIDTH = 70;
const int UNKNOWN_CHECKBOX_WIDTH = 80;
const int SECOND_ROW = ELEMENT_MARGIN + ELEMENT_MARGIN + ELEMENT_HEIGHT;
const int THIRD_ROW = SECOND_ROW + ELEMENT_MARGIN + ELEMENT_HEIGHT;
const QString DEBUG_TEXT = "[DEBUG]";
const QString INFO_TEXT = "[INFO]";
const QString CRITICAL_TEXT = "[CRITICAL]";
const QString WARNING_TEXT = "[WARNING]";
const QString FATAL_TEXT = "[FATAL]";
const QString SUPPRESS_TEXT = "[SUPPRESS]";
const QString UNKNOWN_TEXT = "[UNKNOWN]";

LogDialog::LogDialog(QWidget* parent, AbstractLoggerInterface* logger) : BaseLogDialog(parent), _windowGeometry("logDialogGeometry", QRect()) {
    _logger = logger;
    setWindowTitle("Log");

    _revealLogButton = new QPushButton("Reveal log file", this);
    // set object name for css styling
    _revealLogButton->setObjectName("revealLogButton");
    _revealLogButton->show();
    connect(_revealLogButton, &QPushButton::clicked, this, &LogDialog::handleRevealButton);

    connect(_logger, &AbstractLoggerInterface::logReceived, this, &LogDialog::appendLogLine);

    _leftPad = MARGIN_LEFT;
    _debugPrintBox = new QCheckBox("DEBUG", this);
    _debugPrintBox->setGeometry(_leftPad, SECOND_ROW, DEBUG_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->debugPrint()) {
        _debugPrintBox->setCheckState(Qt::Checked);
    }
    _debugPrintBox->show();
    connect(_debugPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleDebugPrintBox);

    _leftPad += DEBUG_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _infoPrintBox = new QCheckBox("INFO", this);
    _infoPrintBox->setGeometry(_leftPad, SECOND_ROW, INFO_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->infoPrint()) {
        _infoPrintBox->setCheckState(Qt::Checked);
    }
    _infoPrintBox->show();
    connect(_infoPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleInfoPrintBox);

    _leftPad += INFO_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _criticalPrintBox = new QCheckBox("CRITICAL", this);
    _criticalPrintBox->setGeometry(_leftPad, SECOND_ROW, CRITICAL_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->criticalPrint()) {
        _criticalPrintBox->setCheckState(Qt::Checked);
    }
    _criticalPrintBox->show();
    connect(_criticalPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleCriticalPrintBox);

    _leftPad += CRITICAL_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _warningPrintBox = new QCheckBox("WARNING", this);
    _warningPrintBox->setGeometry(_leftPad, SECOND_ROW, WARNING_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->warningPrint()) {
        _warningPrintBox->setCheckState(Qt::Checked);
    }
    _warningPrintBox->show();
    connect(_warningPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleWarningPrintBox);

    _leftPad += WARNING_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _suppressPrintBox = new QCheckBox("SUPPRESS", this);
    _suppressPrintBox->setGeometry(_leftPad, SECOND_ROW, SUPPRESS_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->suppressPrint()) {
        _suppressPrintBox->setCheckState(Qt::Checked);
    }
    _suppressPrintBox->show();
    connect(_suppressPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleSuppressPrintBox);

    _leftPad += SUPPRESS_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _fatalPrintBox = new QCheckBox("FATAL", this);
    _fatalPrintBox->setGeometry(_leftPad, SECOND_ROW, FATAL_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->fatalPrint()) {
        _fatalPrintBox->setCheckState(Qt::Checked);
    }
    _fatalPrintBox->show();
    connect(_fatalPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleFatalPrintBox);

    _leftPad += FATAL_CHECKBOX_WIDTH + BUTTON_MARGIN;
    _unknownPrintBox = new QCheckBox("UNKNOWN", this);
    _unknownPrintBox->setGeometry(_leftPad, SECOND_ROW, UNKNOWN_CHECKBOX_WIDTH, ELEMENT_HEIGHT);
    if (_logger->unknownPrint()) {
        _unknownPrintBox->setCheckState(Qt::Checked);
    }
    _unknownPrintBox->show();
    connect(_unknownPrintBox, &QCheckBox::stateChanged, this, &LogDialog::handleUnknownPrintBox);

    _leftPad = MARGIN_LEFT;
    _filterDropdown = new QComboBox(this);
    _filterDropdown->setGeometry(_leftPad, THIRD_ROW, COMBOBOX_WIDTH, ELEMENT_HEIGHT);
    _filterDropdown->addItem("No secondary filter...");
    _filterDropdown->addItem("default");
    _filterDropdown->addItem("hifi.audio");
    _filterDropdown->addItem("hifi.audioclient");
    _filterDropdown->addItem("hifi.animation");
    _filterDropdown->addItem("hifi.avatars");
    _filterDropdown->addItem("hifi.controllers");
    _filterDropdown->addItem("hifi.entities");
    _filterDropdown->addItem("hifi.gl");
    _filterDropdown->addItem("hifi.gpu.gl");
    _filterDropdown->addItem("hifi.interface");
    _filterDropdown->addItem("hifi.interface.deadlock");
    _filterDropdown->addItem("hifi.modelformat");
    _filterDropdown->addItem("hifi.networking");
    _filterDropdown->addItem("hifi.networking.resource");
    _filterDropdown->addItem("hifi.plugins");
    _filterDropdown->addItem("hifi.render");
    _filterDropdown->addItem("hifi.scriptengine");
    _filterDropdown->addItem("hifi.scriptengine.module");
    _filterDropdown->addItem("hifi.scriptengine.script");
    _filterDropdown->addItem("hifi.shared");
    _filterDropdown->addItem("hifi.ui");
    _filterDropdown->addItem("qml");
    connect(_filterDropdown, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &LogDialog::handleFilterDropdownChanged);

    _leftPad += COMBOBOX_WIDTH + MARGIN_LEFT + MARGIN_LEFT;
    _messageCount = new QLabel("", this);
    _messageCount->setObjectName("messageCount");
    _messageCount->show();

    _keepOnTopBox = new QCheckBox(" Keep window on top", this);
    bool isOnTop = qApp-> getLogWindowOnTopSetting();
    _keepOnTopBox->setCheckState(isOnTop ? Qt::Checked : Qt::Unchecked);
#ifdef Q_OS_WIN
    connect(_keepOnTopBox, &QCheckBox::stateChanged, qApp, &Application::recreateLogWindow);
#else
    connect(_keepOnTopBox, &QCheckBox::stateChanged, this, &LogDialog::handleKeepWindowOnTop);
#endif
    _keepOnTopBox->show();

    _extraDebuggingBox = new QCheckBox("Extra debugging", this);
    if (_logger->extraDebugging()) {
        _extraDebuggingBox->setCheckState(Qt::Checked);
    }
    _extraDebuggingBox->show();
    connect(_extraDebuggingBox, &QCheckBox::stateChanged, this, &LogDialog::handleExtraDebuggingCheckbox);
    
    _showSourceDebuggingBox = new QCheckBox("Show script sources", this);
    if (_logger->showSourceDebugging()) {
        _showSourceDebuggingBox->setCheckState(Qt::Checked);
    }
    _showSourceDebuggingBox->show();
    connect(_showSourceDebuggingBox, &QCheckBox::stateChanged, this, &LogDialog::handleShowSourceDebuggingCheckbox);

    _allLogsButton = new QPushButton("All Messages", this);
    // set object name for css styling

    _allLogsButton->setObjectName("allLogsButton");
    _allLogsButton->show();
    connect(_allLogsButton, &QPushButton::clicked, this, &LogDialog::handleAllLogsButton);
    handleAllLogsButton();

    auto windowGeometry = _windowGeometry.get();
    if (windowGeometry.isValid()) {
        setGeometry(windowGeometry);
    }
}

void LogDialog::resizeEvent(QResizeEvent* event) {
    BaseLogDialog::resizeEvent(event);
    _revealLogButton->setGeometry(width() - ELEMENT_MARGIN - REVEAL_BUTTON_WIDTH,
        ELEMENT_MARGIN,
        REVEAL_BUTTON_WIDTH,
        ELEMENT_HEIGHT);
    _allLogsButton->setGeometry(width() - ELEMENT_MARGIN - ALL_LOGS_BUTTON_WIDTH,
        THIRD_ROW,
        ALL_LOGS_BUTTON_WIDTH,
        ELEMENT_HEIGHT);
    _extraDebuggingBox->setGeometry(width() - ELEMENT_MARGIN - COMBOBOX_WIDTH - ELEMENT_MARGIN - ALL_LOGS_BUTTON_WIDTH,
        THIRD_ROW,
        COMBOBOX_WIDTH,
        ELEMENT_HEIGHT);
    _keepOnTopBox->setGeometry(width() - ELEMENT_MARGIN - COMBOBOX_WIDTH - ELEMENT_MARGIN - ALL_LOGS_BUTTON_WIDTH - ELEMENT_MARGIN - COMBOBOX_WIDTH - ELEMENT_MARGIN,
        THIRD_ROW,
        COMBOBOX_WIDTH,
        ELEMENT_HEIGHT);
    _showSourceDebuggingBox->setGeometry(width() - ELEMENT_MARGIN - COMBOBOX_WIDTH - ELEMENT_MARGIN - ALL_LOGS_BUTTON_WIDTH
                                                 - ELEMENT_MARGIN - COMBOBOX_WIDTH - ELEMENT_MARGIN - ELEMENT_MARGIN
                                                 - COMBOBOX_WIDTH - ELEMENT_MARGIN,
        THIRD_ROW,
        COMBOBOX_WIDTH,
        ELEMENT_HEIGHT);
    _messageCount->setGeometry(_leftPad,
        THIRD_ROW,
        COMBOBOX_WIDTH,
        ELEMENT_HEIGHT);
}

void LogDialog::closeEvent(QCloseEvent* event) {
    BaseLogDialog::closeEvent(event);
    _windowGeometry.set(geometry());
}

void LogDialog::handleRevealButton() {
    _logger->locateLog();
}

void LogDialog::handleAllLogsButton() {
    _logger->setShowSourceDebugging(false);
    _showSourceDebuggingBox->setCheckState(Qt::Unchecked);
    _logger->setExtraDebugging(false);
    _extraDebuggingBox->setCheckState(Qt::Unchecked);
    _logger->setDebugPrint(true);
    _debugPrintBox->setCheckState(Qt::Checked);
    _logger->setInfoPrint(true);
    _infoPrintBox->setCheckState(Qt::Checked);
    _logger->setCriticalPrint(true);
    _criticalPrintBox->setCheckState(Qt::Checked);
    _logger->setWarningPrint(true);
    _warningPrintBox->setCheckState(Qt::Checked);
    _logger->setSuppressPrint(true);
    _suppressPrintBox->setCheckState(Qt::Checked);
    _logger->setFatalPrint(true);
    _fatalPrintBox->setCheckState(Qt::Checked);
    _logger->setUnknownPrint(true);
    _unknownPrintBox->setCheckState(Qt::Checked);
    clearSearch();
    _filterDropdown->setCurrentIndex(0);
    printLogFile();
}

void LogDialog::handleShowSourceDebuggingCheckbox(int state) {
    _logger->setShowSourceDebugging(state != 0);
}

void LogDialog::handleExtraDebuggingCheckbox(int state) {
    _logger->setExtraDebugging(state != 0);
}

void LogDialog::handleDebugPrintBox(int state) {
    _logger->setDebugPrint(state != 0);
    printLogFile();
}

void LogDialog::handleInfoPrintBox(int state) {
    _logger->setInfoPrint(state != 0);
    printLogFile();
}

void LogDialog::handleKeepWindowOnTop(int state) {
    bool keepOnTop = (state != 0);

    Qt::WindowFlags flags = windowFlags();

    if (keepOnTop) {
        flags |= Qt::Tool;
    } else {
        flags &= ~Qt::Tool;
    }

    setWindowFlags(flags);
    qApp->setLogWindowOnTopSetting(keepOnTop);

    show();
}

void LogDialog::handleCriticalPrintBox(int state) {
    _logger->setCriticalPrint(state != 0);
    printLogFile();
}

void LogDialog::handleWarningPrintBox(int state) {
    _logger->setWarningPrint(state != 0);
    printLogFile();
}

void LogDialog::handleSuppressPrintBox(int state) {
    _logger->setSuppressPrint(state != 0);
    printLogFile();
}

void LogDialog::handleFatalPrintBox(int state) {
    _logger->setFatalPrint(state != 0);
    printLogFile();
}

void LogDialog::handleUnknownPrintBox(int state) {
    _logger->setUnknownPrint(state != 0);
    printLogFile();
}

void LogDialog::handleFilterDropdownChanged(int selection) {
    if (selection != 0) {
        _filterSelection = "[" + _filterDropdown->currentText() + "]";
    } else {
        _filterSelection = "";
    }
    printLogFile();
}

QString LogDialog::getCurrentLog() {
    return _logger->getLogData();
}

void LogDialog::appendLogLine(QString logLine) {
    if (logLine.contains(_searchTerm, Qt::CaseInsensitive) &&
        logLine.contains(_filterSelection, Qt::CaseSensitive)) {
        int indexToBold = _logTextBox->document()->characterCount();
        _highlighter->setBold(indexToBold);

        if (logLine.contains(DEBUG_TEXT, Qt::CaseSensitive)) {
            if (_logger->debugPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        } else if (logLine.contains(INFO_TEXT, Qt::CaseSensitive)) {
            if (_logger->infoPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            } 
        } else if (logLine.contains(CRITICAL_TEXT, Qt::CaseSensitive)) {
            if (_logger->criticalPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        } else if (logLine.contains(WARNING_TEXT, Qt::CaseSensitive)) {
            if (_logger->warningPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        } else if (logLine.contains(SUPPRESS_TEXT, Qt::CaseSensitive)) {
            if (_logger->suppressPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        } else if (logLine.contains(FATAL_TEXT, Qt::CaseSensitive)) {
            if (_logger->fatalPrint()) {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        } else {
            if (_logger->unknownPrint() && logLine.trimmed() != "") {
                _logTextBox->appendPlainText(logLine.trimmed());
                _count++;
                updateMessageCount();
            }
        }
    }
}

void LogDialog::printLogFile() {
    _count = 0;
    _logTextBox->clear();
    QString log = getCurrentLog();
    QStringList logList = log.split('\n');
    for (const auto& message : logList) {
        appendLogLine(message);
    }
    updateMessageCount();
}

void LogDialog::updateMessageCount() {
    _countLabel = QString::number(_count);
    if (_count != 1) {
        _countLabel.append("  log messages");
    }
    else {
        _countLabel.append("  log message");
    }
    _messageCount->setText(_countLabel);
}
