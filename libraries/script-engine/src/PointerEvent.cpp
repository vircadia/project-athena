//
//  PointerEvent.cpp
//  script-engine/src
//
//  Created by Anthony Thibault on 2016-8-11.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PointerEvent.h"

#include "RegisteredMetaTypes.h"
#include "ScriptEngine.h"
#include "ScriptValue.h"
#include "ScriptValueUtils.h"

static bool areFlagsSet(uint32_t flags, uint32_t mask) {
    return (flags & mask) != 0;
}

PointerEvent::PointerEvent(EventType type, uint32_t id) :
    _type(type),
    _id(id)
{
}

PointerEvent::PointerEvent(EventType type, uint32_t id, const glm::vec2& pos2D, Button button, uint32_t buttons, Qt::KeyboardModifiers keyboardModifiers) :
    _type(type),
    _id(id),
    _pos2D(pos2D),
    _button(button),
    _buttons(buttons),
    _keyboardModifiers(keyboardModifiers)
{
}

PointerEvent::PointerEvent(const glm::vec2& pos2D, const glm::vec3& pos3D, const glm::vec3& normal, const glm::vec3& direction) :
    _pos2D(pos2D),
    _pos3D(pos3D),
    _normal(normal),
    _direction(direction)
{
}

PointerEvent::PointerEvent(EventType type, uint32_t id,
                           const glm::vec2& pos2D, const glm::vec3& pos3D,
                           const glm::vec3& normal, const glm::vec3& direction,
                           Button button, uint32_t buttons, Qt::KeyboardModifiers keyboardModifiers) :
    _type(type),
    _id(id),
    _pos2D(pos2D),
    _pos3D(pos3D),
    _normal(normal),
    _direction(direction),
    _button(button),
    _buttons(buttons),
    _keyboardModifiers(keyboardModifiers)
{
}

void PointerEvent::setButton(Button button) {
    _button = button;
    _buttons |= button;
}

/*@jsdoc
 * A 2D or 3D mouse or similar pointer event.
 * @typedef {object} PointerEvent
 * @property {string} type - The type of event: <code>"Press"</code>, <code>"DoublePress"</code>, <code>"Release"</code>, or
 *     <code>"Move"</code>.
 * @property {number} id - Integer number used to identify the pointer: <code>0</code> = hardware mouse, <code>1</code> = left
 *     controller, <code>2</code> = right controller.
 * @property {Vec2} pos2D - The 2D position of the event on the intersected object XY plane, where applicable.
 * @property {Vec3} pos3D - The 3D position of the event on the intersected object, where applicable.
 * @property {Vec3} normal - The surface normal at the intersection point.
 * @property {Vec3} direction - The direction of the intersection ray.
 * @property {string} button - The name of the button pressed: <code>None</code>, <code>Primary</code>, <code>Secondary</code>,
 *    or <code>Tertiary</code>.
 * @property {boolean} isPrimaryButton - <code>true</code> if the button pressed was the primary button, otherwise 
 *     <code>undefined</code>;
 * @property {boolean} isLeftButton - <code>true</code> if the button pressed was the primary button, otherwise
 *     <code>undefined</code>;
 * @property {boolean} isSecondaryButton - <code>true</code> if the button pressed was the secondary button, otherwise
 *     <code>undefined</code>;
 * @property {boolean} isRightButton - <code>true</code> if the button pressed was the secondary button, otherwise
 *     <code>undefined</code>;
 * @property {boolean} isTertiaryButton - <code>true</code> if the button pressed was the tertiary button, otherwise
 *     <code>undefined</code>;
 * @property {boolean} isMiddleButton - <code>true</code> if the button pressed was the tertiary button, otherwise
 *     <code>undefined</code>;
 * @property {boolean} isPrimaryHeld - <code>true</code> if the primary button is currently being pressed, otherwise
 *     <code>false</code>
 * @property {boolean} isSecondaryHeld - <code>true</code> if the secondary button is currently being pressed, otherwise
 *     <code>false</code>
 * @property {boolean} isTertiaryHeld - <code>true</code> if the tertiary button is currently being pressed, otherwise
 *     <code>false</code>
 * @property {KeyboardModifiers} keyboardModifiers - Integer value with bits set according to which keyboard modifier keys were
 *     pressed when the event was generated.
 */
/*@jsdoc
 * <p>A KeyboardModifiers value is used to specify which modifier keys on the keyboard are pressed. The value is the sum 
 * (bitwise OR) of the relevant combination of values from the following table:</p>
 * <table>
 *   <thead>
 *     <tr><th>Key</th><th>Hexadecimal value</th><th>Decimal value</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td>Shift</td><td><code>0x02000000</code></td><td><code>33554432</code></td>
 *         <td>A Shift key on the keyboard is pressed.</td></tr>
 *     <tr><td>Control</td><td><code>0x04000000</code></td><td><code>67108864</code></td>
 *         <td>A control key on the keyboard is pressed. On Windows the "control" key is the Ctrl key; on OSX it is the Command 
 *         key.</td></tr>
 *     <tr><td>Alt</td><td><code>0x08000000</code></td><td><code>134217728</code></td>
 *         <td>An Alt key on the keyboard is pressed.</td></tr>
 *     <tr><td>Meta</td><td><code>0x10000000</code></td><td><code>268435456</code></td>
 *         <td>A meta key on the keyboard is pressed. On Windows the "meta" key is the Windows key; on OSX it is the Control 
 *         (Splat) key.</td></tr>
 *     <tr><td>Keypad</td><td><code>0x20000000</code></td><td><code>536870912</code></td>
 *         <td>A keypad button is pressed.</td></tr>
 *     <tr><td>Group</td><td><code>0x40000000</code></td><td><code>1073741824</code></td>
 *         <td>X11 operating system only: An AltGr / Mode_switch key on the keyboard is pressed.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {number} KeyboardModifiers
 */
ScriptValuePointer PointerEvent::toScriptValue(ScriptEngine* engine, const PointerEvent& event) {
    ScriptValuePointer obj = engine->newObject();

    switch (event._type) {
    case Press:
        obj->setProperty("type", "Press");
        break;
    case DoublePress:
        obj->setProperty("type", "DoublePress");
        break;
    case Release:
        obj->setProperty("type", "Release");
        break;
    default:
    case Move:
        obj->setProperty("type", "Move");
        break;
    };

    obj->setProperty("id", event._id);

    ScriptValuePointer pos2D = engine->newObject();
    pos2D->setProperty("x", event._pos2D.x);
    pos2D->setProperty("y", event._pos2D.y);
    obj->setProperty("pos2D", pos2D);

    ScriptValuePointer pos3D = engine->newObject();
    pos3D->setProperty("x", event._pos3D.x);
    pos3D->setProperty("y", event._pos3D.y);
    pos3D->setProperty("z", event._pos3D.z);
    obj->setProperty("pos3D", pos3D);

    ScriptValuePointer normal = engine->newObject();
    normal->setProperty("x", event._normal.x);
    normal->setProperty("y", event._normal.y);
    normal->setProperty("z", event._normal.z);
    obj->setProperty("normal", normal);

    ScriptValuePointer direction = engine->newObject();
    direction->setProperty("x", event._direction.x);
    direction->setProperty("y", event._direction.y);
    direction->setProperty("z", event._direction.z);
    obj->setProperty("direction", direction);

    bool isPrimaryButton = false;
    bool isSecondaryButton = false;
    bool isTertiaryButton = false;
    switch (event._button) {
    case NoButtons:
        obj->setProperty("button", "None");
        break;
    case PrimaryButton:
        obj->setProperty("button", "Primary");
        isPrimaryButton = true;
        break;
    case SecondaryButton:
        obj->setProperty("button", "Secondary");
        isSecondaryButton = true;
        break;
    case TertiaryButton:
        obj->setProperty("button", "Tertiary");
        isTertiaryButton = true;
        break;
    }

    if (isPrimaryButton) {
        obj->setProperty("isPrimaryButton", isPrimaryButton);
        obj->setProperty("isLeftButton", isPrimaryButton);
    }
    if (isSecondaryButton) {
        obj->setProperty("isSecondaryButton", isSecondaryButton);
        obj->setProperty("isRightButton", isSecondaryButton);
    }
    if (isTertiaryButton) {
        obj->setProperty("isTertiaryButton", isTertiaryButton);
        obj->setProperty("isMiddleButton", isTertiaryButton);
    }

    obj->setProperty("isPrimaryHeld", areFlagsSet(event._buttons, PrimaryButton));
    obj->setProperty("isSecondaryHeld", areFlagsSet(event._buttons, SecondaryButton));
    obj->setProperty("isTertiaryHeld", areFlagsSet(event._buttons, TertiaryButton));

    obj->setProperty("keyboardModifiers", engine->newValue(event.getKeyboardModifiers()));

    return obj;
}

void PointerEvent::fromScriptValue(const ScriptValuePointer& object, PointerEvent& event) {
    if (object->isObject()) {
        ScriptValuePointer type = object->property("type");
        QString typeStr = type->isString() ? type->toString() : "Move";
        if (typeStr == "Press") {
            event._type = Press;
        } else if (typeStr == "DoublePress") {
            event._type = DoublePress;
        } else if (typeStr == "Release") {
            event._type = Release;
        } else {
            event._type = Move;
        }

        ScriptValuePointer id = object->property("id");
        event._id = id->isNumber() ? (uint32_t)id->toNumber() : 0;

        vec2FromScriptValue(object->property("pos2D"), event._pos2D);
        vec3FromScriptValue(object->property("pos3D"), event._pos3D);
        vec3FromScriptValue(object->property("normal"), event._normal);
        vec3FromScriptValue(object->property("direction"), event._direction);

        ScriptValuePointer button = object->property("button");
        QString buttonStr = type->isString() ? button->toString() : "NoButtons";

        if (buttonStr == "Primary") {
            event._button = PrimaryButton;
        } else if (buttonStr == "Secondary") {
            event._button = SecondaryButton;
        } else if (buttonStr == "Tertiary") {
            event._button = TertiaryButton;
        } else {
            event._button = NoButtons;
        }

        bool primary = object->property("isPrimaryHeld")->toBool();
        bool secondary = object->property("isSecondaryHeld")->toBool();
        bool tertiary = object->property("isTertiaryHeld")->toBool();
        event._buttons = 0;
        if (primary) {
            event._buttons |= PrimaryButton;
        }
        if (secondary) {
            event._buttons |= SecondaryButton;
        }
        if (tertiary) {
            event._buttons |= TertiaryButton;
        }

        event._keyboardModifiers = (Qt::KeyboardModifiers)(object->property("keyboardModifiers")->toUInt32());
    }
}

static const char* typeToStringMap[PointerEvent::NumEventTypes] = { "Press", "DoublePress", "Release", "Move" };
static const char* buttonsToStringMap[8] = {
    "NoButtons",
    "PrimaryButton",
    "SecondaryButton",
    "PrimaryButton | SecondaryButton",
    "TertiaryButton",
    "PrimaryButton | TertiaryButton",
    "SecondaryButton | TertiaryButton",
    "PrimaryButton | SecondaryButton | TertiaryButton",
};

QDebug& operator<<(QDebug& dbg, const PointerEvent& p) {
    dbg.nospace() << "PointerEvent, type = " << typeToStringMap[p.getType()] << ", id = " << p.getID();
    dbg.nospace() << ", pos2D = (" << p.getPos2D().x << ", " << p.getPos2D().y;
    dbg.nospace() << "), pos3D = (" << p.getPos3D().x << ", " << p.getPos3D().y << ", " << p.getPos3D().z;
    dbg.nospace() << "), normal = (" << p.getNormal().x << ", " << p.getNormal().y << ", " << p.getNormal().z;
    dbg.nospace() << "), dir = (" << p.getDirection().x << ", " << p.getDirection().y << ", " << p.getDirection().z;
    dbg.nospace() << "), button = " << buttonsToStringMap[p.getButton()] << " " << (int)p.getButton();
    dbg.nospace() << ", buttons = " << buttonsToStringMap[p.getButtons()];
    return dbg;
}
