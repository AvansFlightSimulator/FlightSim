#include "calculate_legs.h"

#include <cmath>

namespace {
constexpr float Pi = 3.14159265358979323846f;

// psi, theta, phi are the conventional Euler angles about Z, Y, X.
vec AnglesInRadians(float psi, float theta, float phi) {
    return { psi * (Pi / 180.0f), theta * (Pi / 180.0f), phi * (Pi / 180.0f) };
}
}

vec vec::operator+(const vec& other) const {
    vec result;
    result.x = this->x + other.x;
    result.y = this->y + other.y;
    result.z = this->z + other.z;
    return result;
}

vec vec::operator-(const vec& other) const {
    vec result;
    result.x = this->x - other.x;
    result.y = this->y - other.y;
    result.z = this->z - other.z;
    return result;
}

float vec::magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

vec dot_product(const std::array<std::array<float, 3>, 3>& matrix, const vec& v) {
    vec result;
    result.x = matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z;
    result.y = matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z;
    result.z = matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z;
    return result;
}

// Rotation matrix Rz(psi) * Ry(theta) * Rx(phi), using degree inputs.
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

// l_i means the vector from base mounting point i to platform mounting point i.
vec compute_li_vector(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i) {
    std::array<std::array<float, 3>, 3> PRB = rotation_matrix(psi, theta, phi);
    // T = platform translation, PRB = rotation, p_i/b_i = mounting coordinates.
    vec l_i = T + dot_product(PRB, p_i) - b_i;
    return l_i;
}

// Actuator length is the magnitude of its base-to-platform vector.
float compute_li_length(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i) {
    vec l_i = compute_li_vector(T, psi, theta, phi, p_i, b_i);
    return l_i.magnitude();
}