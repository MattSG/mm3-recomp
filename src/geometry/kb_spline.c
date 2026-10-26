#include "kb_spline.h"

KbPoint kb_segment_point(const KbPoint p[4], float u, float bias, float tension, float continuity)
{
    float h00, h10, h01, h11, out0, out1, in0, in1, factor = .5f * (1 - tension);
    KbPoint result;
    out0 = factor * (1 + bias) * (1 + continuity);
    out1 = factor * (1 - bias) * (1 - continuity);
    in0 = factor * (1 + bias) * (1 - continuity);
    in1 = factor * (1 - bias) * (1 + continuity);
    h00 = 2*u*u*u - 3*u*u + 1; h10 = u*u*u - 2*u*u + u;
    h01 = -2*u*u*u + 3*u*u; h11 = u*u*u - u*u;
#define KB_COMPONENT(axis) \
    result.axis = h00*p[1].axis + h10*(out0*(p[1].axis-p[0].axis) + out1*(p[2].axis-p[1].axis)) + \
                  h01*p[2].axis + h11*(in0*(p[2].axis-p[1].axis) + in1*(p[3].axis-p[2].axis))
    KB_COMPONENT(x); KB_COMPONENT(y); KB_COMPONENT(z);
#undef KB_COMPONENT
    return result;
}
