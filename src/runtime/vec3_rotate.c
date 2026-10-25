#include "vec3_rotate.h"

void vec3_rotate_about_horizontal(float out[3], const float in[3], float dx, float dz,
                                  float c, float s)
{
    float dot = dz * in[0] - dx * in[2];
    out[0] = c * in[0] + s * dx * in[1] + (1.0f - c) * dz * dot;
    out[1] = c * in[1] - s * (dx * in[0] + dz * in[2]);
    out[2] = c * in[2] + s * dz * in[1] - (1.0f - c) * dx * dot;
}
