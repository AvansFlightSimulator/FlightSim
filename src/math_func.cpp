#include "../include/math_func.h"
#include <cmath>

float degToRad(float deg) {
    return (M_PI / 180.0) * deg;
}

float smoothstep(float edge0, float edge1, float x) {
    // scale x to 0...1 range
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // smoothen
    return x * x * (3 - 2 * x);
}

float smootherstep(float edge0, float edge1, float x) {
    // scale x to 0...1 range
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // smoothen
    return x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
}

float clamp(float x, float lowerlimit, float upperlimit) {
  if (x < lowerlimit) return lowerlimit;
  if (x > upperlimit) return upperlimit;
  return x;
}