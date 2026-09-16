#ifndef CALCULATE_LEGS_H
#define CALCULATE_LEGS_H

#include <array>

struct vec
{
    float x = 0;
    float y = 0;
    float z = 0;

    vec operator+(const vec& other) const;
    vec operator-(const vec& other) const;
    float magnitude() const;
};

vec dot_product(const std::array<std::array<float, 3>, 3>& matrix, const vec& v);
// Leg vector = translation + rotated platform mounting point - base mounting point.
// Angles psi/theta/phi are degrees about Z/Y/X respectively.
vec compute_li_vector(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i);
float compute_li_length(const vec& T, float psi, float theta, float phi, const vec& p_i, const vec& b_i);
// Euler rotation in Z-Y-X order. Callers choose how aircraft axes map to it.
std::array<std::array<float, 3>, 3> rotation_matrix(float psi, float theta, float phi);

#endif
