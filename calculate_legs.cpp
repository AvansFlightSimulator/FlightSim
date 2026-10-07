#include "calculate_legs.h"

#include <cmath>

namespace {
	/// Constant for Pi value to convert degrees to radians
    constexpr float Pi = 3.14159265358979323846f;

    /**   "psi" = Yaw angle in degrees(rotation about Z - axis)
    *     "theta" = Pitch angle in degrees (rotation about Y-axis)
    *     "phi" = Roll angle in degrees (rotation about X-axis)
    */
    vec AnglesInRadians(float psi, float theta, float phi) {
        return { psi * (Pi / 180.0f), theta * (Pi / 180.0f), phi * (Pi / 180.0f) };
    }
}
/**
* Overloaded addition operator for vector addition
* 
* Performs component-wise addition of two 3D vectors. This operator allows
* vector objects to be added using the + operator syntax.
* 
* Other The vector to be added to this vector. Passed as a const reference
* to avoid unnecessary copying.
* 
* A new vec object containing the sum of both vectors where:
*         - result.x = this->x + other.x
*         - result.y = this->y + other.y
*         - result.z = this->z + other.z
* 
* This operator is const-qualified, meaning it does not modify the current
*       vector object. The operation creates and returns a new vector instance.
* 
* Here is an example:
*       vec v1 = {1.0f, 2.0f, 3.0f};
*       vec v2 = {4.0f, 5.0f, 6.0f};
*       vec v3 = v1 + v2;  // v3 = {5.0f, 7.0f, 9.0f}
*/
vec vec::operator+(const vec& other) const {
    vec result;
    result.x = this->x + other.x;
    result.y = this->y + other.y;
    result.z = this->z + other.z;
    return result;
}

/// same as above but for subtraction
vec vec::operator-(const vec& other) const {
    vec result;
    result.x = this->x - other.x;
    result.y = this->y - other.y;
    result.z = this->z - other.z;
    return result;
}

/// calculate magnitude of vector
float vec::magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

/**
* Performs matrix-vector multiplication with a 3x3 matrix and 3D vector.
*
* Multiplies a 3x3 transformation matrix by a 3D vector to produce a new 3D vector.
* This is commonly used in graphics and physics calculations for transformations such as
* rotations, scaling, or general linear transformations.
*
* matrix A constant reference to a 3x3 matrix represented as std::array<std::array<float, 3>, 3>.
* The matrix is indexed as matrix[row][column].
* v A constant reference to the 3D vector to be transformed, containing x, y, and z components.
*
* A new vec object containing the result of the matrix-vector multiplication.
*
* This function assumes both the matrix and vector are properly initialized.
* The operation is computed as: result = matrix * v, where each component of the result
* is the dot product of the corresponding row with the input vector.
*/
vec dot_product(const std::array<std::array<float, 3>, 3>& matrix, const vec& v) {
    vec result;
    result.x = matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z;
    result.y = matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z;
    result.z = matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z;
    return result;
}

/**
* Constructs a 3x3 rotation matrix from Euler angles using the ZYX rotation sequence.
* 
* The rotation matrix is computed as: Rz(psi) * Ry(theta) * Rx(phi)
* This represents a composite rotation applied in the order: yaw (psi) ? pitch (theta) ? roll (phi).
* 
* A 3x3 rotation matrix (PRB) where:
* - Row/Column indices follow [row][column] convention
* - PRB[i][j] represents the element in the i-th row and j-th column
* The matrix transforms vectors from the body frame to the reference frame.
* 
* Input angles are expected in degrees and are internally converted to radians.
* The matrix uses standard aerospace conventions for Euler angle rotations.
*/
std::array<std::array<float, 3>, 3> rotation_matrix(float psi, float theta, float phi) {
    std::array<std::array<float, 3>, 3> PRB;
    const vec rotation_rad = AnglesInRadians(psi, theta, phi);

    float cos_psi = std::cos(rotation_rad.x);
    float sin_psi = std::sin(rotation_rad.x);
    float cos_theta = std::cos(rotation_rad.y);
    float sin_theta = std::sin(rotation_rad.y);
    float cos_phi = std::cos(rotation_rad.z);
    float sin_phi = std::sin(rotation_rad.z);

    PRB[0][0] = cos_psi * cos_theta;
    PRB[0][1] = -sin_psi * cos_phi + cos_psi * sin_theta * sin_phi;
    PRB[0][2] = sin_psi * sin_phi + cos_psi * sin_theta * cos_phi;

    PRB[1][0] = sin_psi * cos_theta;
    PRB[1][1] = cos_psi * cos_phi + sin_psi * sin_theta * sin_phi;
    PRB[1][2] = -cos_psi * sin_phi + sin_psi * sin_theta * cos_phi;

    PRB[2][0] = -sin_theta;
    PRB[2][1] = cos_theta * sin_phi;
    PRB[2][2] = cos_theta * cos_phi;

    return PRB;
}  

/**
 * Computes the vector from base mounting point i to platform mounting point i.
 *
 * This function calculates the vector l_i that represents the displacement from a base
 * mounting point to the corresponding platform mounting point, accounting for platform
 * translation, rotation, and the relative positions of both mounting points.
 *
 * T The translation vector of the platform relative to the base coordinate system.
 * psi The yaw angle (rotation about Z-axis) in radians.
 * theta The pitch angle (rotation about Y-axis) in radians.
 * phi The roll angle (rotation about X-axis) in radians.
 * p_i The coordinates of mounting point i on the platform in platform-local coordinates.
 * b_i The coordinates of mounting point i on the base in base-fixed coordinates.
 *
 * vec The vector l_i from base mounting point i to platform mounting point i,
 *            expressed in the base coordinate system.
 *
 * The computation applies the rotation matrix to the platform coordinates,
 *       then combines with the translation offset and subtracts the base coordinates.
 *
 * Formula: l_i = T + R * p_i - b_i
 *   where R is the rotation matrix derived from (psi, theta, phi)
 */
vec compute_li_vector(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i) {
    std::array<std::array<float, 3>, 3> PRB = rotation_matrix(psi, theta, phi);
    // T = platform translation, PRB = rotation, p_i/b_i = mounting coordinates.
    vec l_i = T + dot_product(PRB, p_i) - b_i;
    return l_i;
}

/**
 * Computes the length of an actuator (leg) in a parallel robot platform.
 *
 * This function calculates the distance between a base mounting point and its corresponding
 * platform mounting point, accounting for platform position, orientation, and the relative
 * locations of both attachment points. The result represents the extended or contracted
 * length of the actuator connecting these two points.
 *
 * T The translation vector of the platform relative to the base coordinate system.
 * psi The yaw angle (rotation about Z-axis) in degrees.
 * theta The pitch angle (rotation about Y-axis) in degrees.
 * phi The roll angle (rotation about X-axis) in degrees.
 * p_i The coordinates of mounting point i on the platform in platform-local coordinates.
 * b_i The coordinates of mounting point i on the base in base-fixed coordinates.
 *
 * float The length of actuator i, calculated as the magnitude of the vector from
 * the base mounting point to the platform mounting point.
 *
 * The computation internally calls compute_li_vector() to obtain the displacement vector
 * l_i and returns its magnitude. The function follows the formula:
 * length = |l_i| = |T + R * p_i - b_i|
 * where R is the rotation matrix derived from the Euler angles (psi, theta, phi).
 *
 * compute_li_vector() For details on l_i vector calculation
 * rotation_matrix() For Euler angle rotation matrix computation
 *
 * Angles are provided in degrees and internally converted to radians.
 */
float compute_li_length(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i) {
    vec l_i = compute_li_vector(T, psi, theta, phi, p_i, b_i);
    return l_i.magnitude();
}