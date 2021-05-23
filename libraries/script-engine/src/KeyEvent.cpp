//
//  KeyEvent.cpp
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "KeyEvent.h"

#include <qdebug.h>

#include "ScriptEngineLogging.h"
#include "ScriptEngine.h"
#include "ScriptValue.h"

KeyEvent::KeyEvent() :
    key(0),
    text(""),
    isShifted(false),
    isControl(false),
    isMeta(false),
    isAlt(false),
    isKeypad(false),
    isValid(false),
    isAutoRepeat(false)
{
    
}

KeyEvent::KeyEvent(const QKeyEvent& event) {
    key = event.key();
    text = event.text();
    isShifted = event.modifiers().testFlag(Qt::ShiftModifier);
    isMeta = event.modifiers().testFlag(Qt::MetaModifier);
    isControl = event.modifiers().testFlag(Qt::ControlModifier);
    isAlt = event.modifiers().testFlag(Qt::AltModifier);
    isKeypad = event.modifiers().testFlag(Qt::KeypadModifier);
    isValid = true;
    isAutoRepeat = event.isAutoRepeat();
    
    // handle special text for special characters...
    if (key == Qt::Key_F1) {
        text = "F1";
    } else if (key == Qt::Key_F2) {
        text = "F2";
    } else if (key == Qt::Key_F3) {
        text = "F3";
    } else if (key == Qt::Key_F4) {
        text = "F4";
    } else if (key == Qt::Key_F5) {
        text = "F5";
    } else if (key == Qt::Key_F6) {
        text = "F6";
    } else if (key == Qt::Key_F7) {
        text = "F7";
    } else if (key == Qt::Key_F8) {
        text = "F8";
    } else if (key == Qt::Key_F9) {
        text = "F9";
    } else if (key == Qt::Key_F10) {
        text = "F10";
    } else if (key == Qt::Key_F11) {
        text = "F11";
    } else if (key == Qt::Key_F12) {
        text = "F12";
    } else if (key == Qt::Key_Up) {
        text = "UP";
    } else if (key == Qt::Key_Down) {
        text = "DOWN";
    } else if (key == Qt::Key_Left) {
        text = "LEFT";
    } else if (key == Qt::Key_Right) {
        text = "RIGHT";
    } else if (key == Qt::Key_Space) {
        text = "SPACE";
    } else if (key == Qt::Key_Escape) {
        text = "ESC";
    } else if (key == Qt::Key_Tab) {
        text = "TAB";
    } else if (key == Qt::Key_Delete) {
        text = "DELETE";
    } else if (key == Qt::Key_Backspace) {
        text = "BACKSPACE";
    } else if (key == Qt::Key_Shift) {
        text = "SHIFT";
    } else if (key == Qt::Key_Alt) {
        text = "ALT";
    } else if (key == Qt::Key_Control) {
        text = "CONTROL";
    } else if (key == Qt::Key_Meta) {
        text = "META";
    } else if (key == Qt::Key_PageDown) {
        text = "PAGE DOWN";
    } else if (key == Qt::Key_PageUp) {
        text = "PAGE UP";
    } else if (key == Qt::Key_Home) {
        text = "HOME";
    } else if (key == Qt::Key_End) {
        text = "END";
    } else if (key == Qt::Key_Help) {
        text = "HELP";
    } else if (key == Qt::Key_CapsLock) {
        text = "CAPS LOCK";
    } else if (key >= Qt::Key_A && key <= Qt::Key_Z && (isMeta || isControl || isAlt))  {
        // this little bit of hackery will fix the text character keys like a-z in cases of control/alt/meta where
        // qt doesn't always give you the key characters and will sometimes give you crazy non-printable characters
        const int lowerCaseAdjust = 0x20;
        QString unicode;
        if (isShifted) {
            text = QString(QChar(key));
        } else {
            text = QString(QChar(key + lowerCaseAdjust));
        }
    }
}

bool KeyEvent::operator==(const KeyEvent& other) const {
    return other.key == key
    && other.isShifted == isShifted
    && other.isControl == isControl
    && other.isMeta == isMeta
    && other.isAlt == isAlt
    && other.isKeypad == isKeypad;
}


KeyEvent::operator QKeySequence() const {
    int resultCode = 0;
    if (text.size() == 1 && text >= "a" && text <= "z") {
        resultCode = text.toUpper().at(0).unicode();
    } else {
        resultCode = key;
    }
    
    if (isMeta) {
        resultCode |= Qt::META;
    }
    if (isAlt) {
        resultCode |= Qt::ALT;
    }
    if (isControl) {
        resultCode |= Qt::CTRL;
    }
    if (isShifted) {
        resultCode |= Qt::SHIFT;
    }
    return QKeySequence(resultCode);
}

/*@jsdoc
 * A keyboard key event.
 * @typedef {object} KeyEvent
 * @property {number} key - The Qt keyboard code of the key pressed. For a list of keyboard codes, see 
 *     <a href="http://doc.qt.io/qt-5/qt.html#Key-enum">http://doc.qt.io/qt-5/qt.html#Key-enum</a>.
 * @property {string} text - A string describing the key. For example, <code>"a"</code> for the "A" key if the Shift is not 
 *     pressed, <code>"F1"</code> for the F1 key, <code>"SPACE"</code> for the space bar.
 * @property {boolean} isShifted - <code>true</code> if a Shift key was pressed when the event was generated, otherwise 
 *     <code>false</code>.
 * @property {boolean} isMeta - <code>true</code> if a meta key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "meta" key is the Windows key; on OSX it is the Control (Splat) key. 
 * @property {boolean} isControl - <code>true</code> if a control key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "control" key is the Ctrl key; on OSX it is the Command key.
 * @property {boolean} isAlt - <code>true</code> if an Alt key was pressed when the event was generated, otherwise 
 *     <code>false</code>.
 * @property {boolean} isKeypad - <code>true</code> if the key is on the numeric keypad, otherwise <code>false</code>.
 * @property {boolean} isAutoRepeat - <code>true</code> if the event is a repeat for key that is being held down, otherwise 
 *     <code>false</code>.
 * @example <caption>Report the KeyEvent details for each key press.</caption>
 * Controller.keyPressEvent.connect(function (event) {
 *     print(JSON.stringify(event));
 * });
 */
ScriptValuePointer KeyEvent::toScriptValue(ScriptEngine* engine, const KeyEvent& event) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("key", event.key);
    obj->setProperty("text", event.text);
    obj->setProperty("isShifted", event.isShifted);
    obj->setProperty("isMeta", event.isMeta);
    obj->setProperty("isControl", event.isControl);
    obj->setProperty("isAlt", event.isAlt);
    obj->setProperty("isKeypad", event.isKeypad);
    obj->setProperty("isAutoRepeat", event.isAutoRepeat);
    return obj;
}

void KeyEvent::fromScriptValue(const ScriptValuePointer& object, KeyEvent& event) {
    
    event.isValid = false; // assume the worst
    event.isMeta = object->property("isMeta")->toVariant().toBool();
    event.isControl = object->property("isControl")->toVariant().toBool();
    event.isAlt = object->property("isAlt")->toVariant().toBool();
    event.isKeypad = object->property("isKeypad")->toVariant().toBool();
    event.isAutoRepeat = object->property("isAutoRepeat")->toVariant().toBool();
    
    ScriptValuePointer key = object->property("key");
    if (key->isValid()) {
        event.key = key->toVariant().toInt();
        event.text = QString(QChar(event.key));
        event.isValid = true;
    } else {
        ScriptValuePointer text = object->property("text");
        if (text->isValid()) {
            event.text = object->property("text")->toVariant().toString();
            
            // if the text is a special command, then map it here...
            // TODO: come up with more elegant solution here, a map? is there a Qt function that gives nice names for keys?
            if (event.text.toUpper() == "F1") {
                event.key = Qt::Key_F1;
            } else if (event.text.toUpper() == "F2") {
                event.key = Qt::Key_F2;
            } else if (event.text.toUpper() == "F3") {
                event.key = Qt::Key_F3;
            } else if (event.text.toUpper() == "F4") {
                event.key = Qt::Key_F4;
            } else if (event.text.toUpper() == "F5") {
                event.key = Qt::Key_F5;
            } else if (event.text.toUpper() == "F6") {
                event.key = Qt::Key_F6;
            } else if (event.text.toUpper() == "F7") {
                event.key = Qt::Key_F7;
            } else if (event.text.toUpper() == "F8") {
                event.key = Qt::Key_F8;
            } else if (event.text.toUpper() == "F9") {
                event.key = Qt::Key_F9;
            } else if (event.text.toUpper() == "F10") {
                event.key = Qt::Key_F10;
            } else if (event.text.toUpper() == "F11") {
                event.key = Qt::Key_F11;
            } else if (event.text.toUpper() == "F12") {
                event.key = Qt::Key_F12;
            } else if (event.text.toUpper() == "UP") {
                event.key = Qt::Key_Up;
                event.isKeypad = true;
            } else if (event.text.toUpper() == "DOWN") {
                event.key = Qt::Key_Down;
                event.isKeypad = true;
            } else if (event.text.toUpper() == "LEFT") {
                event.key = Qt::Key_Left;
                event.isKeypad = true;
            } else if (event.text.toUpper() == "RIGHT") {
                event.key = Qt::Key_Right;
                event.isKeypad = true;
            } else if (event.text.toUpper() == "SPACE") {
                event.key = Qt::Key_Space;
            } else if (event.text.toUpper() == "ESC") {
                event.key = Qt::Key_Escape;
            } else if (event.text.toUpper() == "TAB") {
                event.key = Qt::Key_Tab;
            } else if (event.text.toUpper() == "DELETE") {
                event.key = Qt::Key_Delete;
            } else if (event.text.toUpper() == "BACKSPACE") {
                event.key = Qt::Key_Backspace;
            } else if (event.text.toUpper() == "SHIFT") {
                event.key = Qt::Key_Shift;
            } else if (event.text.toUpper() == "ALT") {
                event.key = Qt::Key_Alt;
            } else if (event.text.toUpper() == "CONTROL") {
                event.key = Qt::Key_Control;
            } else if (event.text.toUpper() == "META") {
                event.key = Qt::Key_Meta;
            } else if (event.text.toUpper() == "PAGE DOWN") {
                event.key = Qt::Key_PageDown;
            } else if (event.text.toUpper() == "PAGE UP") {
                event.key = Qt::Key_PageUp;
            } else if (event.text.toUpper() == "HOME") {
                event.key = Qt::Key_Home;
            } else if (event.text.toUpper() == "END") {
                event.key = Qt::Key_End;
            } else if (event.text.toUpper() == "HELP") {
                event.key = Qt::Key_Help;
            } else if (event.text.toUpper() == "CAPS LOCK") {
                event.key = Qt::Key_CapsLock;
            } else {
                // Key values do not distinguish between uppercase and lowercase
                // and use the uppercase key value.
                event.key = event.text.toUpper().at(0).unicode();
            }
            event.isValid = true;
        }
    }
    
    ScriptValuePointer isShifted = object->property("isShifted");
    if (isShifted->isValid()) {
        event.isShifted = isShifted->toVariant().toBool();
    } else {
        // if no isShifted was included, get it from the text
        QChar character = event.text.at(0);
        if (character.isLetter() && character.isUpper()) {
            event.isShifted = true;
        } else {
            // if it's a symbol, then attempt to detect shifted-ness
            if (QString("~!@#$%^&*()_+{}|:\"<>?").contains(character)) {
                event.isShifted = true;
            }
        }
    }
    
    const bool wantDebug = false;
    if (wantDebug) {
        qCDebug(scriptengine) << "event.key=" << event.key
        << " event.text=" << event.text
        << " event.isShifted=" << event.isShifted
        << " event.isControl=" << event.isControl
        << " event.isMeta=" << event.isMeta
        << " event.isAlt=" << event.isAlt
        << " event.isKeypad=" << event.isKeypad
        << " event.isAutoRepeat=" << event.isAutoRepeat;
    }
}

