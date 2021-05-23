//
//  JSConsole.cpp
//  interface/src/ui
//
//  Created by Ryan Huffman on 05/12/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "JSConsole.h"

#include <QFuture>
#include <QLabel>
#include <QScrollBar>
#include <QtConcurrent/QtConcurrentRun>
#include <QStringListModel>
#include <QListView>
#include <QToolTip>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <shared/QtHelpers.h>
#include <ScriptEngines.h>
#include <PathUtils.h>

#include "Application.h"
#include "ScriptHighlighting.h"

const int NO_CURRENT_HISTORY_COMMAND = -1;
const int MAX_HISTORY_SIZE = 256;
const QString HISTORY_FILENAME = "JSConsole.history.json";

const QString COMMAND_STYLE = "color: #266a9b;";

const QString RESULT_SUCCESS_STYLE = "color: #677373;";
const QString RESULT_INFO_STYLE = "color: #223bd1;";
const QString RESULT_WARNING_STYLE = "color: #999922;";
const QString RESULT_ERROR_STYLE = "color: #d13b22;";

const QString GUTTER_PREVIOUS_COMMAND = "<span style=\"color: #57b8bb;\">&lt;</span>";
const QString GUTTER_ERROR = "<span style=\"color: #d13b22;\">X</span>";

const QString JSDOC_LINE_SEPARATOR = "\r";

const QString JSDOC_STYLE =
    "<style type=\"text/css\"> \
        .code { \
            font-family: Consolas, Monaco, 'Andale Mono', monospace \
        } \
        pre, code { \
            display: inline; \
        } \
        .no-wrap { \
            white-space: nowrap; \
        } \
    </style>";

const QString JSConsole::_consoleFileName { "about:console" };

const QString JSON_KEY = "entries";
QList<QString> _readLines(const QString& filename) {
    QFile file(filename);
    file.open(QFile::ReadOnly);
    auto json = QTextStream(&file).readAll().toUtf8();
    auto root = QJsonDocument::fromJson(json).object();
    // TODO: check root["version"]
    return root[JSON_KEY].toVariant().toStringList();
}
 
void _writeLines(const QString& filename, const QList<QString>& lines) {
    QFile file(filename);
    file.open(QFile::WriteOnly);
    auto root = QJsonObject();
    root["version"] = 1.0;
    root["last-modified"] = QDateTime::currentDateTime().toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate);
    root[JSON_KEY] = QJsonArray::fromStringList(lines);
    auto json = QJsonDocument(root).toJson();
    QTextStream(&file) << json;
}

QString _jsdocTypeToString(QJsonValue jsdocType) {
    return jsdocType.toObject().value("names").toVariant().toStringList().join("/");
}

void JSConsole::readAPI() {
    QFile file(PathUtils::resourcesPath() + "auto-complete/hifiJSDoc.json");
    file.open(QFile::ReadOnly);
    auto json = QTextStream(&file).readAll().toUtf8();
    _apiDocs = QJsonDocument::fromJson(json).array();
}

QStandardItem* getAutoCompleteItem(QJsonValue propertyObject) {
    auto propertyItem = new QStandardItem(propertyObject.toObject().value("name").toString());
    propertyItem->setData(propertyObject.toVariant());
    return propertyItem;
}

QStandardItemModel* JSConsole::getAutoCompleteModel(const QString& memberOf) {
    QString memberOfProperty = nullptr;

    auto model = new QStandardItemModel(this);

    if (memberOf != nullptr) {
        foreach(auto doc, _apiDocs) {
            auto object = doc.toObject();
            if (object.value("name").toString() == memberOf && object.value("scope").toString() == "global" &&
                object.value("kind").toString() == "namespace") {

                memberOfProperty = object.value("longname").toString();

                auto properties = doc.toObject().value("properties").toArray();
                foreach(auto propertyObject, properties) {
                    model->appendRow(getAutoCompleteItem(propertyObject));
                }
            }
        }
        if (memberOfProperty == nullptr) {
            return nullptr;
        }
    }
    
    foreach(auto doc, _apiDocs) {
        auto object = doc.toObject();
        auto scope = object.value("scope");
        if ((memberOfProperty == nullptr && scope.toString() == "global" && object.value("kind").toString() == "namespace") ||
            (memberOfProperty != nullptr && object.value("memberof").toString() == memberOfProperty &&
                object.value("kind").toString() != "typedef")) {

            model->appendRow(getAutoCompleteItem(doc));
        }
    }
    model->sort(0);
    return model;
}

JSConsole::JSConsole(QWidget* parent, const ScriptManagerPointer& scriptManager) :
    QWidget(parent),
    _ui(new Ui::Console),
    _currentCommandInHistory(NO_CURRENT_HISTORY_COMMAND),
    _savedHistoryFilename(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" + HISTORY_FILENAME),
    _commandHistory(_readLines(_savedHistoryFilename)),
    _completer(new QCompleter(this)) {

    readAPI();

    _ui->setupUi(this);
    _ui->promptTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    _ui->promptTextEdit->setWordWrapMode(QTextOption::NoWrap);
    _ui->promptTextEdit->installEventFilter(this);

    QFile styleSheet(PathUtils::resourcesPath() + "styles/console.qss");
    if (styleSheet.open(QIODevice::ReadOnly)) {
        QDir::setCurrent(PathUtils::resourcesPath());
        setStyleSheet(styleSheet.readAll());
    }

    connect(_ui->scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this, &JSConsole::scrollToBottom);
    connect(_ui->promptTextEdit, &QTextEdit::textChanged, this, &JSConsole::resizeTextInput);

    _completer->setWidget(_ui->promptTextEdit);
    _completer->setModel(getAutoCompleteModel(nullptr));
    _completer->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    _completer->setMaxVisibleItems(12);
    _completer->setFilterMode(Qt::MatchStartsWith);
    _completer->setWrapAround(false);
    _completer->setCompletionMode(QCompleter::PopupCompletion);
    _completer->setCaseSensitivity(Qt::CaseSensitive);

    QListView *listView = new QListView();
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    listView->setSelectionMode(QAbstractItemView::SingleSelection);
    listView->setModelColumn(_completer->completionColumn());

    _completer->setPopup(listView);
    _completer->popup()->installEventFilter(this);
    QObject::connect(_completer, static_cast<void(QCompleter::*)(const QModelIndex&)>(&QCompleter::activated), this,
        &JSConsole::insertCompletion);

    QObject::connect(_completer, static_cast<void(QCompleter::*)(const QModelIndex&)>(&QCompleter::highlighted), this,
        &JSConsole::highlightedCompletion);

    setScriptManager(scriptManager);

    resizeTextInput();

    connect(&_executeWatcher, &QFutureWatcher<ScriptValuePointer>::finished, this, &JSConsole::commandFinished);
}

void JSConsole::insertCompletion(const QModelIndex& completion) {
    auto jsdocObject = QJsonValue::fromVariant(completion.data(Qt::UserRole + 1)).toObject();
    auto kind = jsdocObject.value("kind").toString();
    auto completionString = completion.data().toString();
    if (kind == "function") {
        auto params = jsdocObject.value("params").toArray();
        // automatically add the parenthesis/parentheses for the functions
        completionString += params.isEmpty() ? "()" : "(";
    }
    QTextCursor textCursor = _ui->promptTextEdit->textCursor();
    int extra = completionString.length() - _completer->completionPrefix().length();
    textCursor.movePosition(QTextCursor::Left);
    textCursor.movePosition(QTextCursor::EndOfWord);
    textCursor.insertText(completionString.right(extra));
    _ui->promptTextEdit->setTextCursor(textCursor);
}

void JSConsole::highlightedCompletion(const QModelIndex& completion) {
    auto jsdocObject = QJsonValue::fromVariant(completion.data(Qt::UserRole + 1)).toObject();
    QString memberOf = "";
    if (!_completerModule.isEmpty()) {
        memberOf = _completerModule + ".";
    }
    auto name = memberOf + "<b>" + jsdocObject.value("name").toString() + "</b>";
    auto description = jsdocObject.value("description").toString();
    auto examples = jsdocObject.value("examples").toArray();
    auto kind = jsdocObject.value("kind").toString();
    QString returnTypeText = "";

    QString paramsTable = "";
    if (kind == "function") {
        auto params = jsdocObject.value("params").toArray();
        auto returns = jsdocObject.value("returns");
        if (!returns.isUndefined()) {
            returnTypeText = _jsdocTypeToString(jsdocObject.value("returns").toArray().at(0).toObject().value("type")) + " ";
        }
        name += "(";
        if (!params.isEmpty()) {
            bool hasDefaultParam = false;
            bool hasOptionalParam = false;
            bool firstItem = true;
            foreach(auto param, params) {
                auto paramObject = param.toObject();
                if (!hasOptionalParam && paramObject.value("optional").toBool(false)) {
                    hasOptionalParam = true;
                    name += "<i>[";
                }
                if (!firstItem) {
                    name += ", ";
                } else {
                    firstItem = false;
                }
                name += paramObject.value("name").toString();
                if (!hasDefaultParam && !paramObject.value("defaultvalue").isUndefined()) {
                    hasDefaultParam = true;
                }
            }
            if (hasOptionalParam) {
                name += "]</i>";
            }

            paramsTable += "<table border=\"1\" cellpadding=\"10\"><thead><tr><th>Name</th><th>Type</th>";
            if (hasDefaultParam) {
                paramsTable += "<th>Default</th>";
            }
            paramsTable += "<th>Description</th></tr></thead><tbody>";
            foreach(auto param, params) {
                auto paramObject = param.toObject();
                paramsTable += "<tr><td>" + paramObject.value("name").toString() + "</td><td>" +
                    _jsdocTypeToString(paramObject.value("type")) + "</td><td>";
                if (hasDefaultParam) {
                    paramsTable += paramObject.value("defaultvalue").toVariant().toString() + "</td><td>";
                }
                paramsTable += paramObject.value("description").toString() + "</td></tr>";
            }

            paramsTable += "</tbody></table>";
        }
        name += ")";
    } else if (!jsdocObject.value("type").isUndefined()){
        returnTypeText = _jsdocTypeToString(jsdocObject.value("type")) + " ";
    }
    auto popupText = JSDOC_STYLE + "<span class=\"no-wrap\">" + returnTypeText + name + "</span>";
	auto descriptionText = "<p>" + description.replace(JSDOC_LINE_SEPARATOR, "<br>") + "</p>";

    popupText += descriptionText;
    popupText += paramsTable;
    auto returns = jsdocObject.value("returns");
    if (!returns.isUndefined()) {
        foreach(auto returnEntry, returns.toArray()) {
            auto returnsObject = returnEntry.toObject();
            auto returnsDescription = returnsObject.value("description").toString().replace(JSDOC_LINE_SEPARATOR, "<br>");
            popupText += "<h4>Returns</h4><p>" + returnsDescription + "</p><h5>Type</h5><pre><code>" +
                _jsdocTypeToString(returnsObject.value("type")) + "</code></pre>";
        }
    }

    if (!examples.isEmpty()) {
        popupText += "<h4>Examples</h4>";
        foreach(auto example, examples) {
            auto exampleText = example.toString();
            auto exampleLines = exampleText.split(JSDOC_LINE_SEPARATOR);
            foreach(auto exampleLine, exampleLines) {
                if (exampleLine.contains("<caption>")) {
                    popupText += exampleLine.replace("caption>", "h5>");
                } else {
                    popupText += "<pre><code>" + exampleLine + "\n</code></pre>";
                }
            }
        }
    }

    QToolTip::showText(QPoint(_completer->popup()->pos().x() + _completer->popup()->width(), _completer->popup()->pos().y()),
        popupText, _completer->popup());
}

JSConsole::~JSConsole() {
    if (_scriptManager) {
        disconnect(_scriptManager.data(), nullptr, this, nullptr);
        _scriptManager.reset();
    }
    delete _ui;
}

void JSConsole::setScriptManager(const ScriptManagerPointer& scriptManager) {
    if (_scriptManager == scriptManager && scriptManager != nullptr) {
        return;
    }
    if (scriptManager != nullptr) {
        disconnect(_scriptManager.data(), nullptr, this, nullptr);
        _scriptManager.reset();
    }

    // if scriptEngine is nullptr then create one and keep track of it using _ownScriptEngine
    if (scriptManager.isNull()) {
        _scriptManager = DependencyManager::get<ScriptEngines>()->loadScript(_consoleFileName, false);
    } else {
        _scriptManager = scriptManager;
    }

    connect(_scriptManager.data(), &ScriptManager::printedMessage, this, &JSConsole::handlePrint);
    connect(_scriptManager.data(), &ScriptManager::infoMessage, this, &JSConsole::handleInfo);
    connect(_scriptManager.data(), &ScriptManager::warningMessage, this, &JSConsole::handleWarning);
    connect(_scriptManager.data(), &ScriptManager::errorMessage, this, &JSConsole::handleError);
}

void JSConsole::executeCommand(const QString& command) {
    if (_commandHistory.isEmpty() || _commandHistory.constFirst() != command) {
        _commandHistory.prepend(command);
        if (_commandHistory.length() > MAX_HISTORY_SIZE) {
            _commandHistory.removeLast();
        }
        _writeLines(_savedHistoryFilename, _commandHistory);
    }

    _ui->promptTextEdit->setDisabled(true);

    appendMessage(">", "<span style='" + COMMAND_STYLE + "'>" + command.toHtmlEscaped() + "</span>");

    QWeakPointer<ScriptManager> weakScriptManager = _scriptManager;
    auto consoleFileName = _consoleFileName;
    QFuture<ScriptValuePointer> future = QtConcurrent::run([weakScriptManager, consoleFileName, command]() -> ScriptValuePointer {
        ScriptValuePointer result;
        auto scriptManager = weakScriptManager.lock();
        if (scriptManager) {
            BLOCKING_INVOKE_METHOD(scriptManager.data(), "evaluate",
                Q_RETURN_ARG(ScriptValuePointer, result),
                Q_ARG(const QString&, command),
                Q_ARG(const QString&, consoleFileName));
        }
        return result;
    });
    _executeWatcher.setFuture(future);
}

void JSConsole::commandFinished() {
    ScriptValuePointer result = _executeWatcher.result();

    _ui->promptTextEdit->setDisabled(false);

    // Make sure focus is still on this window - some commands are blocking and can take awhile to execute.
    if (window()->isActiveWindow()) {
        _ui->promptTextEdit->setFocus();
    }

    bool error = (_scriptManager->engine()->hasUncaughtException() || result->isError());
    QString gutter = error ? GUTTER_ERROR : GUTTER_PREVIOUS_COMMAND;
    QString resultColor = error ? RESULT_ERROR_STYLE : RESULT_SUCCESS_STYLE;
    QString resultStr = "<span style='" + resultColor + "'>" + result->toString().toHtmlEscaped() + "</span>";
    appendMessage(gutter, resultStr);

    resetCurrentCommandHistory();
}

void JSConsole::handleError(const QString& message, const QString& scriptName) {
    Q_UNUSED(scriptName);
    appendMessage(GUTTER_ERROR, "<span style='" + RESULT_ERROR_STYLE + "'>" + message.toHtmlEscaped() + "</span>");
}

void JSConsole::handlePrint(const QString& message, const QString& scriptName) {
    Q_UNUSED(scriptName);
    appendMessage("", message);
}

void JSConsole::handleInfo(const QString& message, const QString& scriptName) {
    Q_UNUSED(scriptName);
    appendMessage("", "<span style='" + RESULT_INFO_STYLE + "'>" + message.toHtmlEscaped() + "</span>");
}

void JSConsole::handleWarning(const QString& message, const QString& scriptName) {
    Q_UNUSED(scriptName);
    appendMessage("", "<span style='" + RESULT_WARNING_STYLE + "'>" + message.toHtmlEscaped() + "</span>");
}

void JSConsole::mouseReleaseEvent(QMouseEvent* event) {
    _ui->promptTextEdit->setFocus();
}

void JSConsole::showEvent(QShowEvent* event) {
    _ui->promptTextEdit->setFocus();
}

bool JSConsole::eventFilter(QObject* sender, QEvent* event) {
    if ((sender == _ui->promptTextEdit || sender == _completer->popup()) && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();

        if (_completer->popup()->isVisible()) {
            // The following keys are forwarded by the completer to the widget
            switch (key) {
            case Qt::Key_Space:
            case Qt::Key_Enter:
            case Qt::Key_Return:
                insertCompletion(_completer->popup()->currentIndex());
                _completer->popup()->hide();
                return true;
            default:
                return false;
            }
        }

        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                // If the shift key is being used then treat it as a regular return/enter. If this isn't done,
                // a new QTextBlock isn't created.
                keyEvent->setModifiers(keyEvent->modifiers() & ~Qt::ShiftModifier);
            } else {
                QString command = _ui->promptTextEdit->toPlainText().replace("\r\n", "\n").trimmed();

                if (!command.isEmpty()) {
                    QTextCursor cursor = _ui->promptTextEdit->textCursor();
                    _ui->promptTextEdit->clear();

                    executeCommand(command);
                }

                return true;
            }
        } else if (key == Qt::Key_Down) {
            // Go to the next command in history if the cursor is at the last line of the current command.
            int blockNumber = _ui->promptTextEdit->textCursor().blockNumber();
            int blockCount = _ui->promptTextEdit->document()->blockCount();
            if (blockNumber == blockCount - 1) {
                setToNextCommandInHistory();
                return true;
            }
        } else if (key == Qt::Key_Up) {
            // Go to the previous command in history if the cursor is at the first line of the current command.
            int blockNumber = _ui->promptTextEdit->textCursor().blockNumber();
            if (blockNumber == 0) {
                setToPreviousCommandInHistory();
                return true;
            }
        }
    } else if ((sender == _ui->promptTextEdit || sender == _completer->popup()) && event->type() == QEvent::KeyRelease) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();

        // completer shortcut (CTRL + SPACE)
        bool isCompleterShortcut = ((keyEvent->modifiers() & Qt::ControlModifier) && key == Qt::Key_Space) ||
            key == Qt::Key_Period;
        if (_completer->popup()->isVisible() || isCompleterShortcut) {

            const bool ctrlOrShift = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
            if (ctrlOrShift && keyEvent->text().isEmpty()) {
                return false;
            }

            static QString eow("~!@#$%^&*()+{}|:\"<>?,/;'[]\\-="); // end of word

            if (!isCompleterShortcut && (!keyEvent->text().isEmpty() && eow.contains(keyEvent->text().right(1)))) {
                _completer->popup()->hide();
                return false;
            }

            auto textCursor = _ui->promptTextEdit->textCursor();
                
            textCursor.select(QTextCursor::WordUnderCursor);
                
            QString completionPrefix = textCursor.selectedText();

            auto leftOfCursor = _ui->promptTextEdit->toPlainText().left(textCursor.position());

            // RegEx  [3]               [4]
            // (Module.subModule).(property/subModule)

            const int MODULE_INDEX = 3;
            const int PROPERTY_INDEX = 4;
            // TODO: disallow invalid characters on left of property
            QRegExp regExp("((([A-Za-z0-9_\\.]+)\\.)|(?!\\.))([a-zA-Z0-9_]*)$");
            regExp.indexIn(leftOfCursor);
            auto rexExpCapturedTexts = regExp.capturedTexts();
            auto memberOf = rexExpCapturedTexts[MODULE_INDEX];
            completionPrefix = rexExpCapturedTexts[PROPERTY_INDEX];
            bool switchedModule = false;
            if (memberOf != _completerModule) {
                _completerModule = memberOf;
                auto autoCompleteModel = getAutoCompleteModel(memberOf);
                if (autoCompleteModel == nullptr) {
                    _completer->popup()->hide();
                    return false;
                }
                _completer->setModel(autoCompleteModel);
                _completer->popup()->installEventFilter(this);
                switchedModule = true;
            }

            if (switchedModule || completionPrefix != _completer->completionPrefix()) {
                _completer->setCompletionPrefix(completionPrefix);
                _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0, 0));
            }
            auto cursorRect = _ui->promptTextEdit->cursorRect();
            cursorRect.setWidth(_completer->popup()->sizeHintForColumn(0) +
                _completer->popup()->verticalScrollBar()->sizeHint().width());
            _completer->complete(cursorRect);
            highlightedCompletion(_completer->popup()->currentIndex());
            return false;
        }
    }
    return false;
}

void JSConsole::setToNextCommandInHistory() {
    if (_currentCommandInHistory >= 0) {
        _currentCommandInHistory--;
        if (_currentCommandInHistory == NO_CURRENT_HISTORY_COMMAND) {
            setAndSelectCommand(_rootCommand);
        } else {
            setAndSelectCommand(_commandHistory[_currentCommandInHistory]);
        }
    }
}

void JSConsole::setToPreviousCommandInHistory() {
    if (_currentCommandInHistory < (_commandHistory.length() - 1)) {
        if (_currentCommandInHistory == NO_CURRENT_HISTORY_COMMAND) {
            _rootCommand = _ui->promptTextEdit->toPlainText();
        }
        _currentCommandInHistory++;
        setAndSelectCommand(_commandHistory[_currentCommandInHistory]);
    }
}

void JSConsole::resetCurrentCommandHistory() {
    _currentCommandInHistory = NO_CURRENT_HISTORY_COMMAND;
}

void JSConsole::resizeTextInput() {
    _ui->promptTextEdit->setFixedHeight(_ui->promptTextEdit->document()->size().height());
    _ui->promptTextEdit->updateGeometry();
}

void JSConsole::setAndSelectCommand(const QString& text) {
    QTextCursor cursor = _ui->promptTextEdit->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.deleteChar();
    cursor.insertText(text);
    cursor.movePosition(QTextCursor::End);
}

void JSConsole::scrollToBottom() {
    QScrollBar* scrollBar = _ui->scrollArea->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void JSConsole::appendMessage(const QString& gutter, const QString& message) {
    QWidget* logLine = new QWidget(_ui->logArea);
    QHBoxLayout* layout = new QHBoxLayout(logLine);
    layout->setMargin(0);
    layout->setSpacing(4);

    QLabel* gutterLabel = new QLabel(logLine);
    QLabel* messageLabel = new QLabel(logLine);

    gutterLabel->setFixedWidth(16);
    gutterLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    gutterLabel->setStyleSheet("font-size: 14px; font-family: Inconsolata, Lucida Console, Andale Mono, Monaco;");
    messageLabel->setStyleSheet("font-size: 14px; font-family: Inconsolata, Lucida Console, Andale Mono, Monaco;");

    gutterLabel->setText(gutter);
    messageLabel->setText(message);

    layout->addWidget(gutterLabel);
    layout->addWidget(messageLabel);
    logLine->setLayout(layout);

    layout->setAlignment(gutterLabel, Qt::AlignTop);

    layout->setStretch(0, 0);
    layout->setStretch(1, 1);

    _ui->logArea->layout()->addWidget(logLine);

    _ui->logArea->updateGeometry();
    scrollToBottom();
}

void JSConsole::clear() {
    QLayoutItem* item;
    while ((item = _ui->logArea->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    _ui->logArea->updateGeometry();
    scrollToBottom();
}
