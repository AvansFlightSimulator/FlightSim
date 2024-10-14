#include "calculate_legs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Convert angles to radians
vec rad(float psi, float theta, float phi) {
    vec rad{ psi * ((float)M_PI / 180.0f), theta * ((float)M_PI / 180.0f), phi * ((float)M_PI / 180.0f) };
    return rad;
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

float vec::magnitude() {
    return std::sqrt(x * x + y * y + z * z);
}

vec dot_product(std::array<std::array<float, 3>, 3>& matrix, const vec& v) {
    vec result;
    result.x = matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z;
    result.y = matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z;
    result.z = matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z;
    return result;
}

// Function to compute the rotation matrix
std::array<std::array<float, 3>, 3> rotation_matrix(float psi, float theta, float phi) {
    std::array<std::array<float, 3>, 3> PRB;
    vec rotation_rad = rad(psi, theta, phi);

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

// Function to compute l_i
vec compute_li_vector(vec& T, float psi, float theta, float phi, vec& p_i, vec& b_i) {
    std::array<std::array<float, 3>, 3> PRB = rotation_matrix(psi, theta, phi);
    // Compute l_i as T + PRB * p_i - b_i
    vec l_i = T + dot_product(PRB, p_i) - b_i;
    return l_i;
}

// Function to compute the length of l_i
float compute_li_length(vec& T, float psi, float theta, float phi, vec& p_i, vec& b_i) {
    vec l_i = compute_li_vector(T, psi, theta, phi, p_i, b_i);
    return l_i.magnitude();
}