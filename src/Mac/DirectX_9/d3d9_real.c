#ifdef GFX_REAL_D3D9

#    include <windows.h>
#    include <stdio.h>

typedef void *(__stdcall *PFN_Direct3DCreate9)(UINT SDKVersion);

void *RealD3D9_Direct3DCreate9(void)
{
    static HMODULE d3d9mod;
    static PFN_Direct3DCreate9 pCreate;
    void *iface;

    if (!pCreate) {
        d3d9mod = LoadLibraryA("d3d9.dll");
        if (!d3d9mod) {
            fprintf(stderr, "[real-d3d9] LoadLibrary(d3d9.dll) failed\n");
            return NULL;
        }
        pCreate = (PFN_Direct3DCreate9)GetProcAddress(d3d9mod, "Direct3DCreate9");
        if (!pCreate) {
            fprintf(stderr, "[real-d3d9] GetProcAddress(Direct3DCreate9) failed\n");
            return NULL;
        }
    }
    iface = pCreate(32 );
    fprintf(stderr, "[real-d3d9] Direct3DCreate9 -> %p\n", iface);
    return iface;
}

void *RealD3D9_CreateGameWindow(int w, int h)
{
    HINSTANCE hInst = GetModuleHandleA(NULL);
    WNDCLASSEXA wc;
    HWND hwnd;
    RECT r;
    const char *cls = "CoD2";

    if (getenv("REALD3D9_GENERIC_WND") || !GetClassInfoExA(hInst, "CoD2", &wc)) {
        WNDCLASSA gc;
        memset(&gc, 0, sizeof(gc));
        gc.lpfnWndProc = DefWindowProcA;
        gc.hInstance = hInst;
        gc.hCursor = LoadCursorA(NULL, (LPCSTR)0x7F00 );
        gc.lpszClassName = "CoD2_RealD3D9";
        RegisterClassA(&gc);
        cls = "CoD2_RealD3D9";
    }

    r.left = 0;
    r.top = 0;
    r.right = w;
    r.bottom = h;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd = CreateWindowExA(0, cls, "CoD2 (real D3D9)",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top,
                           NULL, NULL, hInst, NULL);

    fprintf(stderr, "[real-d3d9] game window cls='%s' hwnd=%p %dx%d\n", cls, (void *)hwnd, w, h);
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return (void *)hwnd;
}

void RealD3D9_FillPresentParams(void *ppOut, void *hwnd, int w, int h)
{
    unsigned char *p = (unsigned char *)ppOut;
    memset(p, 0, 56);
    *(unsigned int *)(p + 0) = (unsigned int)w;
    *(unsigned int *)(p + 4) = (unsigned int)h;
    *(unsigned int *)(p + 8) = 22;
    *(unsigned int *)(p + 12) = 1;

    *(unsigned int *)(p + 24) = 1;
    *(void **)(p + 28) = hwnd;
    *(int *)(p + 32) = 1;
    *(int *)(p + 36) = 1;
    *(unsigned int *)(p + 40) = 75;

    *(unsigned int *)(p + 52) = 0;
}

#endif
