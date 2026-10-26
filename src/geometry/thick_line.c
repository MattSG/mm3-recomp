#include <math.h>
#include "thick_line.h"

int thick_line_quad(float xy[8], float x1, float y1, float x2, float y2, float width)
{
    float dx = x2 - x1, dy = y2 - y1, length = sqrtf(dx * dx + dy * dy);
    float nx, ny;
    if (length <= 0.0f) return 0;
    nx = dy * width * 0.5f / length; ny = -dx * width * 0.5f / length;
    xy[0] = x1 + nx; xy[1] = y1 + ny; xy[2] = x2 + nx; xy[3] = y2 + ny;
    xy[4] = x2 - nx; xy[5] = y2 - ny; xy[6] = x1 - nx; xy[7] = y1 - ny;
    return 1;
}
