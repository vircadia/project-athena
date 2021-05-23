//
//  EntityEditFilters.cpp
//  libraries/entities/src
//
//  Created by David Kelly on 2/7/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityEditFilters.h"

#include <QUrl>

#include <ResourceManager.h>
#include <shared/ScriptInitializerMixin.h>
#include <ScriptEngine.h>
#include <ScriptManager.h>
#include <ScriptProgram.h>

QList<EntityItemID> EntityEditFilters::getZonesByPosition(glm::vec3& position) {
    QList<EntityItemID> zones;
    QList<EntityItemID> missingZones;
    _lock.lockForRead();
    auto zoneIDs = _filterDataMap.keys();
    _lock.unlock();
    for (auto id : zoneIDs) {
        if (!id.isInvalidID()) {
            // for now, look it up in the tree (soon we need to cache or similar?)
            EntityItemPointer itemPtr = _tree->findEntityByEntityItemID(id); 
            auto zone = std::dynamic_pointer_cast<ZoneEntityItem>(itemPtr);
            if (!zone) {
                // TODO: maybe remove later?
                removeFilter(id);
            } else if (zone->contains(position)) {
                zones.append(id);
            }
        } else {
            // the null id is the global filter we put in the domain server's 
            // advanced entity server settings
            zones.append(id);
        }
    }
    return zones;
}

bool EntityEditFilters::filter(glm::vec3& position, EntityItemProperties& propertiesIn, EntityItemProperties& propertiesOut,
        bool& wasChanged, EntityTree::FilterType filterType, EntityItemID& itemID, const EntityItemPointer& existingEntity) {
    
    // get the ids of all the zones (plus the global entity edit filter) that the position
    // lies within
    auto zoneIDs = getZonesByPosition(position);
    for (auto id : zoneIDs) {
        if (!itemID.isInvalidID() && id == itemID) {
            continue;
        }
        
        // get the filter pair, etc...  
        _lock.lockForRead();
        FilterData filterData = _filterDataMap.value(id);
        _lock.unlock();
    
        if (filterData.valid()) {
            if (filterData.rejectAll) {
                return false;
            }

            // check to see if this filter wants to filter this message type
            if ((!filterData.wantsToFilterEdit && filterType == EntityTree::FilterType::Edit) ||
                (!filterData.wantsToFilterPhysics && filterType == EntityTree::FilterType::Physics) ||
                (!filterData.wantsToFilterDelete && filterType == EntityTree::FilterType::Delete) ||
                (!filterData.wantsToFilterAdd && filterType == EntityTree::FilterType::Add)) {

                wasChanged = false;
                return true; // accept the message
            }

            auto oldProperties = propertiesIn.getDesiredProperties();
            auto specifiedProperties = propertiesIn.getChangedProperties();
            propertiesIn.setDesiredProperties(specifiedProperties);
            ScriptValuePointer inputValues = propertiesIn.copyToScriptValue(filterData.engine, false, true, true);
            propertiesIn.setDesiredProperties(oldProperties);

            auto in = QJsonValue::fromVariant(inputValues->toVariant()); // grab json copy now, because the inputValues might be side effected by the filter.

            ScriptValueList args;
            args << inputValues;
            args << filterData.engine->newValue(filterType);

            // get the current properties for then entity and include them for the filter call
            if (existingEntity && filterData.wantsOriginalProperties) {
                auto currentProperties = existingEntity->getProperties(filterData.includedOriginalProperties);
                ScriptValuePointer currentValues = currentProperties.copyToScriptValue(filterData.engine, false, true, true);
                args << currentValues;
            }


            // get the zone properties
            if (filterData.wantsZoneProperties) {
                auto zoneEntity = _tree->findEntityByEntityItemID(id);
                if (zoneEntity) {
                    auto zoneProperties = zoneEntity->getProperties(filterData.includedZoneProperties);
                    ScriptValuePointer zoneValues = zoneProperties.copyToScriptValue(filterData.engine, false, true, true);

                    if (filterData.wantsZoneBoundingBox) {
                        bool success = true;
                        AABox aaBox = zoneEntity->getAABox(success);
                        if (success) {
                            ScriptValuePointer boundingBox = filterData.engine->newObject();
                            ScriptValuePointer bottomRightNear = vec3ToScriptValue(filterData.engine, aaBox.getCorner());
                            ScriptValuePointer topFarLeft = vec3ToScriptValue(filterData.engine, aaBox.calcTopFarLeft());
                            ScriptValuePointer center = vec3ToScriptValue(filterData.engine, aaBox.calcCenter());
                            ScriptValuePointer boundingBoxDimensions = vec3ToScriptValue(filterData.engine, aaBox.getDimensions());
                            boundingBox->setProperty("brn", bottomRightNear);
                            boundingBox->setProperty("tfl", topFarLeft);
                            boundingBox->setProperty("center", center);
                            boundingBox->setProperty("dimensions", boundingBoxDimensions);
                            zoneValues->setProperty("boundingBox", boundingBox);
                        }
                    }

                    // If this is an add or delete, or original properties weren't requested
                    // there won't be original properties in the args, but zone properties need
                    // to be the fourth parameter, so we need to pad the args accordingly
                    int EXPECTED_ARGS = 3;
                    if (args.length() < EXPECTED_ARGS) {
                        args << ScriptValuePointer();
                    }
                    assert(args.length() == EXPECTED_ARGS); // we MUST have 3 args by now!
                    args << zoneValues;
                }
            }

            ScriptValuePointer result = filterData.filterFn->call(_nullObjectForFilter, args);

            if (filterData.uncaughtExceptions()) {
                return false;
            }

            if (result->isObject()) {
                // make propertiesIn reflect the changes, for next filter...
                propertiesIn.copyFromScriptValue(result, false);

                // and update propertiesOut too.  TODO: this could be more efficient...
                propertiesOut.copyFromScriptValue(result, false);
                // Javascript objects are == only if they are the same object. To compare arbitrary values, we need to use JSON.
                auto out = QJsonValue::fromVariant(result->toVariant());
                wasChanged |= (in != out);
            } else if (result->isBool()) {

                // if the filter returned false, then it's authoritative
                if (!result->toBool()) {
                    return false;
                }

                // otherwise, assume it wants to pass all properties
                propertiesOut = propertiesIn;
                wasChanged = false;
                
            } else {
                return false;
            }
        }
    }
    // if we made it here, 
    return true;
}

void EntityEditFilters::removeFilter(EntityItemID entityID) {
    QWriteLocker writeLock(&_lock);
    FilterData filterData = _filterDataMap.value(entityID);
    if (filterData.valid()) {
        delete filterData.engine;
    }
    _filterDataMap.remove(entityID);
}

void EntityEditFilters::addFilter(EntityItemID entityID, QString filterURL) {

    QUrl scriptURL(filterURL);
    
    // setting it to an empty string is same as removing 
    if (filterURL.size() == 0) {
        removeFilter(entityID);
        return;
    }
   
    // The following should be abstracted out for use in Agent.cpp (and maybe later AvatarMixer.cpp)
    if (scriptURL.scheme().isEmpty() || (scriptURL.scheme() == HIFI_URL_SCHEME_FILE)) {
        qWarning() << "Cannot load script from local filesystem, because assignment may be on a different computer.";
        scriptRequestFinished(entityID);
        return;
    }
   
    // first remove any existing info for this entity
    removeFilter(entityID);
    
    // reject all edits until we load the script
    FilterData filterData;
    filterData.rejectAll = true;

    _lock.lockForWrite();
    _filterDataMap.insert(entityID, filterData);
    _lock.unlock();
   
    auto scriptRequest = DependencyManager::get<ResourceManager>()->createResourceRequest(
        this, scriptURL, true, -1, "EntityEditFilters::addFilter");
    if (!scriptRequest) {
        qWarning() << "Could not create ResourceRequest for Entity Edit filter.";
        scriptRequestFinished(entityID);
        return;
    }
    // Agent.cpp sets up a timeout here, but that is unnecessary, as ResourceRequest has its own.
    connect(scriptRequest, &ResourceRequest::finished, this, [this, entityID]{ EntityEditFilters::scriptRequestFinished(entityID);} );
    // FIXME: handle atp rquests setup here. See Agent::requestScript()
    scriptRequest->send();
    qDebug() << "script request sent for entity " << entityID;
}

// Copied from ScriptEngine.cpp. We should make this a class method for reuse.
// Note: I've deliberately stopped short of using ScriptEngine instead of ScriptEngine, as that is out of project scope at this point.
static bool hasCorrectSyntax(const ScriptProgramPointer& program) {
    const auto syntaxCheck = program->checkSyntax();
    if (syntaxCheck->state() != ScriptSyntaxCheckResult::Valid) {
        const auto error = syntaxCheck->errorMessage();
        const auto line = QString::number(syntaxCheck->errorLineNumber());
        const auto column = QString::number(syntaxCheck->errorColumnNumber());
        const auto message = QString("[SyntaxError] %1 in %2:%3(%4)").arg(error, program->fileName(), line, column);
        qCritical() << qPrintable(message);
        return false;
    }
    return true;
}
static bool hadUncaughtExceptions(ScriptEngine& engine, const QString& fileName) {
    if (engine.hasUncaughtException()) {
        const auto backtrace = engine.uncaughtExceptionBacktrace();
        const auto exception = engine.uncaughtException()->toString();
        const auto line = QString::number(engine.uncaughtExceptionLineNumber());
        engine.clearExceptions();

        static const QString SCRIPT_EXCEPTION_FORMAT = "[UncaughtException] %1 in %2:%3";
        auto message = QString(SCRIPT_EXCEPTION_FORMAT).arg(exception, fileName, line);
        if (!backtrace.empty()) {
            static const auto lineSeparator = "\n    ";
            message += QString("\n[Backtrace]%1%2").arg(lineSeparator, backtrace.join(lineSeparator));
        }
        qCritical() << qPrintable(message);
        return true;
    }
    return false;
}

void EntityEditFilters::scriptRequestFinished(EntityItemID entityID) {
    qDebug() << "script request completed for entity " << entityID;
    auto scriptRequest = qobject_cast<ResourceRequest*>(sender());
    if (scriptRequest && scriptRequest->getResult() == ResourceRequest::Success) {
        const QString urlString = scriptRequest->getUrl().toString();
        auto scriptContents = scriptRequest->getData();
        qInfo() << "Downloaded script:" << scriptContents;
        // create a ScriptEngine for this script
        ScriptManagerPointer manager = newScriptManager(ScriptManager::ENTITY_SERVER_SCRIPT, "", urlString);
        ScriptEnginePointer engine = manager->engine();
        ScriptProgramPointer program = engine->newProgram(scriptContents, urlString);
        if (hasCorrectSyntax(program)) {
            engine->setObjectName("filter:" + entityID.toString());
            engine->setProperty("type", "edit_filter");
            engine->setProperty("fileName", urlString);
            engine->setProperty("entityID", entityID);
            engine->globalObject()->setProperty("Script", engine->newQObject(manager.get()));
            DependencyManager::get<ScriptInitializers>()->runScriptInitializers(engine.data());
            engine->evaluate(scriptContents, urlString);
            if (!hadUncaughtExceptions(*engine, urlString)) {
                // put the engine in the engine map (so we don't leak them, etc...)
                FilterData filterData;
                filterData.engine = engine.get();
                filterData.rejectAll = false;
                
                // define the uncaughtException function
                ScriptEngine& engineRef = *engine;
                filterData.uncaughtExceptions = [&engineRef, urlString]() { return hadUncaughtExceptions(engineRef, urlString); };

                // now get the filter function
                auto global = engine->globalObject();
                auto entitiesObject = engine->newObject();
                entitiesObject->setProperty("ADD_FILTER_TYPE", EntityTree::FilterType::Add);
                entitiesObject->setProperty("EDIT_FILTER_TYPE", EntityTree::FilterType::Edit);
                entitiesObject->setProperty("PHYSICS_FILTER_TYPE", EntityTree::FilterType::Physics);
                entitiesObject->setProperty("DELETE_FILTER_TYPE", EntityTree::FilterType::Delete);
                global->setProperty("Entities", entitiesObject);
                filterData.filterFn = global->property("filter");
                if (!filterData.filterFn->isFunction()) {
                    qDebug() << "Filter function specified but not found. Will reject all edits for those without lock rights.";
                    engine.reset();
                    filterData.rejectAll=true;
                }

                // if the wantsToFilterEdit is a boolean evaluate as a boolean, otherwise assume true
                ScriptValuePointer wantsToFilterAddValue = filterData.filterFn->property("wantsToFilterAdd");
                filterData.wantsToFilterAdd = wantsToFilterAddValue->isBool() ? wantsToFilterAddValue->toBool() : true;

                // if the wantsToFilterEdit is a boolean evaluate as a boolean, otherwise assume true
                ScriptValuePointer wantsToFilterEditValue = filterData.filterFn->property("wantsToFilterEdit");
                filterData.wantsToFilterEdit = wantsToFilterEditValue->isBool() ? wantsToFilterEditValue->toBool() : true;

                // if the wantsToFilterPhysics is a boolean evaluate as a boolean, otherwise assume true
                ScriptValuePointer wantsToFilterPhysicsValue = filterData.filterFn->property("wantsToFilterPhysics");
                filterData.wantsToFilterPhysics = wantsToFilterPhysicsValue->isBool() ? wantsToFilterPhysicsValue->toBool() : true;

                // if the wantsToFilterDelete is a boolean evaluate as a boolean, otherwise assume false
                ScriptValuePointer wantsToFilterDeleteValue = filterData.filterFn->property("wantsToFilterDelete");
                filterData.wantsToFilterDelete = wantsToFilterDeleteValue->isBool() ? wantsToFilterDeleteValue->toBool() : false;

                // check to see if the filterFn has properties asking for Original props
                ScriptValuePointer wantsOriginalPropertiesValue = filterData.filterFn->property("wantsOriginalProperties");
                // if the wantsOriginalProperties is a boolean, or a string, or list of strings, then evaluate as follows:
                //   - boolean - true  - include all original properties
                //               false - no properties at all
                //   - string  - empty - no properties at all
                //               any valid property - include just that property in the Original properties
                //   - list of strings - include only those properties in the Original properties
                if (wantsOriginalPropertiesValue->isBool()) {
                    filterData.wantsOriginalProperties = wantsOriginalPropertiesValue->toBool();
                } else if (wantsOriginalPropertiesValue->isString()) {
                    auto stringValue = wantsOriginalPropertiesValue->toString();
                    filterData.wantsOriginalProperties = !stringValue.isEmpty();
                    if (filterData.wantsOriginalProperties) {
                        EntityPropertyFlagsFromScriptValue(wantsOriginalPropertiesValue, filterData.includedOriginalProperties);
                    }
                } else if (wantsOriginalPropertiesValue->isArray()) {
                    EntityPropertyFlagsFromScriptValue(wantsOriginalPropertiesValue, filterData.includedOriginalProperties);
                    filterData.wantsOriginalProperties = !filterData.includedOriginalProperties.isEmpty();
                }

                // check to see if the filterFn has properties asking for Zone props
                ScriptValuePointer wantsZonePropertiesValue = filterData.filterFn->property("wantsZoneProperties");
                // if the wantsZoneProperties is a boolean, or a string, or list of strings, then evaluate as follows:
                //   - boolean - true  - include all Zone properties
                //               false - no properties at all
                //   - string  - empty - no properties at all
                //               any valid property - include just that property in the Zone properties
                //   - list of strings - include only those properties in the Zone properties
                if (wantsZonePropertiesValue->isBool()) {
                    filterData.wantsZoneProperties = wantsZonePropertiesValue->toBool();
                    filterData.wantsZoneBoundingBox = filterData.wantsZoneProperties; // include this too
                } else if (wantsZonePropertiesValue->isString()) {
                    auto stringValue = wantsZonePropertiesValue->toString();
                    filterData.wantsZoneProperties = !stringValue.isEmpty();
                    if (filterData.wantsZoneProperties) {
                        if (stringValue == "boundingBox") {
                            filterData.wantsZoneBoundingBox = true;
                        } else {
                            EntityPropertyFlagsFromScriptValue(wantsZonePropertiesValue, filterData.includedZoneProperties);
                        }
                    }
                } else if (wantsZonePropertiesValue->isArray()) {
                    auto length = wantsZonePropertiesValue->property("length")->toInteger();
                    for (int i = 0; i < length; i++) {
                        auto stringValue = wantsZonePropertiesValue->property(i)->toString();
                        if (!stringValue.isEmpty()) {
                            filterData.wantsZoneProperties = true;

                            // boundingBox is a special case since it's not a true EntityPropertyFlag, so we
                            // need to detect it here.
                            if (stringValue == "boundingBox") {
                                filterData.wantsZoneBoundingBox = true;
                                break; // we can break here, since there are no other special cases
                            }

                        }
                    }
                    if (filterData.wantsZoneProperties) {
                        EntityPropertyFlagsFromScriptValue(wantsZonePropertiesValue, filterData.includedZoneProperties);
                    }
                }

                _lock.lockForWrite();
                _filterDataMap.insert(entityID, filterData);
                _lock.unlock();

                qDebug() << "script request filter processed for entity id " << entityID;
                
                emit filterAdded(entityID, true);
                return;
            }
        } 
    } else if (scriptRequest) {
        const QString urlString = scriptRequest->getUrl().toString();
        qCritical() << "Failed to download script";
        // See HTTPResourceRequest::onRequestFinished for interpretation of codes. For example, a 404 is code 6 and 403 is 3. A timeout is 2. Go figure.
        qCritical() << "ResourceRequest error was" << scriptRequest->getResult();
    } else {
        qCritical() << "Failed to create script request.";
    }
    emit filterAdded(entityID, false);
}
