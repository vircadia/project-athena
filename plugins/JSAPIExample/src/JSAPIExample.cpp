//
//  JSAPIExample.cpp
//  plugins/JSAPIExample/src
//
//  Copyright (c) 2019-2020 humbletim (humbletim@gmail.com)
//  Copyright (c) 2019 Kalila L. (somnilibertas@gmail.com)
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

//  Example of prototyping new JS APIs by leveraging the existing plugin system.

#include <QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <SettingHelpers.h>  // for ::settingsFilename()
#include <SharedUtil.h>      // for ::usecTimestampNow()
#include <shared/ScriptInitializerMixin.h>
#include <ScriptContext.h>
#include <ScriptEngine.h>
#include <ScriptManager.h>
#include <ScriptValue.h>
#include <Scriptable.h>

// NOTE: replace this with your own namespace when starting a new plugin (to avoid .so/.dll symbol clashes)
namespace REPLACE_ME_WITH_UNIQUE_NAME {

    static constexpr auto JSAPI_SEMANTIC_VERSION = "0.0.1";
    static constexpr auto JSAPI_EXPORT_NAME = "JSAPIExample";

    QLoggingCategory logger { "jsapiexample" };

    inline QVariant raiseScriptingError(ScriptContext* context, const QString& message, const QVariant& returnValue = QVariant()) {
        if (context) {
            // when a ScriptContext is available throw an actual JS Exception (which can be caught using try/catch on JS side)
            context->throwError(message);
        } else {
            // otherwise just log the error
            qCWarning(logger) << "error:" << message;
        }
        return returnValue;
    }

    QObject* createScopedSettings(const QString& scope, QObject* parent, QString& error);

    class JSAPIExample : public QObject, public Scriptable {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "JSAPIExample" FILE "plugin.json")
        Q_PROPERTY(QString version MEMBER _version CONSTANT)
    public:
        JSAPIExample() {
            setObjectName(JSAPI_EXPORT_NAME);
            auto scriptInit = DependencyManager::get<ScriptInitializers>();
            if (!scriptInit) {
                qCWarning(logger) << "COULD NOT INITIALIZE (ScriptInitializers unavailable)" << qApp << this;
                return;
            }
            qCWarning(logger) << "registering w/ScriptInitializerMixin..." << scriptInit.data();
            scriptInit->registerScriptInitializer([this](ScriptEngine* engine) {
                auto value = engine->newQObject(this, ScriptEngine::QtOwnership, ScriptEngine::ExcludeDeleteLater);
                engine->globalObject()->setProperty(objectName(), value);
                // qCDebug(logger) << "setGlobalInstance" << objectName() << engine->property("fileName");
            });
            // qCInfo(logger) << "plugin loaded" << qApp << toString() << QThread::currentThread();
        }

        // NOTES: everything within the "public slots:" section below will be available from JS via overall plugin QObject
        //    also, to demonstrate future-proofing JS API code, QVariant's are used throughout most of these examples --
        //    which still makes them very Qt-specific, but avoids depending directly on deprecated ScriptValuePointer APIs.
        //    (as such this plugin class and its methods remain forward-compatible with other engines like QML's QJSEngine)

    public slots:
        // pretty-printed representation for logging eg: print(JSAPIExample)
        // (note: Qt script engines automatically look for a ".toString" method on native classes when coercing values to strings)
        QString toString() const { return QString("[%1 version=%2]").arg(objectName()).arg(_version); }

        /*@jsdoc
         * Returns current microseconds (usecs) since Epoch. note: 1000usecs == 1ms
         * @example <caption>Measure current setTimeout accuracy.</caption>
         * var expected = 1000;
         * var start = JSAPIExample.now();
         * Script.setTimeout(function () {
         *     var elapsed = (JSAPIExample.now() - start)/1000;
         *     print("expected (ms):", expected, "actual (ms):", elapsed);
         * }, expected);
         */
        QVariant now() const { return usecTimestampNow(); }

        /*@jsdoc
         * Example of returning a JS Object key-value map
         * @example <caption>"zip" a list of keys and corresponding values to form key-value map</caption>
         * print(JSON.stringify(JSAPIExample.zip(["a","b"], [1,2])); // { "a": 1, "b": 2 }
         */
        QVariant zip(const QStringList& keys, const QVariantList& values) const {
            QVariantMap out;
            for (int i = 0; i < keys.size(); i++) {
                out[keys[i]] = i < values.size() ? values[i] : QVariant();
            }
            return out;
        }

        /*@jsdoc
         * Example of returning a JS Array result
         * @example <caption>emulate Object.values(keyValues)</caption>
         * print(JSON.stringify(JSAPIExample.values({ "a": 1, "b": 2 }))); // [1,2]
         */
        QVariant values(const QVariantMap& keyValues) const {
            QVariantList values = keyValues.values();
            return values;
        }

        /*@jsdoc
         * Another example of returning JS Array data
         * @example <caption>generate an integer sequence (inclusive of [from, to])</caption>
         * print(JSON.stringify(JSAPIExample.seq(1,5)));// [1,2,3,4,5]
         */
        QVariant seq(int from, int to) const {
            QVariantList out;
            for (int i = from; i <= to; i++) {
                out.append(i);
            }
            return out;
        }

        /*@jsdoc
         * Example of returning arbitrary binary data from C++ (resulting in a JS ArrayBuffer)
         * see also: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer#Examples
         * @example <caption>return compressed/decompressed versions of the input data</caption>
         * var data = "testing 1 2 3";
         * var z = JSAPIExample.qCompressString(data); // z will be an ArrayBuffer
         * var u = JSAPIExample.qUncompressString(z); // u will be a String value
         * print(JSON.stringify({ input: data, compressed: z.byteLength, output: u, uncompressed: u.length }));
         */
        QVariant qCompressString(const QString& jsString, int compress_level = -1) const {
            QByteArray arrayBuffer = qCompress(jsString.toUtf8(), compress_level);
            return arrayBuffer;
        }
        QVariant qUncompressString(const QByteArray& arrayBuffer) const {
            QString jsString = QString::fromUtf8(qUncompress(arrayBuffer));
            return jsString;
        }

        /**
          * Example of exposing a custom "managed" C++ QObject to JS
          * The lifecycle of the created QObject* instance becomes managed by the invoking ScriptEngine --
          * it will be automatically cleaned up once no longer reachable from any JS variables/closures.
          * @example <caption>access persistent settings stored in separate .json files</caption>
          * var settings = JSAPIExample.getScopedSettings("example");
          * print("example settings stored in:", settings.fileName());
          * print("(before) example::timestamp", settings.getValue("timestamp"));
          * settings.setValue("timestamp", Date.now());
          * print("(after) example::timestamp", settings.getValue("timestamp"));
          * print("all example::* keys", settings.allKeys());
          * settings = null; // optional best pratice; allows the object to be reclaimed ASAP by the JS garbage collector
          */
        ScriptValuePointer getScopedSettings(const QString& scope) {
            auto engine = Scriptable::engine();
            if (!engine) {
                return ScriptValuePointer();
            }
            auto manager = engine->manager();
            if (!manager) {
                return ScriptValuePointer();
            }
            QString error;
            auto cppValue = createScopedSettings(scope, manager, error);
            if (!cppValue) {
                raiseScriptingError(context(), "error creating scoped settings instance: " + error);
                return engine->nullValue();
            }
            return engine->newQObject(cppValue, ScriptEngine::ScriptOwnership, ScriptEngine::ExcludeDeleteLater);
        }

    private:
        const QString _version { JSAPI_SEMANTIC_VERSION };
    };

    // JSSettingsHelper wraps a scoped (prefixed/separate) QSettings and exposes a subset of QSetting APIs as slots
    class JSSettingsHelper : public QObject {
        Q_OBJECT
    public:
        JSSettingsHelper(const QString& scope, QObject* parent = nullptr);
        ~JSSettingsHelper();
        operator bool() const;
    public slots:
        QString fileName() const;
        QString toString() const;
        QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant());
        bool setValue(const QString& key, const QVariant& value);
        QStringList allKeys() const;
    protected:
        QString _scope;
        QString _fileName;
        QSharedPointer<QSettings> _settings;
        QString getLocalSettingsPath(const QString& scope) const;
    };

    // verifies the requested scope is sensible and creates/returns a scoped JSSettingsHelper instance
    QObject* createScopedSettings(const QString& scope, QObject* parent, QString& error) {
        const QRegExp VALID_SETTINGS_SCOPE { "[-_A-Za-z0-9]{1,64}" };
        if (!VALID_SETTINGS_SCOPE.exactMatch(scope)) {
            error = QString("invalid scope (expected alphanumeric <= 64 chars not '%1')").arg(scope);
            return nullptr;
        }
        return new JSSettingsHelper(scope, parent);
    }

    // --------------------------------------------------
    // ----- inline JSSettingsHelper implementation -----
    JSSettingsHelper::operator bool() const {
        return (bool)_settings;
    }
    JSSettingsHelper::JSSettingsHelper(const QString& scope, QObject* parent) :
        QObject(parent), _scope(scope), _fileName(getLocalSettingsPath(scope)),
        _settings(_fileName.isEmpty() ? nullptr : new QSettings(_fileName, JSON_FORMAT)) {
    }
    JSSettingsHelper::~JSSettingsHelper() {
        qCDebug(logger) << "~JSSettingsHelper" << _scope << _fileName << this;
    }
    QString JSSettingsHelper::fileName() const {
        return _settings ? _settings->fileName() : "";
    }
    QString JSSettingsHelper::toString() const {
        return QString("[JSSettingsHelper scope=%1 valid=%2]").arg(_scope).arg((bool)_settings);
    }
    QVariant JSSettingsHelper::getValue(const QString& key, const QVariant& defaultValue) {
        return _settings ? _settings->value(key, defaultValue) : defaultValue;
    }
    bool JSSettingsHelper::setValue(const QString& key, const QVariant& value) {
        if (_settings) {
            if (value.isValid()) {
                _settings->setValue(key, value);
            } else {
                _settings->remove(key);
            }
            return true;
        }
        return false;
    }
    QStringList JSSettingsHelper::allKeys() const {
        return _settings ? _settings->allKeys() : QStringList{};
    }
    QString JSSettingsHelper::getLocalSettingsPath(const QString& scope) const {
        // generate a prefixed filename (relative to the main application's Interface.json file)
        const QString fileName = QString("jsapi_%1.json").arg(scope);
        return QFileInfo(::settingsFilename()).dir().filePath(fileName);
    }
    // ----- /inline JSSettingsHelper implementation -----

}  // namespace REPLACE_ME_WITH_UNIQUE_NAME

#include "JSAPIExample.moc"
