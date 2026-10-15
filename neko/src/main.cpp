// Demo host: Win32 + DX11 window running the Neko menu.
//
//   NekoDemo.exe                                 run interactively (Insert toggles the menu)
//   NekoDemo.exe --shot out.bmp [--tab N] [--sub N] [--scroll Y] [--click X Y] [--clickat N] [--frames N]
//                                                render a few frames with the menu at (0,0),
//                                                dump the backbuffer and exit
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "neko.h"

#include <d3d11.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdio>

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static bool                    g_SwapChainOccluded = false;
static UINT                    g_ResizeWidth = 0, g_ResizeHeight = 0;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// Saves the current backbuffer to a 32-bit BMP (used by --shot).
static void SaveBackbuffer(const char* path)
{
    ID3D11Texture2D* back = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back))) || !back) return;

    D3D11_TEXTURE2D_DESC desc{};
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &staging)) || !staging) {
        back->Release();
        return;
    }
    g_pd3dDeviceContext->CopyResource(staging, back);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(g_pd3dDeviceContext->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
        int w = (int)desc.Width, h = (int)desc.Height;
        int rowBytes = w * 4;

        BITMAPFILEHEADER fh{};
        BITMAPINFOHEADER ih{};
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + rowBytes * h;
        ih.biSize = sizeof(ih);
        ih.biWidth = w;
        ih.biHeight = -h;          // top-down
        ih.biPlanes = 1;
        ih.biBitCount = 32;
        ih.biCompression = BI_RGB;

        FILE* f = fopen(path, "wb");
        if (f) {
            fwrite(&fh, sizeof(fh), 1, f);
            fwrite(&ih, sizeof(ih), 1, f);
            std::vector<unsigned char> line(rowBytes);
            for (int y = 0; y < h; ++y) {
                const unsigned char* row = (const unsigned char*)m.pData + (size_t)y * m.RowPitch;
                for (int x = 0; x < w; ++x) {   // RGBA -> BGRA
                    line[x * 4 + 0] = row[x * 4 + 2];
                    line[x * 4 + 1] = row[x * 4 + 1];
                    line[x * 4 + 2] = row[x * 4 + 0];
                    line[x * 4 + 3] = row[x * 4 + 3];
                }
                fwrite(line.data(), 1, rowBytes, f);
            }
            fclose(f);
        }
        g_pd3dDeviceContext->Unmap(staging, 0);
    }
    staging->Release();
    back->Release();
}

static std::string Narrow(const std::wstring& w)
{
    std::string s(w.size(), '\0');
    for (size_t i = 0; i < w.size(); ++i) s[i] = (char)w[i];
    return s;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    ::SetProcessDPIAware();   // 1 ImGui pixel == 1 screen pixel

    std::string shotPath;
    int startTab = 0, startSub = 0, shotFrames = 90;
    float startScroll = 0.0f, clickX = -1.0f, clickY = -1.0f;
    int clickAt = 10;
    {
        int argc = 0;
        LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        for (int i = 1; i < argc; ++i) {
            std::wstring a(argv[i]);
            if (a == L"--shot" && i + 1 < argc)       shotPath = Narrow(argv[++i]);
            else if (a == L"--tab" && i + 1 < argc)   startTab = _wtoi(argv[++i]);
            else if (a == L"--sub" && i + 1 < argc)   startSub = _wtoi(argv[++i]);
            else if (a == L"--scroll" && i + 1 < argc) startScroll = (float)_wtof(argv[++i]);
            else if (a == L"--frames" && i + 1 < argc) shotFrames = _wtoi(argv[++i]);
            else if (a == L"--clickat" && i + 1 < argc) clickAt = _wtoi(argv[++i]);
            else if (a == L"--click" && i + 2 < argc) { clickX = (float)_wtof(argv[++i]); clickY = (float)_wtof(argv[++i]); }
        }
        if (argv) ::LocalFree(argv);
    }

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                       hInstance, nullptr, nullptr, nullptr, nullptr,
                       L"Neko", nullptr };
    ::RegisterClassExW(&wc);

    RECT rc = { 0, 0, 1280, 800 };
    ::AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Neko for Roblox - ImGui remake",
                                WS_OVERLAPPEDWINDOW, 100, 60, rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    neko::LoadFonts(NEKO_ASSET_DIR);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool menuOpen = true;
    const bool shotMode = !shotPath.empty();
    neko::SetDebugState(startTab, startSub, startScroll);

    const float clear[4] = { 0.09f, 0.10f, 0.12f, 1.0f };
    bool done = false;
    int frame = 0;

    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        // Screenshots run faster than real time, so pin the clock; the mouse
        // is parked off-menu unless --click places it.
        if (shotMode) {
            io.DeltaTime = 1.0f / 60.0f;
            if (clickX < 0.0f) io.AddMousePosEvent(-10000.0f, -10000.0f);
        }
        // --click: move the mouse there, press on frame --clickat, release on the next.
        if (shotMode && clickX >= 0.0f) {
            io.AddMousePosEvent(clickX, clickY);
            if (frame == clickAt)     io.AddMouseButtonEvent(0, true);
            if (frame == clickAt + 1) io.AddMouseButtonEvent(0, false);
        }
        ImGui::NewFrame();

        if (ImGui::IsKeyPressed(ImGuiKey_Insert, false)) menuOpen = !menuOpen;

        // Screenshots pin the menu to the top-left corner.
        if (shotMode) ImGui::SetNextWindowPos(ImVec2(0, 0));
        else          ImGui::SetNextWindowPos(ImVec2(110, 55), ImGuiCond_FirstUseEver);
        neko::Render(menuOpen);

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

        if (!shotPath.empty() && ++frame >= shotFrames) {
            SaveBackbuffer(shotPath.c_str());
            done = true;
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

// ------------------------------------------------------------ d3d11 ------

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
        back->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth  = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
