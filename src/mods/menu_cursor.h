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

static HCURSOR create_menu_cursor(void)
{
    const POINT outline[] = {{1,1},{2,30},{10,24},{17,37},{24,33},{17,21},{29,19}};
    const POINT face[] = {{4,6},{5,26},{11,20},{18,34},{21,32},{14,19},{25,18}};
    const POINT gold[] = {{7,12},{8,21},{12,17},{20,17}};
    const POINT orange[] = {{13,22},{18,31},{20,30},{15,21}};
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = 32; info.bmiHeader.biHeight = -40;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    uint32_t *pixels = NULL;
    HBITMAP color = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    unsigned char mask_bits[160] = {0};
    HBITMAP mask = CreateBitmap(32, 40, 1, 1, mask_bits);
    HCURSOR cursor = NULL;
    if (color && mask) {
        for (int y = 0; y < 40; ++y) for (int x = 0; x < 32; ++x) {
            unsigned a = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
                float px = x + (sx+.5f)/4, py = y + (sy+.5f)/4;
                if (!cursor_inside(px, py, outline, 7)) continue;
                unsigned cr = 51, cg = 22, cb = 5;
                if (cursor_inside(px, py, face, 7)) {
                    cr = 255; cg = 190-y*3; cb = 35;
                }
                if (cursor_inside(px, py, gold, 4)) { cr = 255; cg = 219; cb = 93; }
                if (cursor_inside(px, py, orange, 4)) { cr = 255; cg = 155; cb = 25; }
                a += 255; r += cr; g += cg; b += cb;
            }
            pixels[y*32+x] = (a/16<<24) | (r/16<<16) | (g/16<<8) | b/16;
        }
        ICONINFO icon = {0};
        icon.xHotspot = icon.yHotspot = 2;
        icon.hbmColor = color; icon.hbmMask = mask;
        cursor = (HCURSOR)CreateIconIndirect(&icon);
    }
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    return cursor;
}

/* Keep the familiar Windows hand silhouette, coloured to match the arrow. */
static HCURSOR create_menu_hand(void)
{
    HCURSOR source = LoadCursorA(NULL, IDC_HAND), hand = NULL;
    ICONINFO original = {0};
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = 32; info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    uint32_t *pixels = NULL;
    HBITMAP color = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    unsigned char mask_bits[128] = {0}, opaque[1024];
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, mask_bits);
    HDC dc = CreateCompatibleDC(NULL);
    if (source && color && mask && dc && GetIconInfo(source, &original)) {
        HGDIOBJ previous = SelectObject(dc, color);
        memset(pixels, 255, 32*32*4);
        DrawIconEx(dc, 0, 0, source, 32, 32, 0, NULL, DI_MASK);
        GdiFlush();
        for (unsigned i = 0; i < 1024; ++i) opaque[i] = !(pixels[i] & 0xFFFFFF);
        memset(pixels, 0, 32*32*4);
        DrawIconEx(dc, 0, 0, source, 32, 32, 0, NULL, DI_NORMAL);
        GdiFlush();
        for (unsigned i = 0; i < 1024; ++i)
            pixels[i] = !opaque[i] ? 0 : (pixels[i] & 0xFFFFFF) > 0x808080 ? 0xFFFFBB35 : 0xFF331605;
        BITMAP size;
        GetObject(original.hbmMask, sizeof size, &size);
        ICONINFO icon = {0};
        icon.xHotspot = original.xHotspot*32/size.bmWidth;
        icon.yHotspot = original.yHotspot*32/(original.hbmColor ? size.bmHeight : size.bmHeight/2);
        icon.hbmColor = color; icon.hbmMask = mask;
        hand = (HCURSOR)CreateIconIndirect(&icon);
        SelectObject(dc, previous);
    }
    if (original.hbmColor) DeleteObject(original.hbmColor);
    if (original.hbmMask) DeleteObject(original.hbmMask);
    if (dc) DeleteDC(dc);
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    return hand;
}
