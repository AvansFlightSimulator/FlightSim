#ifndef CALCULATE_LEGS_H
#define CALCULATE_LEGS_H

#include <array>
#include <cmath>
#include <vector>

const int CALCULATE_LEGS_SIZE = 3;

struct vec
{
    float x = 0;
    float y = 0;
    float z = 0;

    vec operator+(const vec& other) const;
    vec operator-(const vec& other) const;
    float magnitude();
};

vec dot_product(std::array<std::array<float, 3>, 3>& matrix, const vec& v);
vec compute_li_vector(vec& T, float psi, float theta, float phi, vec& p_i, vec& b_i);
float compute_li_length(vec& T, float psi, float theta, float phi, vec& p_i, vec& b_i);
vec dot_product(float matrix[CALCULATE_LEGS_SIZE][CALCULATE_LEGS_SIZE], vec& v);
std::array<std::array<float, 3>, 3> rotation_matrix(float psi, float theta, float phi);
vec rad(float psi, float theta, float phi);

#endif
