#include "camera_math.h"
#include <math.h>

float xbox_fovy_from_horizontal_degrees(float degrees, float aspect)
{
    return 2.0f * atanf(tanf(degrees * (3.14159265358979323846f / 360.0f)) / aspect);
}
