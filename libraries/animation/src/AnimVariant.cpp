//
//  AnimVariant.cpp
//  library/animation
//
//  Created by Howard Stearns on 10/15/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimVariant.h" // which has AnimVariant/AnimVariantMap

#include <ScriptEngine.h>
#include <QThread>
#include <ScriptValueIterator.h>
#include <ScriptValueUtils.h>

const AnimVariant AnimVariant::False = AnimVariant();

ScriptValuePointer AnimVariantMap::animVariantMapToScriptValue(ScriptEngine* engine, const QStringList& names, bool useNames) const {
    if (QThread::currentThread() != engine->thread()) {
        qCWarning(animation) << "Cannot create Javacript object from non-script thread" << QThread::currentThread();
        Q_ASSERT(false);
        return ScriptValuePointer();
    }
    ScriptValuePointer target = engine->newObject();
    auto setOne = [&] (const QString& name, const AnimVariant& value) {
        switch (value.getType()) {
            case AnimVariant::Type::Bool:
                target->setProperty(name, value.getBool());
                break;
            case AnimVariant::Type::Int:
                target->setProperty(name, value.getInt());
                break;
            case AnimVariant::Type::Float:
                target->setProperty(name, value.getFloat());
                break;
            case AnimVariant::Type::String:
                target->setProperty(name, value.getString());
                break;
            case AnimVariant::Type::Vec3:
                target->setProperty(name, vec3ToScriptValue(engine, value.getVec3()));
                break;
            case AnimVariant::Type::Quat:
                target->setProperty(name, quatToScriptValue(engine, value.getQuat()));
                break;
            default:
                // Unknown type
                assert(QString("AnimVariant::Type") == QString("valid"));
        }
    };
    if (useNames) { // copy only the requested names
        for (const QString& name : names) {
            auto search = _map.find(name);
            if (search != _map.end()) {
                setOne(name, search->second);
            } else if (_triggers.count(name) == 1) {
                target->setProperty(name, true);
            } // scripts are allowed to request names that do not exist
        }

    } else {  // copy all of them
        for (auto& pair : _map) {
            setOne(pair.first, pair.second);
        }
    }
    return target;
}

void AnimVariantMap::copyVariantsFrom(const AnimVariantMap& other) {
    for (auto& pair : other._map) {
        _map[pair.first] = pair.second;
    }
}

void AnimVariantMap::animVariantMapFromScriptValue(const ScriptValuePointer& source) {
    if (QThread::currentThread() != source->engine()->thread()) {
        qCWarning(animation) << "Cannot examine Javacript object from non-script thread" << QThread::currentThread();
        Q_ASSERT(false);
        return;
    }
    // POTENTIAL OPTIMIZATION: cache the types we've seen. I.e, keep a dictionary mapping property names to an enumeration of types.
    // Whenever we identify a new outbound type in animVariantMapToScriptValue above, or a new inbound type in the code that follows here,
    // we would enter it into the dictionary. Then switch on that type here, with the code that follow being executed only if
    // the type is not known. One problem with that is that there is no checking that two different script use the same name differently.
    ScriptValueIteratorPointer property(source->newIterator());
    // Note: ScriptValueIterator iterates only over source's own properties. It does not follow the prototype chain.
    while (property->hasNext()) {
        property->next();
        ScriptValuePointer value = property->value();
        if (value->isBool()) {
            set(property->name(), value->toBool());
        } else if (value->isString()) {
            set(property->name(), value->toString());
        } else if (value->isNumber()) {
            int asInteger = value->toInt32();
            float asFloat = value->toNumber();
            if (asInteger == asFloat) {
                set(property->name(), asInteger);
            } else {
                set(property->name(), asFloat);
            }
        } else { // Try to get x,y,z and possibly w
            if (value->isObject()) {
                ScriptValuePointer x = value->property("x");
                if (x->isNumber()) {
                    ScriptValuePointer y = value->property("y");
                    if (y->isNumber()) {
                        ScriptValuePointer z = value->property("z");
                        if (z->isNumber()) {
                            ScriptValuePointer w = value->property("w");
                            if (w->isNumber()) {
                                set(property->name(), glm::quat(w->toNumber(), x->toNumber(), y->toNumber(), z->toNumber()));
                            } else {
                                set(property->name(), glm::vec3(x->toNumber(), y->toNumber(), z->toNumber()));
                            }
                            continue; // we got either a vector or quaternion object, so don't fall through to warning
                        }
                    }
                }
            }
            qCWarning(animation) << "Ignoring unrecognized data" << value->toString() << "for animation property" << property->name();
            Q_ASSERT(false);
        }
    }
}

std::map<QString, QString> AnimVariantMap::toDebugMap() const {
    std::map<QString, QString> result;
    for (auto& pair : _map) {
        switch (pair.second.getType()) {
        case AnimVariant::Type::Bool:
            result[pair.first] = QString("%1").arg(pair.second.getBool());
            break;
        case AnimVariant::Type::Int:
            result[pair.first] = QString("%1").arg(pair.second.getInt());
            break;
        case AnimVariant::Type::Float:
            result[pair.first] = QString::number(pair.second.getFloat(), 'f', 3);
            break;
        case AnimVariant::Type::Vec3: {
            // To prevent filling up debug stats, don't show vec3 values
            glm::vec3 value = pair.second.getVec3();
            result[pair.first] = QString("(%1, %2, %3)").
                arg(QString::number(value.x, 'f', 3)).
                arg(QString::number(value.y, 'f', 3)).
                arg(QString::number(value.z, 'f', 3));
            break;
        }
        case AnimVariant::Type::Quat: {
            // To prevent filling up the anim stats, don't show quat values
            glm::quat value = pair.second.getQuat();
            result[pair.first] = QString("(%1, %2, %3, %4)").
                arg(QString::number(value.x, 'f', 3)).
                arg(QString::number(value.y, 'f', 3)).
                arg(QString::number(value.z, 'f', 3)).
                arg(QString::number(value.w, 'f', 3));
            break;
        }
        case AnimVariant::Type::String:
            // To prevent filling up anim stats, don't show string values
            result[pair.first] = pair.second.getString();
            break;
        default:
            // invalid AnimVariant::Type
            assert(false);
        }
    }
    return result;
}
