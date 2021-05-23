//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GraphicsScriptingUtil.h"

#include <graphics/BufferViewHelpers.h>
#include <AABox.h>
#include <Extents.h>
#include "ScriptContext.h"
#include "ScriptEngine.h"
#include "ScriptValue.h"

using buffer_helpers::glmVecToVariant;

Q_LOGGING_CATEGORY(graphics_scripting, "hifi.scripting.graphics")

namespace scriptable {

QVariant toVariant(const glm::mat4& mat4) {
    QVector<float> floats;
    floats.resize(16);
    memcpy(floats.data(), &mat4, sizeof(glm::mat4));
    QVariant v;
    v.setValue<QVector<float>>(floats);
    return v;
};

QVariant toVariant(const Extents& box) {
    return QVariantMap{
        { "center", glmVecToVariant(box.minimum + (box.size() / 2.0f)) },
        { "minimum", glmVecToVariant(box.minimum) },
        { "maximum", glmVecToVariant(box.maximum) },
        { "dimensions", glmVecToVariant(box.size()) },
    };
}

/*@jsdoc
 * The extents of a mesh.
 * @typedef {object} Graphics.MeshExtents
 * @property {Vec3} brn - The bottom right near (minimum axes values) corner of the enclosing box.
 * @property {Vec3} tfl - The top far left (maximum axes values) corner of the enclosing box.
 * @property {Vec3} center - The center of the enclosing box.
 * @property {Vec3} dimensions - The dimensions of the enclosing box.
 */
QVariant toVariant(const AABox& box) {
    return QVariantMap{
        { "brn", glmVecToVariant(box.getCorner()) },
        { "tfl", glmVecToVariant(box.calcTopFarLeft()) },
        { "center", glmVecToVariant(box.calcCenter()) },
        { "minimum", glmVecToVariant(box.getMinimumPoint()) },
        { "maximum", glmVecToVariant(box.getMaximumPoint()) },
        { "dimensions", glmVecToVariant(box.getDimensions()) },
    };
}

/*@jsdoc
 * Details of a buffer element's format.
 * @typedef {object} Graphics.BufferElementFormat
 * @property {string} type - Type.
 * @property {string} semantic - Semantic.
 * @property {string} dimension - Dimension.
 * @property {number} scalarCount - Scalar count.
 * @property {number} byteSize - Byte size.
 * @property {number} BYTES_PER_ELEMENT - Bytes per element.
 */
QVariant toVariant(const gpu::Element& element) {
    return QVariantMap{
        { "type", gpu::toString(element.getType()) },
        { "semantic", gpu::toString(element.getSemantic()) },
        { "dimension", gpu::toString(element.getDimension()) },
        { "scalarCount", element.getScalarCount() },
        { "byteSize", element.getSize() },
        { "BYTES_PER_ELEMENT", element.getSize() / element.getScalarCount() },
     };
}

ScriptValuePointer jsBindCallback(ScriptValuePointer value) {
    if (value->isObject() && value->property("callback")->isFunction()) {
        // value is already a bound callback
        return value;
    }
    auto engine = value->engine();
    auto context = engine ? engine->currentContext() : nullptr;
    auto length = context ? context->argumentCount() : 0;
    ScriptValuePointer scope = context ? context->thisObject() : engine->nullValue();
    ScriptValuePointer method;
#ifdef SCRIPTABLE_MESH_DEBUG
    qCInfo(graphics_scripting) << "jsBindCallback" << engine << length << scope.toQObject() << method.toString();
#endif

    // find position in the incoming JS Function.arguments array (so we can test for the two-argument case)
    for (int i = 0; context && i < length; i++) {
        if (context->argument(i)->strictlyEquals(value)) {
            method = context->argument(i+1);
        }
    }
    if (method->isFunction() || method->isString()) {
        // interpret as `API.func(..., scope, function callback(){})` or `API.func(..., scope, "methodName")`
        scope = value;
    } else {
        // interpret as `API.func(..., function callback(){})`
        method = value;
    }
#ifdef SCRIPTABLE_MESH_DEBUG
    qCInfo(graphics_scripting) << "scope:" << scope.toQObject() << "method:" << method.toString();
#endif
    return ::makeScopedHandlerObject(scope,  method);
}

template <typename T>
T this_qobject_cast(ScriptEngine* engine) {
    auto context = engine ? engine->currentContext() : nullptr;
    return scriptvalue_cast<T>(context ? context->thisObject() : ScriptValuePointer::NullValue);
}
QString toDebugString(QObject* tmp) {
    QString s;
    QTextStream out(&s);
    out << tmp;
    return s;
    // return QString("%0 (0x%1%2)")
    //     .arg(tmp ? tmp->metaObject()->className() : "QObject")
    //     .arg(qulonglong(tmp), 16, 16, QChar('0'))
    //     .arg(tmp && tmp->objectName().size() ? " name=" + tmp->objectName() : "");
}
template <typename T> QString toDebugString(std::shared_ptr<T> tmp) {
    return toDebugString(qobject_cast<QObject*>(tmp.get()));
}

}
