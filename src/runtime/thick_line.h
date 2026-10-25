/* The four corners of a segment drawn as a quad of a given width, for drawing
 * thick 2D lines as triangles. Corners are written as x, y pairs in order
 * around the quad. Returns 0 and writes nothing for a zero-length segment. */
#ifndef THICK_LINE_H
#define THICK_LINE_H

int thick_line_quad(float xy[8], float x1, float y1, float x2, float y2, float width);

#endif
