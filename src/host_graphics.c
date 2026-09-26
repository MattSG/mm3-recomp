#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <d3d/d3d8_xbox.h>

#include "host_graphics.h"

static IDirect3D8 *g_mm3_d3d;
static IDirect3DDevice8 *g_mm3_device;

static LRESULT CALLBACK mm3_graphics_wndproc(HWND window, UINT message,
                                              WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CLOSE || message == WM_DESTROY)
        return 0;
    return DefWindowProcA(window, message, wparam, lparam);
}

int mm3_graphics_init(void)
{
    WNDCLASSA window_class;
    D3DPRESENT_PARAMETERS present;
    HWND window;
    HRESULT hr;

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = mm3_graphics_wndproc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "MM3RecompGraphics";
    RegisterClassA(&window_class);

    window = CreateWindowExA(0, window_class.lpszClassName, "MM3 Recomp",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             640, 480, NULL, NULL, window_class.hInstance, NULL);
    if (!window) {
        fprintf(stderr, "graphics: window creation failed\n");
        return 0;
    }

    g_mm3_d3d = xbox_Direct3DCreate8(0);
    if (!g_mm3_d3d) {
        fprintf(stderr, "graphics: Direct3DCreate8 failed\n");
        return 0;
    }

    memset(&present, 0, sizeof(present));
    present.BackBufferWidth = 640;
    present.BackBufferHeight = 480;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.BackBufferCount = 1;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.Windowed = TRUE;
    present.EnableAutoDepthStencil = TRUE;
    present.AutoDepthStencilFormat = D3DFMT_D24S8;

    hr = g_mm3_d3d->lpVtbl->CreateDevice(g_mm3_d3d, 0, 0, window, 0,
                                          &present, &g_mm3_device);
    if (FAILED(hr) || !g_mm3_device) {
        fprintf(stderr, "graphics: CreateDevice failed: 0x%08lX\n",
                (unsigned long)hr);
        return 0;
    }

    fprintf(stderr, "graphics: D3D8/D3D11 device initialized\n");
    return 1;
}
