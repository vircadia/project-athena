//
//  JSConsole.h
//  interface/src/ui
//
//  Created by Ryan Huffman on 05/12/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_JSConsole_h
#define hifi_JSConsole_h

#include <QFutureWatcher>
#include <QObject>
#include <QSharedPointer>
#include <QCompleter>
#include <QtCore/QJsonArray>

#include "ui_console.h"

class QStandardItemModel;
class ScriptManager;
class ScriptValue;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

const QString CONSOLE_TITLE = "Scripting Console";
const float CONSOLE_WINDOW_OPACITY = 0.95f;
const int CONSOLE_WIDTH = 800;
const int CONSOLE_HEIGHT = 200;

class JSConsole : public QWidget {
    Q_OBJECT
public:
    JSConsole(QWidget* parent, const ScriptManagerPointer& scriptManager = ScriptManagerPointer());
    ~JSConsole();

    void setScriptManager(const ScriptManagerPointer& scriptManager = ScriptManagerPointer());
    void clear();

public slots:
    void executeCommand(const QString& command);

protected:
    void setAndSelectCommand(const QString& command);
    virtual bool eventFilter(QObject* sender, QEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;

protected slots:
    void scrollToBottom();
    void resizeTextInput();
    void handlePrint(const QString& message, const QString& scriptName);
    void handleInfo(const QString& message, const QString& scriptName);
    void handleWarning(const QString& message, const QString& scriptName);
    void handleError(const QString& message, const QString& scriptName);
    void commandFinished();

private slots:
    void insertCompletion(const QModelIndex& completion);
    void highlightedCompletion(const QModelIndex& completion);

private:
    void appendMessage(const QString& gutter, const QString& message);
    void setToNextCommandInHistory();
    void setToPreviousCommandInHistory();
    void resetCurrentCommandHistory();

    void readAPI();

    QStandardItemModel* getAutoCompleteModel(const QString& memberOf = nullptr);

    QFutureWatcher<ScriptValuePointer> _executeWatcher;
    Ui::Console* _ui;
    int _currentCommandInHistory;
    QString _savedHistoryFilename;
    QList<QString> _commandHistory;
    QString _rootCommand;
    ScriptManagerPointer _scriptManager;
    static const QString _consoleFileName;
    QJsonArray _apiDocs;
    QCompleter* _completer;
    QString _completerModule {""};
};


#endif // hifi_JSConsole_h
