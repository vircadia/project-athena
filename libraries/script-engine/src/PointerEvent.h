//
//  PointerEvent.h
//  script-engine/src
//
//  Created by Anthony Thibault on 2016-8-11.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PointerEvent_h
#define hifi_PointerEvent_h

#include <Qt>

#include <stdint.h>
#include <glm/glm.hpp>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class PointerEvent {
public:
    enum Button {
        NoButtons = 0x0,
        PrimaryButton = 0x1,
        SecondaryButton = 0x2,
        TertiaryButton = 0x4
    };

    enum EventType {
        Press,       // A button has just been pressed
        DoublePress, // A button has just been double pressed
        Release,     // A button has just been released
        Move,         // The pointer has just moved
        NumEventTypes
    };

    PointerEvent() {}
    PointerEvent(EventType type, uint32_t id);
    PointerEvent(EventType type, uint32_t id, const glm::vec2& pos2D, Button button, uint32_t buttons, Qt::KeyboardModifiers keyboardModifiers);
    PointerEvent(const glm::vec2& pos2D, const glm::vec3& pos3D, const glm::vec3& normal, const glm::vec3& direction);
    PointerEvent(EventType type, uint32_t id,
                 const glm::vec2& pos2D, const glm::vec3& pos3D,
                 const glm::vec3& normal, const glm::vec3& direction,
                 Button button = NoButtons, uint32_t buttons = NoButtons, Qt::KeyboardModifiers keyboardModifiers = Qt::NoModifier);

    static ScriptValuePointer toScriptValue(ScriptEngine* engine, const PointerEvent& event);
    static void fromScriptValue(const ScriptValuePointer& object, PointerEvent& event);

    ScriptValuePointer toScriptValue(ScriptEngine* engine) const { return PointerEvent::toScriptValue(engine, *this); }

    EventType getType() const { return _type; }
    uint32_t getID() const { return _id; }
    const glm::vec2& getPos2D() const { return _pos2D; }
    const glm::vec3& getPos3D() const { return _pos3D; }
    const glm::vec3& getNormal() const { return _normal; }
    const glm::vec3& getDirection() const { return _direction; }
    Button getButton() const { return _button; }
    uint32_t getButtons() const { return _buttons; }
    Qt::KeyboardModifiers getKeyboardModifiers() const { return _keyboardModifiers; }
    bool shouldFocus() const { return _shouldFocus; }
    bool sendMoveOnHoverLeave() const { return _moveOnHoverLeave; }

    void setID(uint32_t id) { _id = id; }
    void setType(EventType type) { _type = type; }
    void setButton(Button button);
    void setPos2D(const glm::vec2& pos2D) { _pos2D = pos2D; }
    void setShouldFocus(bool focus) { _shouldFocus = focus; }
    void setMoveOnHoverLeave(bool moveOnHoverLeave) { _moveOnHoverLeave = moveOnHoverLeave; }

    static const unsigned int INVALID_POINTER_ID { 0 };

private:
    EventType _type;
    uint32_t _id { INVALID_POINTER_ID };     // used to identify the pointer.  (left vs right hand, for example)
    glm::vec2 _pos2D { glm::vec2(NAN) };     // (in meters) projected onto the xy plane of entities dimension box, (0, 0) is upper right hand corner
    glm::vec3 _pos3D { glm::vec3(NAN) };     // surface location in world coordinates (in meters)
    glm::vec3 _normal { glm::vec3(NAN) };    // surface normal
    glm::vec3 _direction { glm::vec3(NAN) }; // incoming direction of pointer ray.

    Button _button { NoButtons };  // button associated with this event, (if type is Press, this will be the button that is pressed)
    uint32_t _buttons { NoButtons }; // the current state of all the buttons.
    Qt::KeyboardModifiers _keyboardModifiers { Qt::KeyboardModifier::NoModifier }; // set of keys held when event was generated

    bool _shouldFocus { true };
    bool _moveOnHoverLeave { true };
};

QDebug& operator<<(QDebug& dbg, const PointerEvent& p);

Q_DECLARE_METATYPE(PointerEvent)

#endif // hifi_PointerEvent_h
