/* Native ARGB cursor: MM3's orange and gold menu palette. */
static int cursor_inside(float x, float y, const POINT *p, unsigned n)
{
    int inside = 0;
    for (unsigned i = 0, j = n-1; i < n; j = i++)
        if ((p[i].y > y) != (p[j].y > y) &&
            x < (p[j].x-p[i].x)*(y-p[i].y)/(p[j].y-p[i].y)+p[i].x)
            inside = !inside;
    return inside;
}

/* SDL3: the same arrow pixels as a colour cursor. The hand is the system
 * pointer; the Win32 build recolours the Windows hand instead. */
static XboxCursor create_menu_cursor(void)
{
    const POINT outline[] = {{1,1},{2,30},{10,24},{17,37},{24,33},{17,21},{29,19}};
    const POINT face[] = {{4,6},{5,26},{11,20},{18,34},{21,32},{14,19},{25,18}};
    const POINT gold[] = {{7,12},{8,21},{12,17},{20,17}};
    const POINT orange[] = {{13,22},{18,31},{20,30},{15,21}};
    XboxCursor cursor = NULL;
    SDL_Surface *surface = SDL_CreateSurface(32, 40, SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        for (int y = 0; y < 40; ++y) {
            uint32_t *row = (uint32_t *)((uint8_t *)surface->pixels + (size_t)y*surface->pitch);
            for (int x = 0; x < 32; ++x) {
                unsigned a = 0, n = 0, r = 0, g = 0, b = 0;
                for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
                    float px = x + (sx+.5f)/4, py = y + (sy+.5f)/4;
                    if (!cursor_inside(px, py, outline, 7)) continue;
                    unsigned cr = 51, cg = 22, cb = 5;
                    if (cursor_inside(px, py, face, 7)) {
                        cr = 255; cg = 190-y*3; cb = 35;
                    }
                    if (cursor_inside(px, py, gold, 4)) { cr = 255; cg = 219; cb = 93; }
                    if (cursor_inside(px, py, orange, 4)) { cr = 255; cg = 155; cb = 25; }
                    a += 255; n++; r += cr; g += cg; b += cb;
                }
                /* Straight alpha: the colour is the mean of the covered samples, not
                 * their sum over 16 (which would darken the anti-aliased edges). */
                row[x] = n ? (a/16<<24) | (r/n<<16) | (g/n<<8) | b/n : 0;
            }
        }
        cursor = SDL_CreateColorCursor(surface, 2, 2);
        SDL_DestroySurface(surface);
    }
    if (!cursor) cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    return cursor;
}

static XboxCursor create_menu_hand(void)
{
    return SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
}
