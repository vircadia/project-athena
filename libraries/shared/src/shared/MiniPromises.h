//
//  Created by Timothy Dedischew on 2017/12/21
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

// Minimalist threadsafe Promise-like helper for managing asynchronous results
//
// This class pivots around continuation-style callback handlers:
//     auto successCallback = [=](QVariantMap result) { .... }
//     auto errorCallback = [=](QString error) { .... }
//     auto combinedCallback = [=](QString error, QVariantMap result) { .... }
//
// * Callback Handlers are automatically invoked on the right thread (the Promise's thread).
// * Callbacks can be assigned anytime during a Promise's life and "do the right thing".
//   - ie: for code clarity you can define success cases first or choose to maintain time order
// * "Deferred" concept can be used to publish outcomes.
// * "Promise" concept be used to subscribe to outcomes.
//
// See AssetScriptingInterface.cpp for some examples of using to simplify chained async results.

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QVariantMap>
#include <QDebug>
#include "ReadWriteLockable.h"
#include <memory>

class MiniPromise : public QObject, public std::enable_shared_from_this<MiniPromise>, public ReadWriteLockable {
    Q_OBJECT
    Q_PROPERTY(QString state READ getStateString)
    Q_PROPERTY(QString error READ getError)
    Q_PROPERTY(QVariantMap result READ getResult)
public:
    using HandlerFunction = std::function<void(QString error, QVariantMap result)>;
    using SuccessFunction = std::function<void(QVariantMap result)>;
    using ErrorFunction = std::function<void(QString error)>;
    using HandlerFunctions = QVector<HandlerFunction>;
    using Promise = std::shared_ptr<MiniPromise>;

    static int metaTypeID;

    MiniPromise() {}
    MiniPromise(const QString debugName) { setObjectName(debugName); }

    ~MiniPromise() {
        if (getStateString() == "pending") {
            qWarning() << "MiniPromise::~MiniPromise -- destroying pending promise:" << objectName() << _error << _result << "handlers:" << getPendingHandlerCount();
        }
    }
    Promise self() { return shared_from_this(); }

    Q_INVOKABLE void executeOnPromiseThread(std::function<void()> function, MiniPromise::Promise root = nullptr) {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(
                this, "executeOnPromiseThread", Qt::QueuedConnection,
                Q_ARG(std::function<void()>, function),
                Q_ARG(MiniPromise::Promise, self()));
        } else {
            function();
        }
    }

    // result aggregation helpers -- eg: deferred->defaults(interimResultMap)->ready(...)
    // copy values from the input map, but only for keys that don't already exist
    Promise mixin(const QVariantMap& source) {
        withWriteLock([&]{
            for (const auto& key : source.keys()) {
                if (!_result.contains(key)) {
                    _result[key] = source[key];
                }
            }
        });
        return self();
    }
    // copy values from the input map, replacing any existing keys
    Promise assignResult(const QVariantMap& source) {
        withWriteLock([&]{
            for (const auto& key : source.keys()) {
                _result[key] = source[key];
            }
        });
        return self();
    }

    // callback registration methods
    Promise ready(HandlerFunction always) { return finally(always); }
    Promise finally(HandlerFunction always) {
        if (!_rejected && !_resolved) {
            withWriteLock([&]{
                _onfinally << always;
            });
        } else {
            executeOnPromiseThread([&]{
                always(getError(), getResult());
            });
        }
        return self();
    }
    Promise fail(ErrorFunction errorOnly) {
        return fail([errorOnly](QString error, QVariantMap result) {
            errorOnly(error);
        });
    }

    Promise fail(HandlerFunction failFunc) {
        if (!_rejected) {
            withWriteLock([&]{
                _onreject << failFunc;
            });
        } else {
            executeOnPromiseThread([&]{
                failFunc(getError(), getResult());
            });
        }
        return self();
    }

    Promise then(SuccessFunction successOnly) {
        return then([successOnly](QString error, QVariantMap result) {
            successOnly(result);
        });
    }
    Promise then(HandlerFunction successFunc) {
        if (!_resolved) {
            withWriteLock([&]{
                _onresolve << successFunc;
            });
        } else {
            executeOnPromiseThread([&]{
                successFunc(getError(), getResult());
            });
        }
        return self();
    }

    // NOTE: first arg may be null (see ES6 .then(null, errorHandler) conventions)
    Promise then(SuccessFunction successOnly, ErrorFunction errorOnly) {
        if (successOnly) {
            then(successOnly);
        }
        if (errorOnly) {
            fail(errorOnly);
        }
        return self();
    }


    // helper functions for forwarding results on to a next Promise
    Promise ready(Promise next) { return finally(next); }
    Promise finally(Promise next) {
        return finally([next](QString error, QVariantMap result) {
            next->handle(error, result);
        });
    }
    Promise fail(Promise next) {
        return fail([next](QString error, QVariantMap result) {
            next->reject(error, result);
        });
    }
    Promise then(Promise next) {
        return then([next](QString error, QVariantMap result) {
            next->resolve(error, result);
        });
    }


    // trigger methods
    // handle() automatically resolves or rejects the promise (based on whether an error value occurred)
    Promise handle(QString error, const QVariantMap& result) {
        if (error.isEmpty()) {
            resolve(error, result);
        } else {
            reject(error, result);
        }
        return self();
    }

    Promise resolve(QVariantMap result) {
        return resolve(QString(), result);
    }
    Promise resolve(QString error, const QVariantMap& result) {
        setState(true, error, result);

        executeOnPromiseThread([&]{
            const QString localError{ getError() };
            const QVariantMap localResult{ getResult() };
            HandlerFunctions resolveHandlers;
            HandlerFunctions finallyHandlers;
            withReadLock([&]{
                resolveHandlers = _onresolve;
                finallyHandlers = _onfinally;
            });
            for (const auto& onresolve : resolveHandlers) {
                onresolve(localError, localResult);
            }
            for (const auto& onfinally : finallyHandlers) {
                onfinally(localError, localResult);
            }
        });
        return self();
    }

    Promise reject(QString error) {
        return reject(error, QVariantMap());
    }
    Promise reject(QString error, const QVariantMap& result) {
        setState(false, error, result);

        executeOnPromiseThread([&]{
            const QString localError{ getError() };
            const QVariantMap localResult{ getResult() };
            HandlerFunctions rejectHandlers;
            HandlerFunctions finallyHandlers;
            withReadLock([&]{
                rejectHandlers = _onreject;
                finallyHandlers = _onfinally;
            });
            for (const auto& onreject : rejectHandlers) {
                onreject(localError, localResult);
            }
            for (const auto& onfinally : finallyHandlers) {
                onfinally(localError, localResult);
            }
        });
        return self();
    }

private:

    Promise setState(bool resolved, QString error, const QVariantMap& result) {
        if (resolved) {
            _resolved = true;
        } else {
            _rejected = true;
        }
        setError(error);
        assignResult(result);
        return self();
    }

    void setError(const QString error) { withWriteLock([&]{ _error = error; }); }
    QString getError() const { return resultWithReadLock<QString>([this]() -> QString { return _error; }); }
    QVariantMap getResult() const { return resultWithReadLock<QVariantMap>([this]() -> QVariantMap { return _result; }); }
    int getPendingHandlerCount() const {
        return resultWithReadLock<int>([this]() -> int {
            return _onresolve.size() + _onreject.size() + _onfinally.size();
        });
    }
    QString getStateString() const {
        return _rejected ? "rejected" :
            _resolved ? "resolved" :
            getPendingHandlerCount() ? "pending" :
            "unknown";
    }
    QString _error;
    QVariantMap _result;
    std::atomic<bool> _rejected{false};
    std::atomic<bool> _resolved{false};
    HandlerFunctions _onresolve;
    HandlerFunctions _onreject;
    HandlerFunctions _onfinally;
};

Q_DECLARE_METATYPE(MiniPromise::Promise)

inline MiniPromise::Promise makePromise(const QString& hint = QString()) {
    if (!QMetaType::isRegistered(qMetaTypeId<MiniPromise::Promise>())) {
        int type = qRegisterMetaType<MiniPromise::Promise>();
        qDebug() << "makePromise -- registered MetaType<MiniPromise::Promise>:" << type;
    }
    return std::make_shared<MiniPromise>(hint);
}
