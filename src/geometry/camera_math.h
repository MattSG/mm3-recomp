/* Projection helpers for a title that widens its horizontal field of view. */
#ifndef CAMERA_MATH_H
#define CAMERA_MATH_H

/* The vertical field of view (radians) that keeps the horizontal one (degrees)
 * at the given aspect ratio: fovy = 2 atan(tan(hfov / 2) / aspect). */
float xbox_fovy_from_horizontal_degrees(float degrees, float aspect);

#endif
