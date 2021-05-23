//
//  ScriptValueUtils.h
//  libraries/shared/src
//
//  Created by Anthony Thibault on 4/15/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Utilities for working with QtScriptValues
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptValueUtils_h
#define hifi_ScriptValueUtils_h

#include <QtCore/QVector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ScriptValue.h"

bool isListOfStrings(const ScriptValuePointer& value);


void registerMetaTypes(ScriptEngine* engine);

// Mat4
/*@jsdoc
 * A 4 x 4 matrix, typically containing a scale, rotation, and translation transform. See also the {@link Mat4(0)|Mat4} object.
 *
 * @typedef {object} Mat4
 * @property {number} r0c0 - Row 0, column 0 value.
 * @property {number} r1c0 - Row 1, column 0 value.
 * @property {number} r2c0 - Row 2, column 0 value.
 * @property {number} r3c0 - Row 3, column 0 value.
 * @property {number} r0c1 - Row 0, column 1 value.
 * @property {number} r1c1 - Row 1, column 1 value.
 * @property {number} r2c1 - Row 2, column 1 value.
 * @property {number} r3c1 - Row 3, column 1 value.
 * @property {number} r0c2 - Row 0, column 2 value.
 * @property {number} r1c2 - Row 1, column 2 value.
 * @property {number} r2c2 - Row 2, column 2 value.
 * @property {number} r3c2 - Row 3, column 2 value.
 * @property {number} r0c3 - Row 0, column 3 value.
 * @property {number} r1c3 - Row 1, column 3 value.
 * @property {number} r2c3 - Row 2, column 3 value.
 * @property {number} r3c3 - Row 3, column 3 value.
 */
ScriptValuePointer mat4toScriptValue(ScriptEngine* engine, const glm::mat4& mat4);
void mat4FromScriptValue(const ScriptValuePointer& object, glm::mat4& mat4);

/*@jsdoc
* A 2-dimensional vector.
*
* @typedef {object} Vec2
* @property {number} x - X-coordinate of the vector. Synonyms: <code>u</code>.
* @property {number} y - Y-coordinate of the vector. Synonyms: <code>v</code>.
* @example <caption>Vec2s can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
* Entities.editEntity(<id>, { materialMappingPos: { x: 0.1, y: 0.2 }});          // { x: 0.1, y: 0.2 }
* Entities.editEntity(<id>, { materialMappingPos: { u: 0.3, v: 0.4 }});          // { x: 0.3, y: 0.4 }
* Entities.editEntity(<id>, { materialMappingPos: [0.5, 0.6] });                 // { x: 0.5, y: 0.6 }
* Entities.editEntity(<id>, { materialMappingPos: 0.7 });                        // { x: 0.7, y: 0.7 }
* var color = Entities.getEntityProperties(<id>).materialMappingPos;             // { x: 0.7, y: 0.7 }
* color.v = 0.8;                                                                 // { x: 0.7, y: 0.8 }
*/
ScriptValuePointer vec2ToScriptValue(ScriptEngine* engine, const glm::vec2& vec2);
void vec2FromScriptValue(const ScriptValuePointer& object, glm::vec2& vec2);

/*@jsdoc
* A 3-dimensional vector. See also the {@link Vec3(0)|Vec3} object.
*
* @typedef {object} Vec3
* @property {number} x - X-coordinate of the vector. Synonyms: <code>r</code>, <code>red</code>.
* @property {number} y - Y-coordinate of the vector. Synonyms: <code>g</code>, <code>green</code>.
* @property {number} z - Z-coordinate of the vector. Synonyms: <code>b</code>, <code>blue</code>.
* @example <caption>Vec3 values can be set in multiple ways and modified with their aliases, but still stringify in the same 
*     way.</caption>
* Entities.editEntity(<id>, { position: { x: 1, y: 2, z: 3 }});                 // { x: 1, y: 2, z: 3 }
* Entities.editEntity(<id>, { position: { r: 4, g: 5, b: 6 }});                 // { x: 4, y: 5, z: 6 }
* Entities.editEntity(<id>, { position: { red: 7, green: 8, blue: 9 }});        // { x: 7, y: 8, z: 9 }
* Entities.editEntity(<id>, { position: [10, 11, 12] });                        // { x: 10, y: 11, z: 12 }
* Entities.editEntity(<id>, { position: 13 });                                  // { x: 13, y: 13, z: 13 }
* var position = Entities.getEntityProperties(<id>).position;                   // { x: 13, y: 13, z: 13 }
* position.g = 14;                                                              // { x: 13, y: 14, z: 13 }
* position.blue = 15;                                                           // { x: 13, y: 14, z: 15 }
* Entities.editEntity(<id>, { position: "red"});                                // { x: 255, y: 0, z: 0 }
* Entities.editEntity(<id>, { position: "#00FF00"});                            // { x: 0, y: 255, z: 0 }
*/
ScriptValuePointer vec3ToScriptValue(ScriptEngine* engine, const glm::vec3& vec3);
ScriptValuePointer vec3ColorToScriptValue(ScriptEngine* engine, const glm::vec3& vec3);
void vec3FromScriptValue(const ScriptValuePointer& object, glm::vec3& vec3);

/*@jsdoc
 * A color vector. See also the {@link Vec3(0)|Vec3} object.
 *
 * @typedef {object} Color
 * @property {number} red - Red component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>r</code>, <code>x</code>.
 * @property {number} green - Green component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>g</code>, <code>y</code>.
 * @property {number} blue - Blue component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>b</code>, <code>z</code>.
 * @example <caption>Colors can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
 * Entities.editEntity(<id>, { color: { x: 1, y: 2, z: 3 }});                 // { red: 1, green: 2, blue: 3 }
 * Entities.editEntity(<id>, { color: { r: 4, g: 5, b: 6 }});                 // { red: 4, green: 5, blue: 6 }
 * Entities.editEntity(<id>, { color: { red: 7, green: 8, blue: 9 }});        // { red: 7, green: 8, blue: 9 }
 * Entities.editEntity(<id>, { color: [10, 11, 12] });                        // { red: 10, green: 11, blue: 12 }
 * Entities.editEntity(<id>, { color: 13 });                                  // { red: 13, green: 13, blue: 13 }
 * var color = Entities.getEntityProperties(<id>).color;                      // { red: 13, green: 13, blue: 13 }
 * color.g = 14;                                                              // { red: 13, green: 14, blue: 13 }
 * color.blue = 15;                                                           // { red: 13, green: 14, blue: 15 }
 * Entities.editEntity(<id>, { color: "red"});                                // { red: 255, green: 0, blue: 0 }
 * Entities.editEntity(<id>, { color: "#00FF00"});                            // { red: 0, green: 255, blue: 0 }
 */
/*@jsdoc
 * A color vector with real values. Values may also be <code>null</code>. See also the {@link Vec3(0)|Vec3} object.
 *
 * @typedef {object} ColorFloat
 * @property {number} red - Red component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>r</code>, <code>x</code>.
 * @property {number} green - Green component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>g</code>, <code>y</code>.
 * @property {number} blue - Blue component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>b</code>, <code>z</code>.
 * @example <caption>ColorFloats can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
 * Entities.editEntity(<id>, { color: { x: 1, y: 2, z: 3 }});                 // { red: 1, green: 2, blue: 3 }
 * Entities.editEntity(<id>, { color: { r: 4, g: 5, b: 6 }});                 // { red: 4, green: 5, blue: 6 }
 * Entities.editEntity(<id>, { color: { red: 7, green: 8, blue: 9 }});        // { red: 7, green: 8, blue: 9 }
 * Entities.editEntity(<id>, { color: [10, 11, 12] });                        // { red: 10, green: 11, blue: 12 }
 * Entities.editEntity(<id>, { color: 13 });                                  // { red: 13, green: 13, blue: 13 }
 * var color = Entities.getEntityProperties(<id>).color;                      // { red: 13, green: 13, blue: 13 }
 * color.g = 14;                                                              // { red: 13, green: 14, blue: 13 }
 * color.blue = 15;                                                           // { red: 13, green: 14, blue: 15 }
 * Entities.editEntity(<id>, { color: "red"});                                // { red: 255, green: 0, blue: 0 }
 * Entities.editEntity(<id>, { color: "#00FF00"});                            // { red: 0, green: 255, blue: 0 }
 */
ScriptValuePointer u8vec3ToScriptValue(ScriptEngine* engine, const glm::u8vec3& vec3);
ScriptValuePointer u8vec3ColorToScriptValue(ScriptEngine* engine, const glm::u8vec3& vec3);
void u8vec3FromScriptValue(const ScriptValuePointer& object, glm::u8vec3& vec3);

/*@jsdoc
 * A 4-dimensional vector.
 *
 * @typedef {object} Vec4
 * @property {number} x - X-coordinate of the vector.
 * @property {number} y - Y-coordinate of the vector.
 * @property {number} z - Z-coordinate of the vector.
 * @property {number} w - W-coordinate of the vector.
 */
ScriptValuePointer vec4toScriptValue(ScriptEngine* engine, const glm::vec4& vec4);
void vec4FromScriptValue(const ScriptValuePointer& object, glm::vec4& vec4);

// Quaternions
ScriptValuePointer quatToScriptValue(ScriptEngine* engine, const glm::quat& quat);
void quatFromScriptValue(const ScriptValuePointer& object, glm::quat& quat);

/*@jsdoc
 * Defines a rectangular portion of an image or screen, or similar.
 * @typedef {object} Rect
 * @property {number} x - Left, x-coordinate value.
 * @property {number} y - Top, y-coordinate value.
 * @property {number} width - Width of the rectangle.
 * @property {number} height - Height of the rectangle.
 */
class QRect;
ScriptValuePointer qRectToScriptValue(ScriptEngine* engine, const QRect& rect);
void qRectFromScriptValue(const ScriptValuePointer& object, QRect& rect);

class QRectF;
ScriptValuePointer qRectFToScriptValue(ScriptEngine* engine, const QRectF& rect);
void qRectFFromScriptValue(const ScriptValuePointer& object, QRectF& rect);

// QColor
class QColor;
ScriptValuePointer qColorToScriptValue(ScriptEngine* engine, const QColor& color);
void qColorFromScriptValue(const ScriptValuePointer& object, QColor& color);

class QUrl;
ScriptValuePointer qURLToScriptValue(ScriptEngine* engine, const QUrl& url);
void qURLFromScriptValue(const ScriptValuePointer& object, QUrl& url);

// vector<vec3>
Q_DECLARE_METATYPE(QVector<glm::vec3>)
ScriptValuePointer qVectorVec3ToScriptValue(ScriptEngine* engine, const QVector<glm::vec3>& vector);
ScriptValuePointer qVectorVec3ColorToScriptValue(ScriptEngine* engine, const QVector<glm::vec3>& vector);
void qVectorVec3FromScriptValue(const ScriptValuePointer& array, QVector<glm::vec3>& vector);
QVector<glm::vec3> qVectorVec3FromScriptValue(const ScriptValuePointer& array);

// vector<quat>
Q_DECLARE_METATYPE(QVector<glm::quat>)
ScriptValuePointer qVectorQuatToScriptValue(ScriptEngine* engine, const QVector<glm::quat>& vector);
void qVectorQuatFromScriptValue(const ScriptValuePointer& array, QVector<glm::quat>& vector);
QVector<glm::quat> qVectorQuatFromScriptValue(const ScriptValuePointer& array);

// vector<bool>
ScriptValuePointer qVectorBoolToScriptValue(ScriptEngine* engine, const QVector<bool>& vector);
void qVectorBoolFromScriptValue(const ScriptValuePointer& array, QVector<bool>& vector);
QVector<bool> qVectorBoolFromScriptValue(const ScriptValuePointer& array);

// vector<float>
ScriptValuePointer qVectorFloatToScriptValue(ScriptEngine* engine, const QVector<float>& vector);
void qVectorFloatFromScriptValue(const ScriptValuePointer& array, QVector<float>& vector);
QVector<float> qVectorFloatFromScriptValue(const ScriptValuePointer& array);

// vector<uint32_t>
ScriptValuePointer qVectorIntToScriptValue(ScriptEngine* engine, const QVector<uint32_t>& vector);
void qVectorIntFromScriptValue(const ScriptValuePointer& array, QVector<uint32_t>& vector);

ScriptValuePointer qVectorQUuidToScriptValue(ScriptEngine* engine, const QVector<QUuid>& vector);
void qVectorQUuidFromScriptValue(const ScriptValuePointer& array, QVector<QUuid>& vector);
QVector<QUuid> qVectorQUuidFromScriptValue(const ScriptValuePointer& array);

class AACube;
ScriptValuePointer aaCubeToScriptValue(ScriptEngine* engine, const AACube& aaCube);
void aaCubeFromScriptValue(const ScriptValuePointer& object, AACube& aaCube);

class PickRay;
ScriptValuePointer pickRayToScriptValue(ScriptEngine* engine, const PickRay& pickRay);
void pickRayFromScriptValue(const ScriptValuePointer& object, PickRay& pickRay);

class Collision;
ScriptValuePointer collisionToScriptValue(ScriptEngine* engine, const Collision& collision);
void collisionFromScriptValue(const ScriptValuePointer& object, Collision& collision);

/*@jsdoc
 * UUIDs (Universally Unique IDentifiers) are used to uniquely identify entities, avatars, and the like. They are represented 
 * in JavaScript as strings in the format, <code>"{nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn}"</code>, where the "n"s are
 * hexadecimal digits.
 * @typedef {string} Uuid
 */
//Q_DECLARE_METATYPE(QUuid) // don't need to do this for QUuid since it's already a meta type
ScriptValuePointer quuidToScriptValue(ScriptEngine* engine, const QUuid& uuid);
void quuidFromScriptValue(const ScriptValuePointer& object, QUuid& uuid);

//Q_DECLARE_METATYPE(QSizeF) // Don't need to to this becase it's arleady a meta type
class QSizeF;
ScriptValuePointer qSizeFToScriptValue(ScriptEngine* engine, const QSizeF& qSizeF);
void qSizeFFromScriptValue(const ScriptValuePointer& object, QSizeF& qSizeF);

class AnimationDetails;
ScriptValuePointer animationDetailsToScriptValue(ScriptEngine* engine, const AnimationDetails& event);
void animationDetailsFromScriptValue(const ScriptValuePointer& object, AnimationDetails& event);

class MeshProxy;
ScriptValuePointer meshToScriptValue(ScriptEngine* engine, MeshProxy* const& in);
void meshFromScriptValue(const ScriptValuePointer& value, MeshProxy*& out);

class MeshProxyList;
ScriptValuePointer meshesToScriptValue(ScriptEngine* engine, const MeshProxyList& in);
void meshesFromScriptValue(const ScriptValuePointer& value, MeshProxyList& out);

class MeshFace;
ScriptValuePointer meshFaceToScriptValue(ScriptEngine* engine, const MeshFace& meshFace);
void meshFaceFromScriptValue(const ScriptValuePointer& object, MeshFace& meshFaceResult);
ScriptValuePointer qVectorMeshFaceToScriptValue(ScriptEngine* engine, const QVector<MeshFace>& vector);
void qVectorMeshFaceFromScriptValue(const ScriptValuePointer& array, QVector<MeshFace>& result);

enum class StencilMaskMode;
ScriptValuePointer stencilMaskModeToScriptValue(ScriptEngine* engine, const StencilMaskMode& stencilMode);
void stencilMaskModeFromScriptValue(const ScriptValuePointer& object, StencilMaskMode& stencilMode);

class MiniPromise;
void promiseFromScriptValue(const ScriptValuePointer& object, std::shared_ptr<MiniPromise>& promise);
ScriptValuePointer promiseToScriptValue(ScriptEngine* engine, const std::shared_ptr<MiniPromise>& promise);

#endif  // #define hifi_ScriptValueUtils_h
