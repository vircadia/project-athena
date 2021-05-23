//
//  Created by Sam Gateau on 4/27/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "UserInputMapper.h"

#include <set>

#include <QtCore/QThread>
#include <QtCore/QFile>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <PathUtils.h>
#include <NumericalConstants.h>

#include <StreamUtils.h>

#include "StandardController.h"
#include "StateController.h"
#include "InputRecorder.h"
#include "Logging.h"
#include "ScriptValueUtils.h"
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptValue.h>

#include "impl/conditionals/AndConditional.h"
#include "impl/conditionals/NotConditional.h"
#include "impl/conditionals/EndpointConditional.h"
#include "impl/conditionals/ScriptConditional.h"

#include "impl/endpoints/ActionEndpoint.h"
#include "impl/endpoints/AnyEndpoint.h"
#include "impl/endpoints/ArrayEndpoint.h"
#include "impl/endpoints/CompositeEndpoint.h"
#include "impl/endpoints/InputEndpoint.h"
#include "impl/endpoints/JSEndpoint.h"
#include "impl/endpoints/ScriptEndpoint.h"
#include "impl/endpoints/StandardEndpoint.h"

#include "impl/Route.h"
#include "impl/Mapping.h"


namespace controller {
    const uint16_t UserInputMapper::STANDARD_DEVICE = 0;
    const uint16_t UserInputMapper::ACTIONS_DEVICE = Input::invalidInput().device - 0x00FF;
    const uint16_t UserInputMapper::STATE_DEVICE = Input::invalidInput().device - 0x0100;
}

// Default contruct allocate the poutput size with the current hardcoded action channels
controller::UserInputMapper::UserInputMapper() {
    registerDevice(std::make_shared<ActionsDevice>());
    registerDevice(_stateDevice = std::make_shared<StateController>());
    registerDevice(std::make_shared<StandardController>());
}

namespace controller {


UserInputMapper::~UserInputMapper() {
}

void UserInputMapper::registerDevice(InputDevice::Pointer device) {
    Locker locker(_lock);
    if (device->_deviceID == Input::INVALID_DEVICE) {
        device->_deviceID = getFreeDeviceID();
    }
    const auto& deviceID = device->_deviceID;

    qCDebug(controllers) << "Registered input device <" << device->getName() << "> deviceID = " << deviceID;

    auto inputs = device->getAvailableInputs();
    for (const auto& inputMapping : inputs) {
        const auto& input = inputMapping.first;
        // Ignore aliases
        if (_endpointsByInput.count(input)) {
            continue;
        }

        Endpoint::Pointer endpoint = device->createEndpoint(input);
        if (!endpoint) {
            if (input.device == STANDARD_DEVICE) {
                endpoint = std::make_shared<StandardEndpoint>(input);
            } else if (input.device == ACTIONS_DEVICE) {
                endpoint = std::make_shared<ActionEndpoint>(input);
            } else {
                endpoint = std::make_shared<InputEndpoint>(input);
            }
        }
        _inputsByEndpoint[endpoint] = input;
        _endpointsByInput[input] = endpoint;
    }

    _registeredDevices[deviceID] = device;

    auto mapping = loadMappings(device->getDefaultMappingConfigs());
    if (mapping) {
        _mappingsByDevice[deviceID] = mapping;
        enableMapping(mapping);
    }

    emit hardwareChanged();
}

void UserInputMapper::removeDevice(int deviceID) {

    Locker locker(_lock);
    auto proxyEntry = _registeredDevices.find(deviceID);

    if (_registeredDevices.end() == proxyEntry) {
        qCWarning(controllers) << "Attempted to remove unknown device " << deviceID;
        return;
    }

    auto device = proxyEntry->second;
    qCDebug(controllers) << "Unregistering input device <" << device->getName() << "> deviceID = " << deviceID;

    unloadMappings(device->getDefaultMappingConfigs());

    auto mappingsEntry = _mappingsByDevice.find(deviceID);
    if (_mappingsByDevice.end() != mappingsEntry) {
        disableMapping(mappingsEntry->second);
        _mappingsByDevice.erase(mappingsEntry);
    }

    auto inputs = device->getAvailableInputs();
    for (const auto& inputMapping : inputs) {
        const auto& input = inputMapping.first;
        auto endpoint = _endpointsByInput.find(input);
        if (endpoint != _endpointsByInput.end()) {
            _inputsByEndpoint.erase((*endpoint).second);
            _endpointsByInput.erase(input);
        }
    }

    _registeredDevices.erase(proxyEntry);

    emit hardwareChanged();
}


void UserInputMapper::loadDefaultMapping(uint16 deviceID) {
    Locker locker(_lock);
    auto proxyEntry = _registeredDevices.find(deviceID);
    if (_registeredDevices.end() == proxyEntry) {
        qCWarning(controllers) << "Unknown deviceID " << deviceID;
        return;
    }

    auto mapping = loadMappings(proxyEntry->second->getDefaultMappingConfigs());
    if (mapping) {
        auto prevMapping = _mappingsByDevice[deviceID];
        disableMapping(prevMapping);

        _mappingsByDevice[deviceID] = mapping;
        enableMapping(mapping);
    }

    emit hardwareChanged();
}

InputDevice::Pointer UserInputMapper::getDevice(const Input& input) {
    Locker locker(_lock);
    auto device = _registeredDevices.find(input.getDevice());
    if (device != _registeredDevices.end()) {
        return (device->second);
    } else {
        return InputDevice::Pointer();
    }
}

QString UserInputMapper::getDeviceName(uint16 deviceID) {
    Locker locker(_lock);
    if (_registeredDevices.find(deviceID) != _registeredDevices.end()) {
        return _registeredDevices[deviceID]->_name;
    }
    return QString("unknown");
}

int UserInputMapper::findDevice(QString name) const {
    Locker locker(_lock);
    for (const auto& device : _registeredDevices) {
        if (device.second->_name == name) {
            return device.first;
        }
    }
    return Input::INVALID_DEVICE;
}

QVector<QString> UserInputMapper::getDeviceNames() {
    Locker locker(_lock);
    QVector<QString> result;
    for (const auto& device : _registeredDevices) {
        QString deviceName = device.second->_name.split(" (")[0];
        result << deviceName;
    }
    return result;
}

int UserInputMapper::findAction(const QString& actionName) const {
    return findDeviceInput("Actions." + actionName).getChannel();
}

Input UserInputMapper::findDeviceInput(const QString& inputName) const {
    Locker locker(_lock);
    // Split the full input name as such: deviceName.inputName
    auto names = inputName.split('.');

    if (names.size() >= 2) {
        // Get the device name:
        auto deviceName = names[0];
        auto inputName = names[1];

        int deviceID = findDevice(deviceName);
        if (deviceID != Input::INVALID_DEVICE) {
            const auto& device = _registeredDevices.at(deviceID);
            auto deviceInputs = device->getAvailableInputs();

            for (const auto& input : deviceInputs) {
                if (input.second == inputName) {
                    return input.first;
                }
            }

            qCDebug(controllers) << "Couldn\'t find InputChannel named <" << inputName << "> for device <" << deviceName << ">";

        } else {
            qCDebug(controllers) << "Couldn\'t find InputDevice named <" << deviceName << ">";
            findDevice(deviceName);
        }
    } else {
        qCDebug(controllers) << "Couldn\'t understand <" << inputName << "> as a valid inputDevice.inputName";
    }

    return Input::INVALID_INPUT;
}

void fixBisectedAxis(float& full, float& negative, float& positive) {
    full = full + (negative * -1.0f) + positive;
    negative = full >= 0.0f ? 0.0f : full * -1.0f;
    positive = full <= 0.0f ? 0.0f : full;
}

void UserInputMapper::update(float deltaTime) {
    Locker locker(_lock);
    InputRecorder* inputRecorder = InputRecorder::getInstance();
    static uint64_t updateCount = 0;
    ++updateCount;

    inputRecorder->resetFrame();
    // Reset the axis state for next loop
    for (auto& channel : _actionStates) {
        channel = 0.0f;
    }
    for (unsigned int i = 0; i < _actionStatesValid.size(); i++) {
        _actionStatesValid[i] = true;
    }

    for (auto& channel : _poseStates) {
        channel = Pose();
    }

    // Run the mappings code
    runMappings();

    // merge the bisected and non-bisected axes for now
    fixBisectedAxis(_actionStates[toInt(Action::TRANSLATE_X)], _actionStates[toInt(Action::LATERAL_LEFT)], _actionStates[toInt(Action::LATERAL_RIGHT)]);
    fixBisectedAxis(_actionStates[toInt(Action::TRANSLATE_Y)], _actionStates[toInt(Action::VERTICAL_DOWN)], _actionStates[toInt(Action::VERTICAL_UP)]);
    fixBisectedAxis(_actionStates[toInt(Action::TRANSLATE_Z)], _actionStates[toInt(Action::LONGITUDINAL_FORWARD)], _actionStates[toInt(Action::LONGITUDINAL_BACKWARD)]);
    fixBisectedAxis(_actionStates[toInt(Action::TRANSLATE_CAMERA_Z)], _actionStates[toInt(Action::BOOM_IN)], _actionStates[toInt(Action::BOOM_OUT)]);
    fixBisectedAxis(_actionStates[toInt(Action::ROTATE_Y)], _actionStates[toInt(Action::YAW_LEFT)], _actionStates[toInt(Action::YAW_RIGHT)]);
    fixBisectedAxis(_actionStates[toInt(Action::ROTATE_X)], _actionStates[toInt(Action::PITCH_UP)], _actionStates[toInt(Action::PITCH_DOWN)]);

    fixBisectedAxis(_actionStates[toInt(Action::RETICLE_X)], _actionStates[toInt(Action::RETICLE_LEFT)], _actionStates[toInt(Action::RETICLE_RIGHT)]);
    fixBisectedAxis(_actionStates[toInt(Action::RETICLE_Y)], _actionStates[toInt(Action::RETICLE_UP)], _actionStates[toInt(Action::RETICLE_DOWN)]);

    static const float EPSILON = 0.01f;
    for (auto i = 0; i < toInt(Action::NUM_ACTIONS); i++) {
        _actionStates[i] *= _actionScales[i];
        // Emit only on change, and emit when moving back to 0
        if (fabsf(_actionStates[i] - _lastActionStates[i]) > EPSILON) {
            _lastActionStates[i] = _actionStates[i];
            emit actionEvent(i, _actionStates[i]);
        }
        // TODO: emit signal for pose changes
    }

    auto standardInputs = getStandardInputs();
    if ((int)_lastStandardStates.size() != standardInputs.size()) {
        _lastStandardStates.resize(standardInputs.size());
        for (auto& lastValue : _lastStandardStates) {
            lastValue = AxisValue();
        }
    }

    for (int i = 0; i < standardInputs.size(); ++i) {
        const auto& input = standardInputs[i].first;
        AxisValue value = getValue(input);
        AxisValue& oldValue = _lastStandardStates[i];
        if (value != oldValue) {
            oldValue = value;
            emit inputEvent(input.id, value.value);
        }
    }
    inputRecorder->frameTick();
}

Input::NamedVector UserInputMapper::getAvailableInputs(uint16 deviceID) const {
    Locker locker(_lock);
    auto iterator = _registeredDevices.find(deviceID);
    if (iterator != _registeredDevices.end()) {
        return iterator->second->getAvailableInputs();
    }
    return Input::NamedVector();
}

QVector<Action> UserInputMapper::getAllActions() const {
    Locker locker(_lock);
    QVector<Action> actions;
    for (auto i = 0; i < toInt(Action::NUM_ACTIONS); i++) {
        actions.append(Action(i));
    }
    return actions;
}

QString UserInputMapper::getActionName(Action action) const {
    Locker locker(_lock);
    auto inputs = getActionInputs();
    for (const auto& actionPair : inputs) {
        if (actionPair.first.channel == toInt(action)) {
            return actionPair.second;
        }
    }
    return QString();
}

QString UserInputMapper::getStandardPoseName(uint16_t pose) {
    Locker locker(_lock);
    auto inputs = getStandardInputs();
    for (const auto& posePair : inputs) {
        if (posePair.first.channel == pose && posePair.first.getType() == ChannelType::POSE) {
            return posePair.second;
        }
    }
    return QString();
}

QVector<QString> UserInputMapper::getActionNames() const {
    Locker locker(_lock);
    QVector<QString> result;
    auto inputs = getActionInputs();
    for (const auto& actionPair : inputs) {
        result << actionPair.second;
    }
    return result;
}

Pose UserInputMapper::getPoseState(Action action) const {
    assert(QThread::currentThread() == thread());
    return _poseStates[toInt(action)];
}


bool UserInputMapper::triggerHapticPulse(float strength, float duration, uint16_t index) {
    Locker locker(_lock);
    bool toReturn = false;
    for (const auto& device : _registeredDevices) {
        toReturn = device.second->triggerHapticPulse(strength, duration, index) || toReturn;
    }
    return toReturn;
}

bool UserInputMapper::triggerHapticPulseOnDevice(uint16 deviceID, float strength, float duration, uint16_t index) {
    Locker locker(_lock);
    if (_registeredDevices.find(deviceID) != _registeredDevices.end()) {
        return _registeredDevices[deviceID]->triggerHapticPulse(strength, duration, index);
    }
    return false;
}

int actionMetaTypeId = qRegisterMetaType<Action>();
int inputMetaTypeId = qRegisterMetaType<Input>();
int inputPairMetaTypeId = qRegisterMetaType<Input::NamedPair>();
int poseMetaTypeId = qRegisterMetaType<controller::Pose>("Pose");
int handMetaTypeId = qRegisterMetaType<controller::Hand>();

ScriptValuePointer inputToScriptValue(ScriptEngine* engine, const Input& input);
void inputFromScriptValue(const ScriptValuePointer& object, Input& input);
ScriptValuePointer actionToScriptValue(ScriptEngine* engine, const Action& action);
void actionFromScriptValue(const ScriptValuePointer& object, Action& action);
ScriptValuePointer inputPairToScriptValue(ScriptEngine* engine, const Input::NamedPair& inputPair);
void inputPairFromScriptValue(const ScriptValuePointer& object, Input::NamedPair& inputPair);
ScriptValuePointer handToScriptValue(ScriptEngine* engine, const controller::Hand& hand);
void handFromScriptValue(const ScriptValuePointer& object, controller::Hand& hand);

ScriptValuePointer inputToScriptValue(ScriptEngine* engine, const Input& input) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("device", input.getDevice());
    obj->setProperty("channel", input.getChannel());
    obj->setProperty("type", (unsigned short)input.getType());
    obj->setProperty("id", input.getID());
    return obj;
}

void inputFromScriptValue(const ScriptValuePointer& object, Input& input) {
    input.id = object->property("id")->toInt32();
}

ScriptValuePointer actionToScriptValue(ScriptEngine* engine, const Action& action) {
    ScriptValuePointer obj = engine->newObject();
    auto userInputMapper = DependencyManager::get<UserInputMapper>();
    obj->setProperty("action", (int)action);
    obj->setProperty("actionName", userInputMapper->getActionName(action));
    return obj;
}

void actionFromScriptValue(const ScriptValuePointer& object, Action& action) {
    action = Action(object->property("action")->toVariant().toInt());
}

ScriptValuePointer inputPairToScriptValue(ScriptEngine* engine, const Input::NamedPair& inputPair) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("input", inputToScriptValue(engine, inputPair.first));
    obj->setProperty("inputName", inputPair.second);
    return obj;
}

void inputPairFromScriptValue(const ScriptValuePointer& object, Input::NamedPair& inputPair) {
    inputFromScriptValue(object->property("input"), inputPair.first);
    inputPair.second = QString(object->property("inputName")->toVariant().toString());
}

ScriptValuePointer handToScriptValue(ScriptEngine* engine, const controller::Hand& hand) {
    return engine->newValue((int)hand);
}

void handFromScriptValue(const ScriptValuePointer& object, controller::Hand& hand) {
    hand = Hand(object->toVariant().toInt());
}

void UserInputMapper::registerControllerTypes(ScriptEngine* engine) {
    scriptRegisterSequenceMetaType<QVector<Action> >(engine);
    scriptRegisterSequenceMetaType<Input::NamedVector>(engine);
    scriptRegisterMetaType(engine, actionToScriptValue, actionFromScriptValue);
    scriptRegisterMetaType(engine, inputToScriptValue, inputFromScriptValue);
    scriptRegisterMetaType(engine, inputPairToScriptValue, inputPairFromScriptValue);
    scriptRegisterMetaType(engine, handToScriptValue, handFromScriptValue);

    scriptRegisterMetaType(engine, Pose::toScriptValue, Pose::fromScriptValue);
}

Input UserInputMapper::makeStandardInput(controller::StandardButtonChannel button) {
    return Input(STANDARD_DEVICE, button, ChannelType::BUTTON);
}

Input UserInputMapper::makeStandardInput(controller::StandardAxisChannel axis) {
    return Input(STANDARD_DEVICE, axis, ChannelType::AXIS);
}

Input UserInputMapper::makeStandardInput(controller::StandardPoseChannel pose) {
    return Input(STANDARD_DEVICE, pose, ChannelType::POSE);
}

static auto lastDebugTime = usecTimestampNow();
static auto debugRoutes = false;
static auto debuggableRoutes = false;
static const auto DEBUG_INTERVAL = USECS_PER_SECOND;

void UserInputMapper::runMappings() {
    auto now = usecTimestampNow();
    if (debuggableRoutes && now - lastDebugTime > DEBUG_INTERVAL) {
        lastDebugTime = now;
        debugRoutes = true;
    }

    if (debugRoutes) {
        qCDebug(controllers) << "Beginning mapping frame";
    }
    for (const auto& endpointEntry : _endpointsByInput) {
        endpointEntry.second->reset();
    }

    if (debugRoutes) {
        qCDebug(controllers) << "Processing device routes";
    }
    // Now process the current values for each level of the stack
    applyRoutes(_deviceRoutes);

    if (debugRoutes) {
        qCDebug(controllers) << "Processing standard routes";
    }
    applyRoutes(_standardRoutes);

    InputRecorder* inputRecorder = InputRecorder::getInstance();
    if (inputRecorder->isPlayingback()) {
        if (debugRoutes) {
            qCDebug(controllers) << "Playing back recording actions";
        }

        // Play back each numeric action even if there is no current route active for the action.
        auto actionStates = inputRecorder->getActionstates();
        for (InputRecorder::ActionStates::iterator it = actionStates.begin(); it != actionStates.end(); ++it) {
            setActionState((Action)findAction(it->first), it->second);
        }

        // Poses are played back in StandardEndpoint.
    }

    if (debugRoutes) {
        qCDebug(controllers) << "Done with mappings";
    }
    debugRoutes = false;
}

// Encapsulate the logic that routes should not be read before they are written
void UserInputMapper::applyRoutes(const Route::List& routes) {
    Route::List deferredRoutes;

    for (const auto& route : routes) {
        if (!route) {
            continue;
        }

        // Try all the deferred routes
        deferredRoutes.remove_if([](Route::Pointer route) {
            return UserInputMapper::applyRoute(route);
        });

        if (!applyRoute(route)) {
            deferredRoutes.push_back(route);
        }
    }

    bool force = true;
    for (const auto& route : deferredRoutes) {
        UserInputMapper::applyRoute(route, force);
    }
}


bool UserInputMapper::applyRoute(const Route::Pointer& route, bool force) {
    if (debugRoutes && route->debug) {
        qCDebug(controllers) << "Applying route " << route->json;
    }

    // If the source hasn't been written yet, defer processing of this route
    auto& source = route->source;
    auto& sourceInput = source->getInput();
    if (sourceInput.device == STANDARD_DEVICE && !force && source->writeable()) {
        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Source not yet written, deferring";
        }
        return false;
    }

    if (route->conditional) {
        // FIXME for endpoint conditionals we need to check if they've been written
        if (!route->conditional->satisfied()) {
            if (debugRoutes && route->debug) {
                qCDebug(controllers) << "Conditional failed";
            }
            return true;
        }
    }


    // Most endpoints can only be read once (though a given mapping can route them to
    // multiple places).  Consider... If the default is to wire the A button to JUMP
    // and someone else wires it to CONTEXT_MENU, I don't want both to occur when
    // I press the button.  The exception is if I'm wiring a control back to itself
    // in order to adjust my interface, like inverting the Y axis on an analog stick
    if (!route->peek && !source->readable()) {
        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Source unreadable";
        }
        return true;
    }

    auto& destination = route->destination;
    // THis could happen if the route destination failed to create
    // FIXME: Maybe do not create the route if the destination failed and avoid this case ?
    if (!destination) {
        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Bad Destination";
        }
        return true;
    }

    if (!destination->writeable()) {
        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Destination unwritable";
        }
        return true;
    }

    // Fetch the value, may have been overriden by previous loopback routes
    if (source->isPose()) {
        Pose value = getPose(source, route->peek);
        static const Pose IDENTITY_POSE { vec3(), quat() };

        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Value was t:" << value.translation << "r:" << value.rotation;
        }
        // Apply each of the filters.
        for (const auto& filter : route->filters) {
            value = filter->apply(value);
        }

        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Filtered value was t:" << value.translation << "r:" << value.rotation;

            if (!value.valid) {
                qCDebug(controllers) << "Applying invalid pose";
            } else if (value == IDENTITY_POSE) {
                qCDebug(controllers) << "Applying identity pose";
            } else {
                qCDebug(controllers) << "Applying valid pose";
            }
        }
        // no filters yet for pose
        destination->apply(value, source);
    } else {
        // Fetch the value, may have been overriden by previous loopback routes
        auto value = getValue(source, route->peek);

        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Value was " << value.value << value.timestamp;
        }
        // Apply each of the filters.
        for (const auto& filter : route->filters) {
            value = filter->apply(value);
        }

        if (debugRoutes && route->debug) {
            qCDebug(controllers) << "Filtered value was " << value.value << value.timestamp;
        }

        destination->apply(value, source);
    }
    return true;
}

Endpoint::Pointer UserInputMapper::endpointFor(const QJSValue& endpoint) {
    if (endpoint.isNumber()) {
        return endpointFor(Input(endpoint.toInt()));
    }

    if (endpoint.isCallable()) {
        auto result = std::make_shared<JSEndpoint>(endpoint);
        return result;
    }

    qWarning() << "Unsupported input type " << endpoint.toString();
    return Endpoint::Pointer();
}

Endpoint::Pointer UserInputMapper::endpointFor(const ScriptValuePointer& endpoint) {
    if (endpoint->isNumber()) {
        return endpointFor(Input(endpoint->toInt32()));
    }

    if (endpoint->isFunction()) {
        auto result = std::make_shared<ScriptEndpoint>(endpoint);
        return result;
    }

    if (endpoint->isArray()) {
        int length = endpoint->property("length")->toInteger();
        Endpoint::List children;
        for (int i = 0; i < length; i++) {
            ScriptValuePointer arrayItem = endpoint->property(i);
            Endpoint::Pointer destination = endpointFor(arrayItem);
            if (!destination) {
                return Endpoint::Pointer();
            }
            children.push_back(destination);
        }
        return std::make_shared<AnyEndpoint>(children);
    }


    qWarning() << "Unsupported input type " << endpoint->toString();
    return Endpoint::Pointer();
}

Endpoint::Pointer UserInputMapper::endpointFor(const Input& inputId) const {
    Locker locker(_lock);
    auto iterator = _endpointsByInput.find(inputId);
    if (_endpointsByInput.end() == iterator) {
        qWarning() << "Unknown input: " << QString::number(inputId.getID(), 16);
        return Endpoint::Pointer();
    }
    return iterator->second;
}

Endpoint::Pointer UserInputMapper::compositeEndpointFor(Endpoint::Pointer first, Endpoint::Pointer second) {
    EndpointPair pair(first, second);
    Endpoint::Pointer result;
    auto iterator = _compositeEndpoints.find(pair);
    if (_compositeEndpoints.end() == iterator) {
        result = std::make_shared<CompositeEndpoint>(first, second);
        _compositeEndpoints[pair] = result;
    } else {
        result = iterator->second;
    }
    return result;
}


Mapping::Pointer UserInputMapper::newMapping(const QString& mappingName) {
    Locker locker(_lock);
    if (_mappingsByName.count(mappingName)) {
        qCWarning(controllers) << "Refusing to recreate mapping named " << mappingName;
    }
    qCDebug(controllers) << "Creating new Mapping " << mappingName;
    auto mapping = std::make_shared<Mapping>(mappingName);
    _mappingsByName[mappingName] = mapping;
    return mapping;
}

// FIXME handle asynchronous loading in the UserInputMapper
//QObject* ScriptingInterface::loadMapping(const QString& jsonUrl) {
//    QObject* result = nullptr;
//    auto request = ResourceManager::createResourceRequest(nullptr, QUrl(jsonUrl));
//    if (request) {
//        QEventLoop eventLoop;
//        request->setCacheEnabled(false);
//        connect(request, &ResourceRequest::finished, &eventLoop, &QEventLoop::quit);
//        request->send();
//        if (request->getState() != ResourceRequest::Finished) {
//            eventLoop.exec();
//        }
//
//        if (request->getResult() == ResourceRequest::Success) {
//            result = parseMapping(QString(request->getData()));
//        } else {
//            qCWarning(controllers) << "Failed to load mapping url <" << jsonUrl << ">" << endl;
//        }
//        request->deleteLater();
//    }
//    return result;
//}

void UserInputMapper::enableMapping(const QString& mappingName, bool enable) {
    Locker locker(_lock);
    qCDebug(controllers) << "Attempting to " << (enable ? "enable" : "disable") << " mapping " << mappingName;
    auto iterator = _mappingsByName.find(mappingName);
    if (_mappingsByName.end() == iterator) {
        qCWarning(controllers) << "Request to enable / disable unknown mapping " << mappingName;
        return;
    }

    auto mapping = iterator->second;
    if (enable) {
        enableMapping(mapping);
    } else {
        disableMapping(mapping);
    }
}

AxisValue UserInputMapper::getValue(const Endpoint::Pointer& endpoint, bool peek) {
    return peek ? endpoint->peek() : endpoint->value();
}

AxisValue UserInputMapper::getValue(const Input& input) const {
    Locker locker(_lock);
    auto endpoint = endpointFor(input);
    if (!endpoint) {
        return AxisValue();
    }
    return endpoint->value();
}

Pose UserInputMapper::getPose(const Endpoint::Pointer& endpoint, bool peek) {
    if (!endpoint->isPose()) {
        return Pose();
    }
    return peek ? endpoint->peekPose() : endpoint->pose();
}

Pose UserInputMapper::getPose(const Input& input) const {
    Locker locker(_lock);
    auto endpoint = endpointFor(input);
    if (!endpoint) {
        return Pose();
    }
    return getPose(endpoint);
}

Mapping::Pointer UserInputMapper::loadMapping(const QString& jsonFile, bool enable) {
    Locker locker(_lock);
    if (jsonFile.isEmpty()) {
        return Mapping::Pointer();
    }
    // Each mapping only needs to be loaded once
    if (_loadedRouteJsonFiles.contains(jsonFile)) {
        return Mapping::Pointer();
    }
    _loadedRouteJsonFiles.insert(jsonFile);
    QString json;
    {
        QFile file(jsonFile);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            json = QTextStream(&file).readAll();
        }
        file.close();
    }
    auto result = parseMapping(json);
    if (enable) {
        enableMapping(result->name);
    }
    return result;
}

MappingPointer UserInputMapper::loadMappings(const QStringList& jsonFiles) {
    Mapping::Pointer result;
    for (const QString& jsonFile : jsonFiles) {
        auto subMapping = loadMapping(jsonFile);
        if (subMapping) {
            if (!result) {
                result = subMapping;
            } else {
                auto& routes = result->routes;
                routes.insert(routes.end(), subMapping->routes.begin(), subMapping->routes.end());
            }
        }
    }
    return result;
}

void UserInputMapper::unloadMappings(const QStringList& jsonFiles) {
    for (const QString& jsonFile : jsonFiles) {
        unloadMapping(jsonFile);
    }
}

void UserInputMapper::unloadMapping(const QString& jsonFile) {
    auto entry = _loadedRouteJsonFiles.find(jsonFile);
    if (entry != _loadedRouteJsonFiles.end()) {
        _loadedRouteJsonFiles.erase(entry);
    }
}

static const QString JSON_NAME = QStringLiteral("name");
static const QString JSON_CHANNELS = QStringLiteral("channels");
static const QString JSON_CHANNEL_FROM = QStringLiteral("from");
static const QString JSON_CHANNEL_DEBUG = QStringLiteral("debug");
static const QString JSON_CHANNEL_PEEK = QStringLiteral("peek");
static const QString JSON_CHANNEL_WHEN = QStringLiteral("when");
static const QString JSON_CHANNEL_TO = QStringLiteral("to");
static const QString JSON_CHANNEL_FILTERS = QStringLiteral("filters");

Endpoint::Pointer UserInputMapper::parseEndpoint(const QJsonValue& value) {
    Endpoint::Pointer result;
    if (value.isString()) {
        auto input = findDeviceInput(value.toString());
        result = endpointFor(input);
    } else if (value.isArray()) {
        return parseAny(value);
    } else if (value.isObject()) {
        auto axisEndpoint = parseAxis(value);
        if (axisEndpoint) {
            return axisEndpoint;
        }
        // if we have other types of endpoints that are objects, follow the axisEndpoint example, and place them here

        // Endpoint is defined as an object, we expect a js function then
        return Endpoint::Pointer();
    }

    if (!result) {
        qWarning() << "Invalid endpoint definition " << value;
    }
    return result;
}


Conditional::Pointer UserInputMapper::conditionalFor(const QJSValue& condition) {
    return Conditional::Pointer();
}

Conditional::Pointer UserInputMapper::conditionalFor(const ScriptValuePointer& condition) {
    if (condition->isArray()) {
        int length = condition->property("length")->toInteger();
        Conditional::List children;
        for (int i = 0; i < length; i++) {
            Conditional::Pointer destination = conditionalFor(condition->property(i));
            if (!destination) {
                return Conditional::Pointer();
            }
            children.push_back(destination);
        }
        return std::make_shared<AndConditional>(children);
    }

    if (condition->isNumber()) {
        return conditionalFor(Input(condition->toInt32()));
    }

    if (condition->isFunction()) {
        return std::make_shared<ScriptConditional>(condition);
    }

    qWarning() << "Unsupported conditional type " << condition->toString();
    return Conditional::Pointer();
}

Conditional::Pointer UserInputMapper::conditionalFor(const Input& inputId) const {
    Locker locker(_lock);
    auto iterator = _endpointsByInput.find(inputId);
    if (_endpointsByInput.end() == iterator) {
        qWarning() << "Unknown input: " << QString::number(inputId.getID(), 16);
        return Conditional::Pointer();
    }
    return std::make_shared<EndpointConditional>(iterator->second);
}

Conditional::Pointer UserInputMapper::parseConditional(const QJsonValue& value) {
    if (value.isArray()) {
        // Support "when" : [ "GamePad.RB", "GamePad.LB" ]
        Conditional::List children;
        auto array = value.toArray();
        for (const auto& arrayItem : array) {
            Conditional::Pointer childConditional = parseConditional(arrayItem);
            if (!childConditional) {
                return Conditional::Pointer();
            }
            children.push_back(childConditional);
        }
        return std::make_shared<AndConditional>(children);
    } else if (value.isString()) {
        // Support "when" : "GamePad.RB"
        auto conditionalToken = value.toString();

        // Detect for modifier case (Not...)
        QString conditionalModifier;
        const QString JSON_CONDITIONAL_MODIFIER_NOT("!");
        if (conditionalToken.startsWith(JSON_CONDITIONAL_MODIFIER_NOT)) {
            conditionalModifier = JSON_CONDITIONAL_MODIFIER_NOT;
            conditionalToken = conditionalToken.right(conditionalToken.size() - conditionalModifier.size());
        }

        auto input = findDeviceInput(conditionalToken);
        auto endpoint = endpointFor(input);
        if (!endpoint) {
            return Conditional::Pointer();
        }
        auto conditional = std::make_shared<EndpointConditional>(endpoint);

        if (!conditionalModifier.isEmpty()) {
            if (conditionalModifier == JSON_CONDITIONAL_MODIFIER_NOT) {
                return std::make_shared<NotConditional>(conditional);
            }
        }

        // Default and conditional behavior
        return conditional;
    }

    return Conditional::parse(value);
}

Filter::Pointer UserInputMapper::parseFilter(const QJsonValue& value) {
    Filter::Pointer result;
    if (value.isString()) {
        result = Filter::getFactory().create(value.toString());
    } else if (value.isObject()) {
        result = Filter::parse(value.toObject());
    }

    if (!result) {
        qWarning() << "Invalid filter definition " << value;
    }

    return result;
}

Filter::List UserInputMapper::parseFilters(const QJsonValue& value) {
    if (value.isNull()) {
        return Filter::List();
    }

    if (value.isArray()) {
        Filter::List result;
        auto filtersArray = value.toArray();
        for (const auto& filterValue : filtersArray) {
            Filter::Pointer filter = parseFilter(filterValue);
            if (!filter) {
                return Filter::List();
            }
            result.push_back(filter);
        }
        return result;
    }

    Filter::Pointer filter = parseFilter(value);
    if (!filter) {
        return Filter::List();
    }
    return Filter::List({ filter });
}

Endpoint::Pointer UserInputMapper::parseDestination(const QJsonValue& value) {
    if (value.isArray()) {
        ArrayEndpoint::Pointer result = std::make_shared<ArrayEndpoint>();
        auto array = value.toArray();
        for (const auto& arrayItem : array) {
            Endpoint::Pointer destination = parseEndpoint(arrayItem);
            if (!destination) {
                return Endpoint::Pointer();
            }
            result->_children.push_back(destination);
        }
        return result;
    }

    return parseEndpoint(value);
}

Endpoint::Pointer UserInputMapper::parseAxis(const QJsonValue& value) {
    if (value.isObject()) {
        auto object = value.toObject();
        if (object.contains("makeAxis")) {
            auto axisValue = object.value("makeAxis");
            if (axisValue.isArray()) {
                auto axisArray = axisValue.toArray();
                static const int AXIS_ARRAY_SIZE = 2; // axis can only have 2 children
                if (axisArray.size() == AXIS_ARRAY_SIZE) {
                    Endpoint::Pointer first = parseEndpoint(axisArray.first());
                    Endpoint::Pointer second = parseEndpoint(axisArray.last());
                    if (first && second) {
                        return std::make_shared<CompositeEndpoint>(first, second);
                    }
                }
            }
        }
    }
    return Endpoint::Pointer();
}

Endpoint::Pointer UserInputMapper::parseAny(const QJsonValue& value) {
    if (value.isArray()) {
        Endpoint::List children;
        auto array = value.toArray();
        for (const auto& arrayItem : array) {
            Endpoint::Pointer destination = parseEndpoint(arrayItem);
            if (!destination) {
                return Endpoint::Pointer();
            }
            children.push_back(destination);
        }
        return std::make_shared<AnyEndpoint>(children);
    }
    return Endpoint::Pointer();
}

Endpoint::Pointer UserInputMapper::parseSource(const QJsonValue& value) {
    if (value.isObject()) {
        auto axisEndpoint = parseAxis(value);
        if (axisEndpoint) {
            return axisEndpoint;
        }
        // if we have other types of endpoints that are objects, follow the axisEndpoint example, and place them here
    } else if (value.isArray()) {
        return parseAny(value);
    }
    return parseEndpoint(value);
}

Route::Pointer UserInputMapper::parseRoute(const QJsonValue& value) {
    if (!value.isObject()) {
        return Route::Pointer();
    }

    auto obj = value.toObject();
    Route::Pointer result = std::make_shared<Route>();

    result->json = QString(QJsonDocument(obj).toJson());
    result->source = parseSource(obj[JSON_CHANNEL_FROM]);
    result->debug = obj[JSON_CHANNEL_DEBUG].toBool();
    result->peek = obj[JSON_CHANNEL_PEEK].toBool();
    if (!result->source) {
        qWarning() << "Invalid route source " << obj[JSON_CHANNEL_FROM];
        return Route::Pointer();
    }


    result->destination = parseDestination(obj[JSON_CHANNEL_TO]);
    if (!result->destination) {
        qWarning() << "Invalid route destination " << obj[JSON_CHANNEL_TO];
        return Route::Pointer();
    }

    if (result->source == result->destination) {
        qWarning() << "Loopback routes not supported " << obj;
        return Route::Pointer();
    }

    if (obj.contains(JSON_CHANNEL_WHEN)) {
        auto conditionalsValue = obj[JSON_CHANNEL_WHEN];
        result->conditional = parseConditional(conditionalsValue);
        if (!result->conditional) {
            qWarning() << "Invalid route conditionals " << conditionalsValue;
            return Route::Pointer();
        }
    }

    if (obj.contains(JSON_CHANNEL_FILTERS)) {
        auto filtersValue = obj[JSON_CHANNEL_FILTERS];
        result->filters = parseFilters(filtersValue);
        if (result->filters.empty()) {
            qWarning() << "Invalid route filters " << filtersValue;
            return Route::Pointer();
        }
    }

    return result;
}

void injectConditional(Route::Pointer& route, Conditional::Pointer& conditional) {
    if (!conditional) {
        return;
    }

    if (!route->conditional) {
        route->conditional = conditional;
        return;
    }

    route->conditional = std::make_shared<AndConditional>(conditional, route->conditional);
}


Mapping::Pointer UserInputMapper::parseMapping(const QJsonValue& json) {
    if (!json.isObject()) {
        return Mapping::Pointer();
    }

    auto obj = json.toObject();
    auto mapping = std::make_shared<Mapping>("default");
    mapping->name = obj[JSON_NAME].toString();
    const auto& jsonChannels = obj[JSON_CHANNELS].toArray();
    Conditional::Pointer globalConditional;
    if (obj.contains(JSON_CHANNEL_WHEN)) {
        auto conditionalsValue = obj[JSON_CHANNEL_WHEN];
        globalConditional = parseConditional(conditionalsValue);
    }

    for (const auto& channelIt : jsonChannels) {
        Route::Pointer route = parseRoute(channelIt);

        if (!route) {
            qWarning() << "Couldn't parse route:" << mapping->name << QString(QJsonDocument(channelIt.toObject()).toJson());
            continue;
        }

        if (globalConditional) {
            injectConditional(route, globalConditional);
        }

        mapping->routes.push_back(route);
    }
    _mappingsByName[mapping->name] = mapping;
    return mapping;
}

Mapping::Pointer UserInputMapper::parseMapping(const QString& json) {
    Mapping::Pointer result;
    QJsonObject obj;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    // check validity of the document
    if (doc.isNull()) {
        qCDebug(controllers) << "Invalid JSON...\n";
        qCDebug(controllers) << error.errorString();
        qCDebug(controllers) << "JSON was:\n" << json << endl;
        return Mapping::Pointer();
    }

    if (!doc.isObject()) {
        qWarning() << "Mapping json Document is not an object" << endl;
        qCDebug(controllers) << "JSON was:\n" << json << endl;
        return Mapping::Pointer();
    }
    return parseMapping(doc.object());
}

template <typename T>
bool hasDebuggableRoute(const T& routes) {
    for (const auto& route : routes) {
        if (route->debug) {
            return true;
        }
    }
    return false;
}


void UserInputMapper::enableMapping(const Mapping::Pointer& mapping) {
    Locker locker(_lock);
    // New routes for a device get injected IN FRONT of existing routes.  Routes
    // are processed in order so this ensures that the standard -> action processing
    // takes place after all of the hardware -> standard or hardware -> action processing
    // because standard -> action is the first set of routes added.
    Route::List standardRoutes = mapping->routes;
    standardRoutes.remove_if([](const Route::Pointer& value) {
        return (value->source->getInput().device != STANDARD_DEVICE);
    });
    _standardRoutes.insert(_standardRoutes.begin(), standardRoutes.begin(), standardRoutes.end());

    Route::List deviceRoutes = mapping->routes;
    deviceRoutes.remove_if([](const Route::Pointer& value) {
        return (value->source->getInput().device == STANDARD_DEVICE);
    });
    _deviceRoutes.insert(_deviceRoutes.begin(), deviceRoutes.begin(), deviceRoutes.end());

    if (!debuggableRoutes) {
        debuggableRoutes = hasDebuggableRoute(_deviceRoutes) || hasDebuggableRoute(_standardRoutes);
    }
}

void UserInputMapper::disableMapping(const Mapping::Pointer& mapping) {
    Locker locker(_lock);
    const auto& deviceRoutes = mapping->routes;
    std::set<Route::Pointer> routeSet(deviceRoutes.begin(), deviceRoutes.end());
    _deviceRoutes.remove_if([&](const Route::Pointer& value){
        return routeSet.count(value) != 0;
    });
    _standardRoutes.remove_if([&](const Route::Pointer& value) {
        return routeSet.count(value) != 0;
    });

    if (debuggableRoutes) {
        debuggableRoutes = hasDebuggableRoute(_deviceRoutes) || hasDebuggableRoute(_standardRoutes);
    }
}

void UserInputMapper::setActionState(Action action, float value, bool valid) {
    Locker locker(_lock);
    _actionStates[toInt(action)] = value;
    _actionStatesValid[toInt(action)] = valid;
}

void UserInputMapper::deltaActionState(Action action, float delta, bool valid) {
    Locker locker(_lock);
    _actionStates[toInt(action)] += delta;
    bool wasValid = _actionStatesValid[toInt(action)];
    _actionStatesValid[toInt(action)] = wasValid & valid;
}

float UserInputMapper::getActionState(Action action) const {
    Locker locker(_lock);

    int index = toInt(action);
    if (index >= 0 && index < _actionStates.size()) {
        return _actionStates[index];
    }

    qCDebug(controllers) << "UserInputMapper::getActionState invalid action:" << index;
    return 0.0f;
}

bool UserInputMapper::getActionStateValid(Action action) const {
    Locker locker(_lock);

    int index = toInt(action);
    if (index >= 0 && index < _actionStatesValid.size()) {
        return _actionStatesValid[index];
    }

    qCDebug(controllers) << "UserInputMapper::getActionStateValid invalid action:" << index;
    return false;
}


}

