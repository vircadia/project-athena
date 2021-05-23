//
//  Mat4.h
//  libraries/script-engine/src
//
//  Created by Anthony Thibault on 3/7/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Scriptable 4x4 Matrix class library.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Mat4_h
#define hifi_Mat4_h

#include <QObject>
#include <QString>
#include <QVector>
#include <glm/glm.hpp>
#include "RegisteredMetaTypes.h"
#include "Scriptable.h"

/*@jsdoc
 * The <code>Mat4</code> API provides facilities for generating and using 4 x 4 matrices. These matrices are typically used to 
 * represent transforms (scale, rotate, and translate) that convert one coordinate system into another, or perspective 
 * transforms that convert 3D points into screen coordinates.
 *
 * @namespace Mat4
 * @variation 0
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 */

/// Scriptable Mat4 object.  Used exclusively in the JavaScript API
class Mat4 : public QObject, protected Scriptable {
    Q_OBJECT

public slots:

    /*@jsdoc
     * Multiplies two matrices.
     * @function Mat4(0).multiply
     * @param {Mat4} m1 - The first matrix.
     * @param {Mat4} m2 - The second matrix.
     * @returns {Mat4} <code>m1</code> multiplied with <code>m2</code>.
     */
    glm::mat4 multiply(const glm::mat4& m1, const glm::mat4& m2) const;


    /*@jsdoc
     * Creates a matrix that represents a rotation and translation.
     * @function Mat4(0).createFromRotAndTrans
     * @param {Quat} rot - The rotation.
     * @param {Vec3} trans - The translation.
     * @returns {Mat4} The matrix that represents the rotation and translation.
     * @example <caption>Create a matrix with rotation and translation.</caption>
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromRotAndTrans(rot, trans);
     * Mat4.print("Matrix:", matrix);
     * // Matrix: dmat4x4((0.353553, 0.612372, -0.707107, 0.000000),
     * //                 (-0.573223, 0.739199, 0.353553, 0.000000),
     * //                 (0.739199, 0.280330, 0.612372, 0.000000),
     * //                 (10.000000, 11.000000, 12.000000, 1.000000))
     */
    glm::mat4 createFromRotAndTrans(const glm::quat& rot, const glm::vec3& trans) const;

    /*@jsdoc
     * Creates a matrix that represents a scale, rotation, and translation.
     * @function Mat4(0).createFromScaleRotAndTrans
     * @param {Vec3} scale - The scale.
     * @param {Quat} rot - The rotation.
     * @param {Vec3} trans - The translation.
     * @returns {Mat4} The matrix that represents the scale, rotation, and translation.
     * @example <caption>Create a matrix with scale, rotation, and translation.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * Mat4.print("Matrix:", matrix);
     * // Matrix: dmat4x4((0.707107, 1.224745, -1.414214, 0.000000),
     * //                 (-1.146447, 1.478398, 0.707107, 0.000000),
     * //                 (1.478398, 0.560660, 1.224745, 0.000000),
     * //                 (10.000000, 11.000000, 12.000000, 1.000000))
     */
    glm::mat4 createFromScaleRotAndTrans(const glm::vec3& scale, const glm::quat& rot, const glm::vec3& trans) const;

    /*@jsdoc
     * Creates a matrix from columns of values.
     * @function Mat4(0).createFromColumns
     * @param {Vec4} col0 - Column 0 values.
     * @param {Vec4} col1 - Column 1 values.
     * @param {Vec4} col2 - Column 2 values.
     * @param {Vec4} col3 - Column 3 valuse.
     * @returns {Mat4} The matrix with the specified columns values.
     * @example <caption>Create a matrix from columns.</caption>
     * var col0 = { x: 0.707107, y: 1.224745, z: -1.414214, w: 0.0 };
     * var col1 = { x: -1.146447, y: 1.478398, z: 0.707107, w: 0.0 };
     * var col2 = { x: 1.478398, y: 0.560660, z: 1.224745, w: 0.0 };
     * var col3 = { x: 10.0, y: 11.0, z: 12.0, w: 1.0 };
     * var matrix = Mat4.createFromColumns(col0, col1, col2, col3);
     * Mat4.print("Matrix:", matrix);
     * //Matrix: dmat4x4((0.707107, 1.224745, -1.414214, 0.000000),
     * //                (-1.146447, 1.478398, 0.707107, 0.000000),
     * //                (1.478398, 0.560660, 1.224745, 0.000000),
     * //                (10.000000, 11.000000, 12.000000, 1.000000))
     */
    glm::mat4 createFromColumns(const glm::vec4& col0, const glm::vec4& col1, const glm::vec4& col2, const glm::vec4& col3) const;

    /*@jsdoc
     * Creates a matrix from an array of values.
     * @function Mat4(0).createFromArray
     * @param {number[]} arr  - The array of values, starting with column 0.
     * @returns {Mat4} The matrix with the specified values.
     * @example <caption>Create a matrix from an array.</caption>
     * var arr = [
     *     0.707107, 1.224745, -1.414214, 0.0,
     *     -1.146447, 1.478398, 0.707107, 0.0,
     *     1.478398, 0.560660, 1.224745, 0.0,
     *     10.0, 11.0, 12.0, 1.00
     * ];
     * var matrix = Mat4.createFromArray(arr);
     * Mat4.print("Matrix:", matrix);
     * //Matrix: dmat4x4((0.707107, 1.224745, -1.414214, 0.000000),
     * //                (-1.146447, 1.478398, 0.707107, 0.000000),
     * //                (1.478398, 0.560660, 1.224745, 0.000000),
     * //                (10.000000, 11.000000, 12.000000, 1.000000))
     */
    glm::mat4 createFromArray(const QVector<float>& floats) const;


    /*@jsdoc
     * Extracts the translation from a matrix.
     * @function Mat4(0).extractTranslation
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The translation contained in the matrix.
     * @example <caption>Extract the translation from a matrix.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * 
     * trans = Mat4.extractTranslation(matrix);
     * print("Translation: " + JSON.stringify(trans));
     * // Translation: {"x":10,"y":11,"z":12}
     */
    glm::vec3 extractTranslation(const glm::mat4& m) const;

    /*@jsdoc
     * Extracts the rotation from a matrix.
     * @function Mat4(0).extractRotation
     * @param {Mat4} m - The matrix.
     * @returns {Quat} The rotation contained in the matrix.
     * @example <caption>Extract the rotation from a matrix.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * 
     * rot = Mat4.extractRotation(matrix);
     * print("Rotation: " + JSON.stringify(Quat.safeEulerAngles(rot)));
     * // Rotation: {"x":29.999998092651367,"y":45.00000762939453,"z":60.000003814697266}
     */
    glm::quat extractRotation(const glm::mat4& m) const;

    /*@jsdoc
     * Extracts the scale from a matrix.
     * @function Mat4(0).extractScale
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The scale contained in the matrix.
     * @example <caption>Extract the scale from a matrix.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * 
     * scale = Mat4.extractScale(matrix);
     * print("Scale: " + JSON.stringify(scale));
     * // Scale: {"x":1.9999998807907104,"y":1.9999998807907104,"z":1.9999998807907104}
     */
    glm::vec3 extractScale(const glm::mat4& m) const;


    /*@jsdoc
     * Transforms a point into a new coordinate system: the point value is scaled, rotated, and translated.
     * @function Mat4(0).transformPoint
     * @param {Mat4} m - The transform to the new coordinate system.
     * @param {Vec3} point - The point to transform.
     * @returns {Vec3} The point in the new coordinate system.
     * @example <caption>Transform a point.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(0, 45, 0);
     * var trans = { x: 0, y: 10, z: 0 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * 
     * var point = { x: 1, y: 1, z: 1 };
     * var transformedPoint = Mat4.transformPoint(matrix, point);
     * print("Transformed point: " + JSON.stringify(transformedPoint));
     * // Transformed point: { "x": 2.8284270763397217, "y": 12, "z": -2.384185791015625e-7 }
     */
    glm::vec3 transformPoint(const glm::mat4& m, const glm::vec3& point) const;

    /*@jsdoc
     * Transforms a vector into a new coordinate system: the vector is scaled and rotated.
     * @function Mat4(0).transformVector
     * @param {Mat4} m - The transform to the new coordinate system.
     * @param {Vec3} vector - The vector to transform.
     * @returns {Vec3} The vector in the new coordinate system.
     * @example <caption>Transform a vector.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(0, 45, 0);
     * var trans = { x: 0, y: 10, z: 0 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     *
     * var vector = { x: 1, y: 1, z: 1 };
     * var transformedVector = Mat4.transformVector(matrix, vector);
     * print("Transformed vector: " + JSON.stringify(transformedVector));
     * // Transformed vector: { "x": 2.8284270763397217, "y": 2, "z": -2.384185791015625e-7 }
     */
    glm::vec3 transformVector(const glm::mat4& m, const glm::vec3& vector) const;


    /*@jsdoc
     * Calculates the inverse of a matrix.
     * @function Mat4(0).inverse
     * @param {Mat4} m - The matrix.
     * @returns {Mat4} The inverse of the matrix.
     * @example <caption>A matrix multiplied with its inverse is the unit matrix.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * var inverse = Mat4.inverse(matrix);
     * var multiplied = Mat4.multiply(matrix, inverse);
     * Mat4.print("Multiplied:", multiplied);
     * //Multiplied: dmat4x4((1.000000, 0.000000, 0.000000, 0.000000),
     * //                    (0.000000, 1.000000, -0.000000, 0.000000),
     * //                    (0.000000, 0.000000, 1.000000, 0.000000),
     * //                    (0.000000, 0.000000, 0.000001, 1.000000))
     */
    glm::mat4 inverse(const glm::mat4& m) const;


    /*@jsdoc
     * Gets the "forward" direction that the camera would face if its orientation was set to the rotation contained in a 
     * matrix. The High Fidelity camera has axes x = right, y = up, -z = forward. 
     * <p>Synonym for {@link Mat4(0).getForward|getForward}.</p>
     * @function Mat4(0).getFront
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The negative z-axis rotated by orientation.
     */
    // redundant, calls getForward which better describes the returned vector as a direction
    glm::vec3 getFront(const glm::mat4& m) const { return getForward(m); }

    /*@jsdoc
     * Gets the "forward" direction that the camera would face if its orientation was set to the rotation contained in a
     * matrix. The High Fidelity camera has axes x = right, y = up, -z = forward.
     * @function Mat4(0).getForward
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The negative z-axis rotated by the rotation in the matrix.
     * @example <caption>Demonstrate that the "forward" direction is the negative z-axis.</caption>
     * var rot = Quat.IDENTITY;
     * var trans = Vec3.ZERO;
     * var matrix = Mat4.createFromRotAndTrans(rot, trans);
     * var forward = Mat4.getForward(matrix);
     * print("Forward: " + JSON.stringify(forward));
     * // Forward: {"x":0,"y":0,"z":-1}
     */
    glm::vec3 getForward(const glm::mat4& m) const;

    /*@jsdoc
     * Gets the "right" direction that the camera would have if its orientation was set to the rotation contained in a matrix. 
     * The High Fidelity camera has axes x = right, y = up, -z = forward.
     * @function Mat4(0).getRight
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The x-axis rotated by the rotation in the matrix.
     */
    glm::vec3 getRight(const glm::mat4& m) const;

    /*@jsdoc
     * Gets the "up" direction that the camera would have if its orientation was set to the rotation contained in a matrix. The 
     * High Fidelity camera has axes x = right, y = up, -z = forward.
     * @function Mat4(0).getUp
     * @param {Mat4} m - The matrix.
     * @returns {Vec3} The y-axis rotated by the rotation in the matrix.
     */
    glm::vec3 getUp(const glm::mat4& m) const;


    /*@jsdoc
     * Prints a matrix to the program log as a label followed by the matrix's values.
     * @function Mat4(0).print
     * @param {string} label - The label to print.
     * @param {Mat4} m - The matrix to print.
     * @param {boolean} [transpose=false] - <code>true</code> to transpose the matrix before printing (so that it prints the 
     *     matrix's rows), <code>false</code> to not transpose the matrix (so that it prints the matrix's columns).
     * @example <caption>Two ways of printing a label and matrix value.</caption>
     * var scale = Vec3.multiply(2, Vec3.ONE);
     * var rot = Quat.fromPitchYawRollDegrees(30, 45, 60);
     * var trans = { x: 10, y: 11, z: 12 };
     * var matrix = Mat4.createFromScaleRotAndTrans(scale, rot, trans);
     * 
     * Mat4.print("Matrix:", matrix);
     * // Matrix: dmat4x4((0.707107, 1.224745, -1.414214, 0.000000),
     * //                 (-1.146447, 1.478398, 0.707107, 0.000000),
     * //                 (1.478398, 0.560660, 1.224745, 0.000000),
     * //                 (10.000000, 11.000000, 12.000000, 1.000000))
     * 
     * print("Matrix: " + JSON.stringify(matrix));
     * // Matrix: {"r0c0":0.7071067094802856,"r1c0":1.2247446775436401,"r2c0":-1.4142136573791504,"r3c0":0,
     * //          "r0c1": -1.1464465856552124, "r1c1": 1.4783978462219238, "r2c1": 0.7071066498756409, "r3c1": 0,
     * //          "r0c2": 1.4783978462219238, "r1c2": 0.5606603026390076, "r2c2": 1.2247447967529297, "r3c2": 0,
     * //          "r0c3": 10, "r1c3": 11, "r2c3": 12, "r3c3": 1}
     */
    void print(const QString& label, const glm::mat4& m, bool transpose = false) const;
};

#endif // hifi_Mat4_h
