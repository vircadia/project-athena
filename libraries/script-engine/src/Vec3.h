//
//  Vec3.h
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 1/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Scriptable Vec3 class library.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_Vec3_h
#define hifi_Vec3_h

#include <QtCore/QObject>
#include <QtCore/QString>

#include "GLMHelpers.h"
#include "Scriptable.h"

/*@jsdoc
 * The <code>Vec3</code> API provides facilities for generating and manipulating 3-dimensional vectors. Vircadia uses a 
 * right-handed Cartesian coordinate system where the y-axis is the "up" and the negative z-axis is the "front" direction.
 * <img alt="Vircadia coordinate system" src="https://apidocs.vircadia.dev/images/opengl-coord-system.jpg" />
 *
 * @namespace Vec3
 * @variation 0
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {Vec3} UNIT_X - <code>{ x: 1, y: 0, z: 0 }</code> : Unit vector in the x-axis direction. <em>Read-only.</em>
 * @property {Vec3} UNIT_Y - <code>{ x: 0, y: 1, z: 0 }</code> : Unit vector in the y-axis direction. <em>Read-only.</em>
 * @property {Vec3} UNIT_Z - <code>{ x: 0, y: 0, z: 1 }</code> : Unit vector in the z-axis direction. <em>Read-only.</em>
 * @property {Vec3} UNIT_NEG_X - <code>{ x: -1, y: 0, z: 0 }</code> : Unit vector in the negative x-axis direction. 
 *     <em>Read-only.</em>
 * @property {Vec3} UNIT_NEG_Y - <code>{ x: 0, y: -1, z: 0 }</code> : Unit vector in the negative y-axis direction. 
 *     <em>Read-only.</em>
 * @property {Vec3} UNIT_NEG_Z - <code>{ x: 0, y: 0, z: -1 }</code> : Unit vector in the negative z-axis direction. 
 *     <em>Read-only.</em>
 * @property {Vec3} UNIT_XY - <code>{ x: 0.707107, y: 0.707107, z: 0 }</code> : Unit vector in the direction of the diagonal 
 *     between the x and y axes. <em>Read-only.</em>
 * @property {Vec3} UNIT_XZ - <code>{ x: 0.707107, y: 0, z: 0.707107 }</code> : Unit vector in the direction of the diagonal 
 *     between the x and z axes. <em>Read-only.</em>
 * @property {Vec3} UNIT_YZ - <code>{ x: 0, y: 0.707107, z: 0.707107 }</code> : Unit vector in the direction of the diagonal 
 *     between the y and z axes. <em>Read-only.</em>
 * @property {Vec3} UNIT_XYZ - <code>{ x: 0.577350, y: 0.577350, z: 0.577350 }</code> : Unit vector in the direction of the 
 *     diagonal between the x, y, and z axes. <em>Read-only.</em>
 * @property {Vec3} FLOAT_MAX - <code>{ x: 3.402823e+38, y: 3.402823e+38, z: 3.402823e+38 }</code> : Vector with all axis 
 *     values set to the maximum floating point value. <em>Read-only.</em>
 * @property {Vec3} FLOAT_MIN - <code>{ x: -3.402823e+38, y: -3.402823e+38, z: -3.402823e+38 }</code> : Vector with all axis 
 *     values set to the negative of the maximum floating point value. <em>Read-only.</em>
 * @property {Vec3} ZERO - <code>{ x: 0, y: 0, z: 0 }</code> : Vector with all axis values set to <code>0</code>. 
 *     <em>Read-only.</em>
 * @property {Vec3} ONE - <code>{ x: 1, y: 1, z: 1 }</code> : Vector with all axis values set to <code>1</code>. 
 *     <em>Read-only.</em>
 * @property {Vec3} TWO - <code>{ x: 2, y: 2, z: 2 }</code> : Vector with all axis values set to <code>2</code>. 
 *     <em>Read-only.</em>
 * @property {Vec3} HALF - <code>{ x: 0.5, y: 0.5, z: 0.5 }</code> : Vector with all axis values set to <code>0.5</code>. 
 *     <em>Read-only.</em>
 * @property {Vec3} RIGHT - <code>{ x: 1, y: 0, z: 0 }</code> : Unit vector in the "right" direction. Synonym for 
 *     <code>UNIT_X</code>. <em>Read-only.</em>
 * @property {Vec3} UP - <code>{ x: 0, y: 1, z: 0 }</code> : Unit vector in the "up" direction. Synonym for 
 *     <code>UNIT_Y</code>. <em>Read-only.</em>
 * @property {Vec3} FRONT - <code>{ x: 0, y: 0, z: -1 }</code> : Unit vector in the "front" direction. Synonym for 
 *     <code>UNIT_NEG_Z</code>. <em>Read-only.</em>
 */

/// Scriptable interface a Vec3ernion helper class object. Used exclusively in the JavaScript API
class Vec3 : public QObject, protected Scriptable {
    Q_OBJECT
    Q_PROPERTY(glm::vec3 UNIT_X READ UNIT_X CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_Y READ UNIT_Y CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_Z READ UNIT_Z CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_NEG_X READ UNIT_NEG_X CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_NEG_Y READ UNIT_NEG_Y CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_NEG_Z READ UNIT_NEG_Z CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_XY READ UNIT_XY CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_XZ READ UNIT_XZ CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_YZ READ UNIT_YZ CONSTANT)
    Q_PROPERTY(glm::vec3 UNIT_XYZ READ UNIT_XYZ CONSTANT)
    Q_PROPERTY(glm::vec3 FLOAT_MAX READ FLOAT_MAX CONSTANT)
    Q_PROPERTY(glm::vec3 FLOAT_MIN READ FLOAT_MIN CONSTANT)
    Q_PROPERTY(glm::vec3 ZERO READ ZERO CONSTANT)
    Q_PROPERTY(glm::vec3 ONE READ ONE CONSTANT)
    Q_PROPERTY(glm::vec3 TWO READ TWO CONSTANT)
    Q_PROPERTY(glm::vec3 HALF READ HALF CONSTANT)
    Q_PROPERTY(glm::vec3 RIGHT READ RIGHT CONSTANT)
    Q_PROPERTY(glm::vec3 UP READ UP CONSTANT)
    Q_PROPERTY(glm::vec3 FRONT READ FRONT CONSTANT)

public slots:
    
    /*@jsdoc
     * Calculates the reflection of a vector in a plane.
     * @function Vec3(0).reflect
     * @param {Vec3} v - The vector to reflect.
     * @param {Vec3} normal - The normal of the plane.
     * @returns {Vec3} The vector reflected in the plane given by the normal.
     * @example <caption>Reflect a vector in the x-z plane.</caption>
     * var v = { x: 1, y: 2, z: 3 };
     * var normal = Vec3.UNIT_Y;
     * var reflected = Vec3.reflect(v, normal);
     * print(JSON.stringify(reflected));  // {"x":1,"y":-2,"z":3}
     */
    glm::vec3 reflect(const glm::vec3& v1, const glm::vec3& v2) { return glm::reflect(v1, v2); }
    
    /*@jsdoc
     * Calculates the cross product of two vectors.
     * @function Vec3(0).cross
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {Vec3} The cross product of <code>v1</code> and <code>v2</code>.
     * @example <caption>The cross product of x and y unit vectors is the z unit vector.</caption>
     * var v1 = { x: 1, y: 0, z: 0 };
     * var v2 = { x: 0, y: 1, z: 0 };
     * var crossProduct = Vec3.cross(v1, v2);
     * print(JSON.stringify(crossProduct)); // {"x":0,"y":0,"z":1}
     */
    glm::vec3 cross(const glm::vec3& v1, const glm::vec3& v2) { return glm::cross(v1, v2); }
    
    /*@jsdoc
     * Calculates the dot product of two vectors.
     * @function Vec3(0).dot
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {number} The dot product of <code>v1</code> and <code>v2</code>.
     * @example <caption>The dot product of vectors at right angles is <code>0</code>.</caption>
     * var v1 = { x: 1, y: 0, z: 0 };
     * var v2 = { x: 0, y: 1, z: 0 };
     * var dotProduct = Vec3.dot(v1, v2);
     * print(dotProduct); // 0
     */
    float dot(const glm::vec3& v1, const glm::vec3& v2) { return glm::dot(v1, v2); }
    
    /*@jsdoc
     * Multiplies a vector by a scale factor.
     * @function Vec3(0).multiply
     * @param {Vec3} v - The vector.
     * @param {number} scale - The scale factor.
     * @returns {Vec3} The vector with each ordinate value multiplied by the <code>scale</code>.
     */
    glm::vec3 multiply(const glm::vec3& v1, float f) { return v1 * f; }
    
    /*@jsdoc
     * Multiplies a vector by a scale factor.
     * @function Vec3(0).multiply
     * @param {number} scale - The scale factor.
     * @param {Vec3} v - The vector.
     * @returns {Vec3} The vector with each ordinate value multiplied by the <code>scale</code>.
     */
    glm::vec3 multiply(float f, const glm::vec3& v1) { return v1 * f; }
    
    /*@jsdoc
     * Multiplies two vectors.
     * @function Vec3(0).multiplyVbyV
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {Vec3} A vector formed by multiplying the ordinates of two vectors: <code>{ x: v1.x * v2.x, y: v1.y * v2.y, 
     *     z: v1.z * v2.z }</code>.
     * @example <caption>Multiply two vectors.</caption>
     * var v1 = { x: 1, y: 2, z: 3 };
     * var v2 = { x: 1, y: 2, z: 3 };
     * var multiplied = Vec3.multiplyVbyV(v1, v2);
     * print(JSON.stringify(multiplied));  // {"x":1,"y":4,"z":9}
     */
    glm::vec3 multiplyVbyV(const glm::vec3& v1, const glm::vec3& v2) { return v1 * v2; }
    
    /*@jsdoc
     * Rotates a vector.
     * @function Vec3(0).multiplyQbyV
     * @param {Quat} q - The rotation to apply.
     * @param {Vec3} v - The vector to rotate.
     * @returns {Vec3} <code>v</code> rotated by <code>q</code>.
     * @example <caption>Rotate the negative z-axis by 90 degrees about the x-axis.</caption>
     * var v = Vec3.UNIT_NEG_Z;
     * var q = Quat.fromPitchYawRollDegrees(90, 0, 0);
     * var result = Vec3.multiplyQbyV(q, v);
     * print(JSON.stringify(result));  // {"x":0,"y":1.000,"z":1.19e-7}
     */
    glm::vec3 multiplyQbyV(const glm::quat& q, const glm::vec3& v) { return q * v; }
    
    /*@jsdoc
     * Calculates the sum of two vectors.
     * @function Vec3(0).sum
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {Vec3} The sum of the two vectors.
     */
    glm::vec3 sum(const glm::vec3& v1, const glm::vec3& v2) { return v1 + v2; }
    
    /*@jsdoc
     * Calculates one vector subtracted from another.
     * @function Vec3(0).subtract
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {Vec3} The second vector subtracted from the first.
     */
    glm::vec3 subtract(const glm::vec3& v1, const glm::vec3& v2) { return v1 - v2; }
    
    /*@jsdoc
     * Calculates the length of a vector
     * @function Vec3(0).length
     * @param {Vec3} v - The vector.
     * @returns {number} The length of the vector.
     */
    float length(const glm::vec3& v) { return glm::length(v); }
    
    /*@jsdoc
     * Calculates the distance between two points.
     * @function Vec3(0).distance
     * @param {Vec3} p1 - The first point.
     * @param {Vec3} p2 - The second point.
     * @returns {number} The distance between the two points.
     * @example <caption>The distance between two points is aways positive.</caption>
     * var p1 = { x: 0, y: 0, z: 0 };
     * var p2 = { x: 0, y: 0, z: 10 };
     * var distance = Vec3.distance(p1, p2);
     * print(distance); // 10
     *
     * p2 = { x: 0, y: 0, z: -10 };
     * distance = Vec3.distance(p1, p2);
     * print(distance); // 10
     */
    float distance(const glm::vec3& v1, const glm::vec3& v2) { return glm::distance(v1, v2); }
    
    /*@jsdoc
     * Calculates the angle of rotation from one vector onto another, with the sign depending on a reference vector.
     * @function Vec3(0).orientedAngle
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @param {Vec3} ref - Reference vector.
     * @returns {number} The angle of rotation from the first vector to the second, in degrees. The value is positive if the 
     * rotation axis aligns with the reference vector (has a positive dot product), otherwise the value is negative.
     * @example <caption>Compare <code>Vec3.getAngle()</code> and <code>Vec3.orientedAngle()</code>.</caption>
     * var v1 = { x: 5, y: 0, z: 0 };
     * var v2 = { x: 5, y: 0, z: 5 };
     *
     * var angle = Vec3.getAngle(v1, v2);
     * print(angle * 180 / Math.PI);  // 45
     *
     * print(Vec3.orientedAngle(v1, v2, Vec3.UNIT_Y));  // -45
     * print(Vec3.orientedAngle(v1, v2, Vec3.UNIT_NEG_Y));  // 45
     * print(Vec3.orientedAngle(v1, v2, { x: 1, y: 2, z: -1 }));  // -45
     * print(Vec3.orientedAngle(v1, v2, { x: 1, y: -2, z: -1 }));  // 45
     */
    float orientedAngle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3);
    
    /*@jsdoc
     * Normalizes a vector so that its length is <code>1</code>.
     * @function Vec3(0).normalize
     * @param {Vec3} v - The vector to normalize.
     * @returns {Vec3} The vector normalized to have a length of <code>1</code>.
     * @example <caption>Normalize a vector.</caption>
     * var v = { x: 10, y: 10, z: 0 };
     * var normalized = Vec3.normalize(v);
     * print(JSON.stringify(normalized));  // {"x":0.7071,"y":0.7071,"z":0}
     * print(Vec3.length(normalized));  // 1
     */
    glm::vec3 normalize(const glm::vec3& v) { return glm::normalize(v); };
    
    /*@jsdoc
     * Calculates a linear interpolation between two vectors.
     * @function Vec3(0).mix
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @param {number} factor - The interpolation factor, range <code>0.0</code> &ndash; <code>1.0</code>.
     * @returns {Vec3} The linear interpolation between the two vectors: <code>(1 - factor) * v1 + factor * v2</code>.
     * @example <caption>Linear interpolation between two vectors.</caption>
     * var v1 = { x: 10, y: 0, z: 0 };
     * var v2 = { x: 0, y: 10, z: 0 };
     * var interpolated = Vec3.mix(v1, v2, 0.75);  // 1/4 of v1 and 3/4 of v2.
     * print(JSON.stringify(interpolated));  // {"x":2.5,"y":7.5","z":0}
     */
    glm::vec3 mix(const glm::vec3& v1, const glm::vec3& v2, float m) { return glm::mix(v1, v2, m); }
    
    /*@jsdoc
     * Prints the vector to the program log, as a text label followed by the vector value.
     * @function Vec3(0).print
     * @param {string} label - The label to print.
     * @param {Vec3} v - The vector value to print.
     * @example <caption>Two ways of printing a label and vector value.</caption>
     * var v = { x: 1, y: 2, z: 3 };
     * Vec3.print("Vector: ", v);  // dvec3(1.000000, 2.000000, 3.000000)
     * print("Vector: " + JSON.stringify(v));  // {"x":1,"y":2,"z":3}
     */
    void print(const QString& label, const glm::vec3& v);
    
    /*@jsdoc
     * Tests whether two vectors are equal.
     * <p><strong>Note:</strong> The vectors must be exactly equal in order for <code>true</code> to be returned; it is often 
     * better to use {@link Vec3(0).withinEpsilon|withinEpsilon}.</p>
     * @function Vec3(0).equal
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {boolean} <code>true</code> if the two vectors are exactly equal, otherwise <code>false</code>.
     * @example <caption> Vectors are only equal if exactly the same.</caption>
     * var v1 = { x: 10, y: 10, z: 10 };
     * var v2 = { x: 10, y: 10, z: 10 };
     *
     * var equal = Vec3.equal(v1, v2);
     * print(equal);  // true
     *
     * v2 = { x: 10, y: 10, z: 10.0005 };
     * equal = Vec3.equal(v1, v2);
     * print(equal);  // false
     */
    bool equal(const glm::vec3& v1, const glm::vec3& v2) { return v1 == v2; }
    
    /*@jsdoc
     * Tests whether two vectors are equal within a tolerance.
     * <p><strong>Note:</strong> It is often better to use this function than {@link Vec3(0).equal|equal}.</p>
     * @function Vec3(0).withinEpsilon
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @param {number} epsilon - The maximum distance between the two vectors.
     * @returns {boolean} <code>true</code> if the distance between the points represented by the vectors is less than or equal 
     * to <code>epsilon</code>, otherwise <code>false</code>.
     * @example <caption>Testing vectors for near equality.</caption>
     * var v1 = { x: 10, y: 10, z: 10 };
     * var v2 = { x: 10, y: 10, z: 10.0005 };
     *
     * var equal = Vec3.equal(v1, v2);
     * print(equal);  // false
     *
     * equal = Vec3.withinEpsilon(v1, v2, 0.001);
     * print(equal);  // true
     */
    bool withinEpsilon(const glm::vec3& v1, const glm::vec3& v2, float epsilon);
    
    /*@jsdoc
     * Calculates polar coordinates (elevation, azimuth, radius) that transform the unit z-axis vector onto a point.
     * @function Vec3(0).toPolar
     * @param {Vec3} p - The point to calculate the polar coordinates for.
     * @returns {Vec3} Vector of polar coordinates for the point: <code>x</code> elevation rotation about the x-axis in 
     *     radians, <code>y</code> azimuth rotation about the y-axis in radians, and <code>z</code> radius.
     * @example <caption>Polar coordinates for a point.</caption>
     * var v = { x: 5, y: 2.5, z: 5 };
     * var polar = Vec3.toPolar(v);
     * print("Elevation: " + polar.x * 180 / Math.PI);  // -19.471
     * print("Azimuth: " + polar.y * 180 / Math.PI);  // 45
     * print("Radius: " + polar.z);  // 7.5
     */
    // FIXME misnamed, should be 'spherical' or 'euler' depending on the implementation
    glm::vec3 toPolar(const glm::vec3& v);
    
    /*@jsdoc
     * Calculates the coordinates of a point from polar coordinate transformation of the unit z-axis vector.
     * @function Vec3(0).fromPolar
     * @param {Vec3} polar - The polar coordinates of a point: <code>x</code> elevation rotation about the x-axis in radians, 
     *    <code>y</code> azimuth rotation about the y-axis in radians, and <code>z</code> radius.
     * @returns {Vec3} The coordinates of the point.
     * @example <caption>Polar coordinates to Cartesian.</caption>
     * var polar = { x: -19.471 * Math.PI / 180, y: 45 * Math.PI / 180, z: 7.5 };
     * var p = Vec3.fromPolar(polar);
     * print(JSON.stringify(p));  // {"x":5,"y":2.5,"z":5}
     */
    // FIXME misnamed, should be 'spherical' or 'euler' depending on the implementation
    glm::vec3 fromPolar(const glm::vec3& polar);
    
    /*@jsdoc
     * Calculates the unit vector corresponding to polar coordinates elevation and azimuth transformation of the unit z-axis 
     * vector.
     * @function Vec3(0).fromPolar
     * @param {number} elevation - Rotation about the x-axis, in radians.
     * @param {number} azimuth - Rotation about the y-axis, in radians.
     * @returns {Vec3} Unit vector for the direction specified by <code>elevation</code> and <code>azimuth</code>.
     * @example <caption>Polar coordinates to Cartesian coordinates.</caption>
     * var elevation = -19.471 * Math.PI / 180;
     * var rotation = 45 * Math.PI / 180;
     * var p = Vec3.fromPolar(elevation, rotation);
     * print(JSON.stringify(p));  // {"x":0.667,"y":0.333,"z":0.667}
     * p = Vec3.multiply(7.5, p);
     * print(JSON.stringify(p));  // {"x":5,"y":2.5,"z":5}
     */
    // FIXME misnamed, should be 'spherical' or 'euler' depending on the implementation
    glm::vec3 fromPolar(float elevation, float azimuth);
    
    /*@jsdoc
     * Calculates the angle between two vectors.
     * @function Vec3(0).getAngle
     * @param {Vec3} v1 - The first vector.
     * @param {Vec3} v2 - The second vector.
     * @returns {number} The angle between the two vectors, in radians.
     * @example <caption>Calculate the angle between two vectors.</caption>
     * var v1 = { x: 10, y: 0, z: 0 };
     * var v2 = { x: 0, y: 0, z: 10 };
     * var angle = Vec3.getAngle(v1, v2);
     * print(angle * 180 / Math.PI);  // 90
     */
    float getAngle(const glm::vec3& v1, const glm::vec3& v2);

private:
    const glm::vec3& UNIT_X() { return Vectors::UNIT_X; }
    const glm::vec3& UNIT_Y() { return Vectors::UNIT_Y; }
    const glm::vec3& UNIT_Z() { return Vectors::UNIT_Z; }
    const glm::vec3& UNIT_NEG_X() { return Vectors::UNIT_NEG_X; }
    const glm::vec3& UNIT_NEG_Y() { return Vectors::UNIT_NEG_Y; }
    const glm::vec3& UNIT_NEG_Z() { return Vectors::UNIT_NEG_Z; }
    const glm::vec3& UNIT_XY() { return Vectors::UNIT_XY; }
    const glm::vec3& UNIT_XZ() { return Vectors::UNIT_XZ; }
    const glm::vec3& UNIT_YZ() { return Vectors::UNIT_YZ; }
    const glm::vec3& UNIT_XYZ() { return Vectors::UNIT_XYZ; }
    const glm::vec3& FLOAT_MAX() { return Vectors::MAX; }
    const glm::vec3& FLOAT_MIN() { return Vectors::MIN; }
    const glm::vec3& ZERO() { return Vectors::ZERO; }
    const glm::vec3& ONE() { return Vectors::ONE; }
    const glm::vec3& TWO() { return Vectors::TWO; }
    const glm::vec3& HALF() { return Vectors::HALF; }
    const glm::vec3& RIGHT() { return Vectors::RIGHT; }
    const glm::vec3& UP() { return Vectors::UP; }
    const glm::vec3& FRONT() { return Vectors::FRONT; }
};

#endif // hifi_Vec3_h
