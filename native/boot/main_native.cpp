// Native Windows "boot" - platform-layer scaffold only.
//
// This is NOT the decompiled game booting. It's the Win32 window + Direct3D
// 11 device/swapchain init and render loop that a real Phase 2 port would
// need as its foundation - the native equivalent of what GX/OS did on
// GameCube (create a display surface, get a frame buffer, start a loop).
// Nothing from the actual RE4 engine runs here yet because nothing beyond
// ~115 trivial logic functions (see config/progress.yaml) is decompiled -
// there is no ported systemStartInit()/main()/renderer/asset loader to call
// into. What this proves: the native platform layer itself - window
// creation, D3D11 device/swapchain, a real frame loop - works, which is a
// real (if small) piece of what Phase 2 will eventually need.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swapChain = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static bool CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &g_swapChain, &g_device, &featureLevel, &g_context);
    if (FAILED(hr)) {
        std::printf("D3D11CreateDeviceAndSwapChain failed: 0x%08lX\n", hr);
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
    backBuffer->Release();

    std::printf("Direct3D 11 device created. Feature level: 0x%04X\n", featureLevel);
    return true;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    std::printf("RE4 native port - platform bootstrap (NOT the decompiled game)\n");
    std::printf("See native/boot/main_native.cpp for exactly what this is.\n\n");

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RE4NativeBootWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, "RE4NativeBootWindow",
        "RESIDENT EVIL 4 - Native Port (Phase 2 platform bootstrap, not the decompiled game)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 856, 682,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        std::printf("CreateWindowEx failed\n");
        return 1;
    }

    if (!CreateDeviceAndSwapChain(hwnd)) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    std::printf("Window shown. Entering render loop.\n");

    const float clearColor[4] = {0.05f, 0.05f, 0.08f, 1.0f};
    MSG msg = {};
    bool running = true;
    int frame = 0;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clearColor);
        g_swapChain->Present(1, 0);

        frame++;
        if (frame == 1) {
            std::printf("First frame presented. Native boot reached steady state.\n");
        }
    }

    std::printf("Shutting down after %d frames.\n", frame);
    if (g_rtv) g_rtv->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    return 0;
}
