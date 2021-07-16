//
//  LogHandler.cpp
//  libraries/shared/src
//
//  Created by Stephen Birarda on 2014-10-28.
//  Migrated from Logging.cpp created on 6/11/13
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LogHandler.h"

#include <mutex>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_UNIX
#include <stdio.h>
#include <unistd.h>
#endif

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include "ScriptContextHelper.h"

QMutex LogHandler::_mutex(QMutex::Recursive);

LogHandler& LogHandler::getInstance() {
    static LogHandler staticInstance;
    return staticInstance;
}

LogHandler::LogHandler() {
    QString logOptions = qgetenv("VIRCADIA_LOG_OPTIONS").toLower();

#ifdef Q_OS_UNIX
    // Enable color by default if we're on Unix, and output is a tty (so we're not being piped into something)
    //
    // On Windows the situation is more complex, and color is supported or not depending on version and
    // registry settings, so for now it's off by default and it's up to the user to do what's required.
    if (isatty(fileno(stdout))) {
        _useColor = true;
    }
#endif

    auto optionList = logOptions.split(",");

    for (auto option : optionList) {
        option = option.trimmed();

        if (option == "color") {
            _useColor = true;
        } else if (option == "nocolor") {
            _useColor = false;
        } else if (option == "process_id") {
            _shouldOutputProcessID = true;
        } else if (option == "thread_id") {
            _shouldOutputThreadID = true;
        } else if (option == "milliseconds") {
            _shouldDisplayMilliseconds = true;
        } else if (option == "keep_repeats") {
            _keepRepeats = true;
        } else if (option != "") {
            fprintf(stdout, "Unrecognized option in VIRCADIA_LOG_OPTIONS: '%s'\n", option.toUtf8().constData());
        }
    }
}

const char* stringForLogType(LogMsgType msgType) {
    switch (msgType) {
        case LogInfo:
            return "INFO";
        case LogDebug:
            return "DEBUG";
        case LogWarning:
            return "WARNING";
        case LogCritical:
            return "CRITICAL";
        case LogFatal:
            return "FATAL";
        case LogSuppressed:
            return "SUPPRESS";
        default:
            return "UNKNOWN";
    }
}

const char* colorForLogType(LogMsgType msgType) {
    switch (msgType) {
        case LogInfo:
            return "\u001b[37;1m";  // Bold white
        case LogDebug:
            return "";
        case LogWarning:
            return "\u001b[35;1m";  // Bright magenta
        case LogCritical:
            return "\u001b[31;1m";  // Bright red
        case LogFatal:
            return "\u001b[31;1m";  // Bright red
        case LogSuppressed:
            return "";
        default:
            return "";
    }
}

const char* colorReset() {
    return "\u001b[0m";
}

// the following will produce 11/18 13:55:36
const QString DATE_STRING_FORMAT = "MM/dd hh:mm:ss";

// the following will produce 11/18 13:55:36.999
const QString DATE_STRING_FORMAT_WITH_MILLISECONDS = "MM/dd hh:mm:ss.zzz";

void LogHandler::setTargetName(const QString& targetName) {
    QMutexLocker lock(&_mutex);
    _targetName = targetName;
}

void LogHandler::setShouldOutputProcessID(bool shouldOutputProcessID) {
    QMutexLocker lock(&_mutex);
    _shouldOutputProcessID = shouldOutputProcessID;
}

void LogHandler::setShouldOutputThreadID(bool shouldOutputThreadID) {
    QMutexLocker lock(&_mutex);
    _shouldOutputThreadID = shouldOutputThreadID;
}

void LogHandler::setShouldDisplayMilliseconds(bool shouldDisplayMilliseconds) {
    QMutexLocker lock(&_mutex);
    _shouldDisplayMilliseconds = shouldDisplayMilliseconds;
}


void LogHandler::flushRepeatedMessages() {
    QMutexLocker lock(&_mutex);

    // New repeat-suppress scheme:
    for (int m = 0; m < (int)_repeatedMessageRecords.size(); ++m) {
        int repeatCount = _repeatedMessageRecords[m].repeatCount;
        if (repeatCount > 1) {
            QString repeatLogMessage = QString().setNum(repeatCount) + " repeated log entries - Last entry: \"" 
                    + _repeatedMessageRecords[m].repeatString + "\"";
            printMessage(LogSuppressed, QMessageLogContext(), repeatLogMessage);
            _repeatedMessageRecords[m].repeatCount = 0;
            _repeatedMessageRecords[m].repeatString = QString();
        }
    }
}

QString LogHandler::printMessage(LogMsgType type, const QMessageLogContext& context, const QString& message) {
    if (message.isEmpty()) {
        return QString();
    }
    QMutexLocker lock(&_mutex);

    // log prefix is in the following format
    // [TIMESTAMP] [DEBUG] [PID] [TID] [TARGET] logged string

    const QString* dateFormatPtr = &DATE_STRING_FORMAT;
    if (_shouldDisplayMilliseconds) {
        dateFormatPtr = &DATE_STRING_FORMAT_WITH_MILLISECONDS;
    }

    QString prefixString = QString("[%1] [%2] [%3]").arg(QDateTime::currentDateTime().toString(*dateFormatPtr),
        stringForLogType(type), context.category);

    if (_shouldOutputProcessID) {
        prefixString.append(QString(" [%1]").arg(QCoreApplication::applicationPid()));
    }

    if (_shouldOutputThreadID) {
        size_t threadID = (size_t)QThread::currentThreadId();
        prefixString.append(QString(" [%1]").arg(threadID));
    }

    if (!_targetName.isEmpty()) {
        prefixString.append(QString(" [%1]").arg(_targetName));
    }

    // for [qml] console.* messages include an abbreviated source filename
    if (context.category && context.file && !strcmp("qml", context.category)) {
        if (const char* basename = strrchr(context.file, '/')) {
            prefixString.append(QString(" [%1]").arg(basename+1));
        }
    }

    QString logMessage = QString("%1 %2\n").arg(prefixString, message.split('\n').join('\n' + prefixString + " "));

    const char* color = "";
    const char* resetColor = "";

    if (_useColor) {
        color = colorForLogType(type);
        resetColor = colorReset();
    }

    if (_keepRepeats || _previousMessage != message) {
        if (_repeatCount > 0) {
            fprintf(stdout, "[Previous message was repeated %i times]\n", _repeatCount);
        }

        if (context.category && strcmp(context.category, "vircadia.script-engine.logging-agent") != 0 ) {
            QStringList context = ScriptContextHelper::get();
            if ( !context.isEmpty()) {
#ifdef Q_OS_WIN
                OutputDebugStringA(qPrintable(QString("Script context:\n")));
#endif
                fprintf(stdout, "Script context:\n");
                for( auto line : context) {
                    fprintf(stdout, "\t%s\n", line.toStdString().c_str());
#ifdef Q_OS_WIN
                    OutputDebugStringA(qPrintable(line));
#endif
                }
            }
        }
        fprintf(stdout, "%s%s%s", color, qPrintable(logMessage), resetColor);
        _repeatCount = 0;
    } else {
        _repeatCount++;
    }

    _previousMessage = message;
#ifdef Q_OS_WIN
    // On windows, this will output log lines into the Visual Studio "output" tab
    OutputDebugStringA(qPrintable(logMessage));
#endif
    return logMessage;
}

void LogHandler::verboseMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    getInstance().printMessage((LogMsgType) type, context, message);
}

void LogHandler::setupRepeatedMessageFlusher() {
    static std::once_flag once;
    std::call_once(once, [&] {
        // setup our timer to flush the verbose logs every 5 seconds
        QTimer* logFlushTimer = new QTimer(this);
        connect(logFlushTimer, &QTimer::timeout, this, &LogHandler::flushRepeatedMessages);
        logFlushTimer->start(VERBOSE_LOG_INTERVAL_SECONDS * 1000);
    });
}

int LogHandler::newRepeatedMessageID() {
    QMutexLocker lock(&_mutex);
    int newMessageId = _currentMessageID;
    ++_currentMessageID;
    RepeatedMessageRecord newRecord { 0, QString() };
    _repeatedMessageRecords.push_back(newRecord);
    return newMessageId;
}

void LogHandler::printRepeatedMessage(int messageID, LogMsgType type, const QMessageLogContext& context,
                                      const QString& message) {
    QMutexLocker lock(&_mutex);
    if (messageID >= _currentMessageID) {
        return;
    }

    if (_repeatedMessageRecords[messageID].repeatCount == 0) {
        printMessage(type, context, message);
    } else {
        _repeatedMessageRecords[messageID].repeatString = message;
    }
 
    ++_repeatedMessageRecords[messageID].repeatCount;
}
