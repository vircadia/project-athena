//
//  Quat.h
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 1/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Scriptable Quaternion class library.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Quat_h
#define hifi_Quat_h

#include <glm/gtc/quaternion.hpp>

#include <QObject>
#include <QString>

#include <GLMHelpers.h>

#include "Scriptable.h"

/*@jsdoc
 * A quaternion value. See also the {@link Quat(0)|Quat} API.
 * @typedef {object} Quat
 * @property {number} x - Imaginary component i.
 * @property {number} y - Imaginary component j.
 * @property {number} z - Imaginary component k.
 * @property {number} w - Real component.
 */

/*@jsdoc
 * The <code>Quat</code> API provides facilities for generating and manipulating quaternions.
 * Quaternions should be used in preference to Euler angles wherever possible because quaternions don't suffer from the problem
 * of gimbal lock.
 *
 * @namespace Quat
 * @variation 0
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {Quat} IDENTITY - <code>{ x: 0, y: 0, z: 0, w: 1 }</code> : The identity rotation, i.e., no rotation.
 *     <em>Read-only.</em>
 * @example <caption>Print the <code>IDENTITY</code> value.</caption>
 * print(JSON.stringify(Quat.IDENTITY)); // { x: 0, y: 0, z: 0, w: 1 }
 * print(JSON.stringify(Quat.safeEulerAngles(Quat.IDENTITY))); // { x: 0, y: 0, z: 0 }
 */

/// Scriptable interface a Quaternion helper class object. Used exclusively in the JavaScript API
class Quat : public QObject, protected Scriptable {
    Q_OBJECT
    Q_PROPERTY(glm::quat IDENTITY READ IDENTITY CONSTANT)

public slots:

    /*@jsdoc
     * Multiplies two quaternions.
     * @function Quat(0).multiply
     * @param {Quat} q1 - The first quaternion.
     * @param {Quat} q2 - The second quaternion.
     * @returns {Quat} <code>q1</code> multiplied with <code>q2</code>.
     * @example <caption>Calculate the orientation of your avatar's right hand in world coordinates.</caption>
     * var handController = Controller.Standard.RightHand;
     * var handPose = Controller.getPoseValue(handController);
     * if (handPose.valid) {
     *     var handOrientation = Quat.multiply(MyAvatar.orientation, handPose.rotation);
     * }
     */
    glm::quat multiply(const glm::quat& q1, const glm::quat& q2);

    /*@jsdoc
     * Normalizes a quaternion.
     * @function Quat(0).normalize
     * @param {Quat} q - The quaternion to normalize.
     * @returns {Quat} <code>q</code> normalized to have unit length.
     * @example <caption>Normalize a repeated delta rotation so that maths rounding errors don't accumulate.</caption>
     * var deltaRotation = Quat.fromPitchYawRollDegrees(0, 0.1, 0);
     * var currentRotation = Quat.ZERO;
     * while (Quat.safeEulerAngles(currentRotation).y < 180) {
     *     currentRotation = Quat.multiply(deltaRotation, currentRotation);
     *     currentRotation = Quat.normalize(currentRotation);
     *     // Use currentRotatation for something.
     * }
     */
    glm::quat normalize(const glm::quat& q);

    /*@jsdoc
    * Calculates the conjugate of a quaternion. For a unit quaternion, its conjugate is the same as its 
    * {@link Quat(0).inverse|Quat.inverse}.
    * @function Quat(0).conjugate
    * @param {Quat} q - The quaternion to conjugate.
    * @returns {Quat} The conjugate of <code>q</code>.
    * @example <caption>A unit quaternion multiplied by its conjugate is a zero rotation.</caption>
    * var quaternion = Quat.fromPitchYawRollDegrees(10, 20, 30);
    * Quat.print("quaternion", quaternion, true); // dvec3(10.000000, 20.000004, 30.000004)
    * var conjugate = Quat.conjugate(quaternion);
    * Quat.print("conjugate", conjugate, true); // dvec3(1.116056, -22.242186, -28.451778)
    * var identity = Quat.multiply(conjugate, quaternion);
    * Quat.print("identity", identity, true); // dvec3(0.000000, 0.000000, 0.000000)
    */
    glm::quat conjugate(const glm::quat& q);

    /*@jsdoc
     * Calculates a camera orientation given an eye position, point of interest, and "up" direction. The camera's negative 
     * z-axis is the forward direction. The result has zero roll about its forward direction with respect to the given "up" 
     * direction.
     * @function Quat(0).lookAt
     * @param {Vec3} eye - The eye position.
     * @param {Vec3} target - The point to look at.
     * @param {Vec3} up - The "up" direction.
     * @returns {Quat} A quaternion that orients the negative z-axis to point along the eye-to-target vector and the x-axis to
     * be the cross product of the eye-to-target and up vectors.
     * @example <caption>Rotate your view in independent mode to look at the world origin upside down.</caption>
     * Camera.mode = "independent";
     * Camera.orientation = Quat.lookAt(Camera.position, Vec3.ZERO, Vec3.UNIT_NEG_Y);
     */
    glm::quat lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

    /*@jsdoc
    * Calculates a camera orientation given an eye position and point of interest. The camera's negative z-axis is the forward 
    * direction. The result has zero roll about its forward direction.
    * @function Quat(0).lookAtSimple
    * @param {Vec3} eye - The eye position.
    * @param {Vec3} target - The point to look at.
    * @returns {Quat} A quaternion that orients the negative z-axis to point along the eye-to-target vector and the x-axis to be
    *     the cross product of the eye-to-target and an "up" vector. The "up" vector is the y-axis unless the eye-to-target
    *     vector is nearly aligned with it (i.e., looking near vertically up or down), in which case the x-axis is used as the
    *     "up" vector.
    * @example <caption>Rotate your view in independent mode to look at the world origin.</caption>
    * Camera.mode = "independent";
    * Camera.orientation = Quat.lookAtSimple(Camera.position, Vec3.ZERO);
    */
    glm::quat lookAtSimple(const glm::vec3& eye, const glm::vec3& center);

    /*@jsdoc
    * Calculates the shortest rotation from a first vector onto a second.
    * @function Quat(0).rotationBetween
    * @param {Vec3} v1 - The first vector.
    * @param {Vec3} v2 - The second vector.
    * @returns {Quat} The rotation from <code>v1</code> onto <code>v2</code>.
    * @example <caption>Apply a change in velocity to an entity and rotate it to face the direction it's travelling.</caption>
    * var newVelocity = Vec3.sum(entityVelocity, deltaVelocity);
    * var properties = { velocity: newVelocity };
    * if (Vec3.length(newVelocity) > 0.001) {
    *     properties.rotation = Quat.rotationBetween(entityVelocity, newVelocity);
    * }
    * Entities.editEntity(entityID, properties);
    * entityVelocity = newVelocity;
    */
    glm::quat rotationBetween(const glm::vec3& v1, const glm::vec3& v2);

    /*@jsdoc
     * Generates a quaternion from a {@link Vec3} of Euler angles in degrees.
     * @function Quat(0).fromVec3Degrees
     * @param {Vec3} vector - A vector of three Euler angles in degrees, the angles being the rotations about the x, y, and z
     *     axes.
     * @returns {Quat} A quaternion created from the Euler angles in <code>vector</code>.
     * @example <caption>Zero out pitch and roll from an orientation.</caption>
     * var eulerAngles = Quat.safeEulerAngles(orientation);
     * eulerAngles.x = 0;
     * eulerAngles.z = 0;
     * var newOrientation = Quat.fromVec3Degrees(eulerAngles);
     */
    glm::quat fromVec3Degrees(const glm::vec3& vec3);

    /*@jsdoc
     * Generates a quaternion from a {@link Vec3} of Euler angles in radians.
     * @function Quat(0).fromVec3Radians
     * @param {Vec3} vector - A vector of three Euler angles in radians, the angles being the rotations about the x, y, and z
     *     axes.
     * @returns {Quat} A quaternion created using the Euler angles in <code>vector</code>.
     * @example <caption>Create a rotation of 180 degrees about the y axis.</caption>
     * var rotation = Quat.fromVec3Radians({ x: 0, y: Math.PI, z: 0 });
     */
    glm::quat fromVec3Radians(const glm::vec3& vec3);

    /*@jsdoc
    * Generates a quaternion from pitch, yaw, and roll values in degrees.
    * @function Quat(0).fromPitchYawRollDegrees
    * @param {number} pitch - The pitch angle in degrees.
    * @param {number} yaw - The yaw angle in degrees.
    * @param {number} roll - The roll angle in degrees.
    * @returns {Quat} A quaternion created using the <code>pitch</code>, <code>yaw</code>, and <code>roll</code> Euler angles.
    * @example <caption>Create a rotation of 180 degrees about the y axis.</caption>
    * var rotation = Quat.fromPitchYawRollDegrees(0, 180, 0 );
    */
    glm::quat fromPitchYawRollDegrees(float pitch, float yaw, float roll);

    /*@jsdoc
    * Generates a quaternion from pitch, yaw, and roll values in radians.
    * @function Quat(0).fromPitchYawRollRadians
    * @param {number} pitch - The pitch angle in radians.
    * @param {number} yaw - The yaw angle in radians.
    * @param {number} roll - The roll angle in radians.
    * @returns {Quat} A quaternion created from the <code>pitch</code>, <code>yaw</code>, and <code>roll</code> Euler angles.
    * @example <caption>Create a rotation of 180 degrees about the y axis.</caption>
    * var rotation = Quat.fromPitchYawRollRadians(0, Math.PI, 0);
    */
    glm::quat fromPitchYawRollRadians(float pitch, float yaw, float roll);

    /*@jsdoc
     * Calculates the inverse of a quaternion. For a unit quaternion, its inverse is the same as its
     *     {@link Quat(0).conjugate|Quat.conjugate}.
     * @function Quat(0).inverse
     * @param {Quat} q - The quaternion.
     * @returns {Quat} The inverse of <code>q</code>.
     * @example <caption>A quaternion multiplied by its inverse is a zero rotation.</caption>
     * var quaternion = Quat.fromPitchYawRollDegrees(10, 20, 30);
     * Quat.print("quaternion", quaternion, true); // dvec3(10.000000, 20.000004, 30.000004)
     * var inverse = Quat.invserse(quaternion);
     * Quat.print("inverse", inverse, true); // dvec3(1.116056, -22.242186, -28.451778)
     * var identity = Quat.multiply(inverse, quaternion);
     * Quat.print("identity", identity, true); // dvec3(0.000000, 0.000000, 0.000000)
     */
    glm::quat inverse(const glm::quat& q);

    /*@jsdoc
     * Gets the "front" direction that the camera would face if its orientation was set to the quaternion value.
     * This is a synonym for {@link Quat(0).getForward|Quat.getForward}.
     * The Vircadia camera has axes <code>x</code> = right, <code>y</code> = up, <code>-z</code> = forward.
     * @function Quat(0).getFront
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Vec3} The negative z-axis rotated by <code>orientation</code>.
     */
    glm::vec3 getFront(const glm::quat& orientation) { return getForward(orientation); }

    /*@jsdoc
     * Gets the "forward" direction that the camera would face if its orientation was set to the quaternion value.
     * This is a synonym for {@link Quat(0).getFront|Quat.getFront}.
     * The Vircadia camera has axes <code>x</code> = right, <code>y</code> = up, <code>-z</code> = forward.
     * @function Quat(0).getForward
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Vec3} The negative z-axis rotated by <code>orientation</code>.
     * @example <caption>Demonstrate that the "forward" vector is for the negative z-axis.</caption>
     * var forward = Quat.getForward(Quat.IDENTITY);
     * print(JSON.stringify(forward)); // {"x":0,"y":0,"z":-1}
     */
    glm::vec3 getForward(const glm::quat& orientation);

    /*@jsdoc
     * Gets the "right" direction that the camera would have if its orientation was set to the quaternion value.
     * The Vircadia camera has axes <code>x</code> = right, <code>y</code> = up, <code>-z</code> = forward.
     * @function Quat(0).getRight
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Vec3} The x-axis rotated by <code>orientation</code>.
     */
    glm::vec3 getRight(const glm::quat& orientation);

    /*@jsdoc
     * Gets the "up" direction that the camera would have if its orientation was set to the quaternion value.
     * The Vircadia camera has axes <code>x</code> = right, <code>y</code> = up, <code>-z</code> = forward.
     * @function Quat(0).getUp
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Vec3} The y-axis rotated by <code>orientation</code>.
     */
    glm::vec3 getUp(const glm::quat& orientation);

    /*@jsdoc
     * Calculates the Euler angles for the quaternion, in degrees. (The "safe" in the name signifies that the angle results 
     * will not be garbage even when the rotation is particularly difficult to decompose with pitches around +/-90 degrees.)
     * @function Quat(0).safeEulerAngles
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Vec3} A {@link Vec3} of Euler angles for the <code>orientation</code>, in degrees, the angles being the 
     * rotations about the x, y, and z axes.
     * @example <caption>Report the camera yaw.</caption>
     * var eulerAngles = Quat.safeEulerAngles(Camera.orientation);
     * print("Camera yaw: " + eulerAngles.y);
     */
    glm::vec3 safeEulerAngles(const glm::quat& orientation);

    /*@jsdoc
     * Generates a quaternion given an angle to rotate through and an axis to rotate about.
     * @function Quat(0).angleAxis
     * @param {number} angle - The angle to rotate through, in degrees.
     * @param {Vec3} axis - The unit axis to rotate about.
     * @returns {Quat} A quaternion that is a rotation through <code>angle</code> degrees about the <code>axis</code>. 
     * <strong>WARNING:</strong> This value is in degrees whereas the value returned by {@link Quat(0).angle|Quat.angle} is
     * in radians.
     * @example <caption>Calculate a rotation of 90 degrees about the direction your camera is looking.</caption>
     * var rotation = Quat.angleAxis(90, Quat.getForward(Camera.orientation));
     */
    glm::quat angleAxis(float angle, const glm::vec3& v);

    /*@jsdoc
     * Gets the rotation axis for a quaternion.
     * @function Quat(0).axis
     * @param {Quat} q - The quaternion.
     * @returns {Vec3} The normalized rotation axis for <code>q</code>.
     * @example <caption>Get the rotation axis of a quaternion.</caption>
     * var forward = Quat.getForward(Camera.orientation);
     * var rotation = Quat.angleAxis(90, forward);
     * var axis = Quat.axis(rotation);
     * print("Forward: " + JSON.stringify(forward));
     * print("Axis: " + JSON.stringify(axis)); // Same value as forward.
     */
    glm::vec3 axis(const glm::quat& orientation);

    /*@jsdoc
     * Gets the rotation angle for a quaternion.
     * @function Quat(0).angle
     * @param {Quat} q - The quaternion.
     * @returns {number} The rotation angle for <code>q</code>, in radians. <strong>WARNING:</strong> This value is in radians 
     * whereas the value used by {@link Quat(0).angleAxis|Quat.angleAxis} is in degrees.
     * @example <caption>Get the rotation angle of a quaternion.</caption>
     * var forward = Quat.getForward(Camera.orientation);
     * var rotation = Quat.angleAxis(90, forward);
     * var angle = Quat.angle(rotation);
     * print("Angle: " + angle * 180 / Math.PI);  // 90 degrees.
     */
    float angle(const glm::quat& orientation);

    // spherical linear interpolation
    // alpha: 0.0 to 1.0?
    /*@jsdoc
     * Computes a spherical linear interpolation between two rotations, safely handling two rotations that are very similar.
     * See also, {@link Quat(0).slerp|Quat.slerp}.
     * @function Quat(0).mix
     * @param {Quat} q1 - The beginning rotation.
     * @param {Quat} q2 - The ending rotation.
     * @param {number} alpha - The mixture coefficient between <code>0.0</code> and <code>1.0</code>. Specifies the proportion
     *     of <code>q2</code>'s value to return in favor of <code>q1</code>'s value. A value of <code>0.0</code> returns 
     *     <code>q1</code>'s value; <code>1.0</code> returns <code>q2s</code>'s value.
     * @returns {Quat} A spherical linear interpolation between rotations <code>q1</code> and <code>q2</code>.
     * @example <caption>Animate between one rotation and another.</caption>
     * var dt = amountOfTimeThatHasPassed;
     * var mixFactor = amountOfTimeThatHasPassed / TIME_TO_COMPLETE;
     * if (mixFactor > 1) {
     *     mixFactor = 1;
     * }
     * var newRotation = Quat.mix(startRotation, endRotation, mixFactor);
     */
    glm::quat mix(const glm::quat& q1, const glm::quat& q2, float alpha);

    /*@jsdoc
     * Computes a spherical linear interpolation between two rotations, for rotations that are not very similar.
     * See also, {@link Quat(0).mix|Quat.mix}.
     * @function Quat(0).slerp
     * @param {Quat} q1 - The beginning rotation.
     * @param {Quat} q2 - The ending rotation.
     * @param {number} alpha - The mixture coefficient between <code>0.0</code> and <code>1.0</code>. Specifies the proportion
     *     of <code>q2</code>'s value to return in favor of <code>q1</code>'s value. A value of <code>0.0</code> returns
     *     <code>q1</code>'s value; <code>1.0</code> returns <code>q2s</code>'s value.
     * @returns {Quat} A spherical linear interpolation between rotations <code>q1</code> and <code>q2</code>.
     */
    glm::quat slerp(const glm::quat& q1, const glm::quat& q2, float alpha);

    /*@jsdoc
     * Computes a spherical quadrangle interpolation between two rotations along a path oriented toward two other rotations.
     * Equivalent to: <code>Quat.slerp(Quat.slerp(q1, q2, alpha), Quat.slerp(s1, s2, alpha), 2 * alpha * (1.0 - alpha))</code>.
     * @function Quat(0).squad
     * @param {Quat} q1 - Initial rotation.
     * @param {Quat} q2 - Final rotation.
     * @param {Quat} s1 - First control point.
     * @param {Quat} s2 - Second control point.
     * @param {number} alpha - The mixture coefficient between <code>0.0</code> and <code>1.0</code>. A value of 
     *     <code>0.0</code> returns <code>q1</code>'s value; <code>1.0</code> returns <code>q2s</code>'s value.
     * @returns {Quat} A spherical quadrangle interpolation between rotations <code>q1</code> and <code>q2</code> using control
     *     points <code>s1</code> and <code>s2</code>.
     */
    glm::quat squad(const glm::quat& q1, const glm::quat& q2, const glm::quat& s1, const glm::quat& s2, float h);

    /*@jsdoc
     * Calculates the dot product of two quaternions. The closer the quaternions are to each other the more non-zero the value 
     * is (either positive or negative). Identical unit rotations have a dot product of +/-1.
     * @function Quat(0).dot
     * @param {Quat} q1 - The first quaternion.
     * @param {Quat} q2 - The second quaternion.
     * @returns {number} The dot product of <code>q1</code> and <code>q2</code>.
     * @example <caption>Testing unit quaternions for equality.</caption>
     * var q1 = Quat.fromPitchYawRollDegrees(0, 0, 0);
     * var q2 = Quat.fromPitchYawRollDegrees(0, 0, 0);
     * print(Quat.equal(q1, q2)); // true
     * var q3 = Quat.fromPitchYawRollDegrees(0, 0, 359.95);
     * print(Quat.equal(q1, q3)); // false
     *
     * var dot = Quat.dot(q1, q3);
     * print(dot); // -0.9999999403953552
     * var equal = Math.abs(1 - Math.abs(dot)) < 0.000001;
     * print(equal); // true
     */
    float dot(const glm::quat& q1, const glm::quat& q2);
    
    /*@jsdoc
     * Prints to the program log a text label followed by a quaternion's pitch, yaw, and roll Euler angles.
     * @function Quat(0).print
     * @param {string} label - The label to print.
     * @param {Quat} q - The quaternion to print.
     * @param {boolean} [asDegrees=false] - If <code>true</code> the angle values are printed in degrees, otherwise they are
     *     printed in radians.
     * @example <caption>Two ways of printing a label plus a quaternion's Euler angles.</caption>
     * var quaternion = Quat.fromPitchYawRollDegrees(0, 45, 0);
     *
     * // Quaternion: dvec3(0.000000, 45.000004, 0.000000)
     * Quat.print("Quaternion:", quaternion,  true);
     *
     * // Quaternion: {"x":0,"y":45.000003814697266,"z":0}
     * print("Quaternion: " + JSON.stringify(Quat.safeEulerAngles(quaternion)));
     */
    void print(const QString& label, const glm::quat& q, bool asDegrees = false);

    /*@jsdoc
     * Tests whether two quaternions are equal.
     * <p><strong>Note:</strong> The quaternions must be exactly equal in order for <code>true</code> to be returned; it is 
     * often better to use {@link Quat(0).dot|Quat.dot} and test for closeness to +/-1.</p>
     * @function Quat(0).equal
     * @param {Quat} q1 - The first quaternion.
     * @param {Quat} q2 - The second quaternion.
     * @returns {boolean} <code>true</code> if the quaternions are equal, otherwise <code>false</code>.
     * @example <caption>Testing unit quaternions for equality.</caption>
     * var q1 = Quat.fromPitchYawRollDegrees(0, 0, 0);
     * var q2 = Quat.fromPitchYawRollDegrees(0, 0, 0);
     * print(Quat.equal(q1, q2)); // true
     * var q3 = Quat.fromPitchYawRollDegrees(0, 0, 359.95);
     * print(Quat.equal(q1, q3)); // false
     *
     * var dot = Quat.dot(q1, q3);
     * print(dot); // -0.9999999403953552
     * var equal = Math.abs(1 - Math.abs(dot)) < 0.000001;
     * print(equal); // true
     */
    bool equal(const glm::quat& q1, const glm::quat& q2);

    /*@jsdoc
     * Cancels out the roll and pitch component of a quaternion so that its completely horizontal with a yaw pointing in the 
     * given quaternion's direction.
     * @function Quat(0).cancelOutRollAndPitch
     * @param {Quat} orientation - A quaternion representing an orientation.
     * @returns {Quat}  <code>orientation</code> with its roll and pitch canceled out.
     * @example <caption>Two ways of calculating a camera orientation in the x-z plane with a yaw pointing in the direction of
     *     a given quaternion.</caption>
     * var quaternion = Quat.fromPitchYawRollDegrees(10, 20, 30);
     *
     * var noRollOrPitch = Quat.cancelOutRollAndPitch(quaternion);
     * Quat.print("", noRollOrPitch, true); // dvec3(0.000000, 22.245995, 0.000000)
     *
     * var front = Quat.getFront(quaternion);
     * var lookAt = Quat.lookAtSimple(Vec3.ZERO, { x: front.x, y: 0, z: front.z });
     * Quat.print("", lookAt, true); // dvec3(0.000000, 22.245996, 0.000000)
     *
     */
    glm::quat cancelOutRollAndPitch(const glm::quat& q);

    /*@jsdoc
    * Cancels out the roll component of a quaternion so that its horizontal axis is level.
    * @function Quat(0).cancelOutRoll
    * @param {Quat} orientation - A quaternion representing an orientation.
    * @returns {Quat} <code>orientation</code> with its roll canceled out.
    * @example <caption>Two ways of calculating a camera orientation that points in the direction of a given quaternion but
    *     keeps the camera's horizontal axis level.</caption>
    * var quaternion = Quat.fromPitchYawRollDegrees(10, 20, 30);
    *
    * var noRoll = Quat.cancelOutRoll(quaternion);
    * Quat.print("", noRoll, true); // dvec3(-1.033004, 22.245996, -0.000000)
    *
    * var front = Quat.getFront(quaternion);
    * var lookAt = Quat.lookAtSimple(Vec3.ZERO, front);
    * Quat.print("", lookAt, true); // dvec3(-1.033004, 22.245996, -0.000000)
    */
    glm::quat cancelOutRoll(const glm::quat& q);

private:
    const glm::quat& IDENTITY() const { return Quaternions::IDENTITY; }

};

#endif // hifi_Quat_h
