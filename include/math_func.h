#include <math.h>  
#include <algorithm>

#ifndef __MATH_FUNC_H__
#define __MATH_FUNC_H__

float degToRad(float deg);

float smoothstep(float edge0, float edge1, float x);

float smootherstep(float edge0, float edge1, float x);

float clamp(float x, float lowerlimit = 0.0f, float upperlimit = 1.0f);

#endif