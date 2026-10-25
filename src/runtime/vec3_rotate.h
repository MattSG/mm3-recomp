/* Rotation of a 3D vector about the horizontal axis perpendicular to (dx, 0, dz).
 *
 * The axis is cross(up, (dx, 0, dz)) = (dz, 0, -dx), and the rotation is
 * Rodrigues' formula with cos = c and sin = s. out may not alias in. */
#ifndef VEC3_ROTATE_H
#define VEC3_ROTATE_H

void vec3_rotate_about_horizontal(float out[3], const float in[3], float dx, float dz,
                                  float c, float s);

#endif
