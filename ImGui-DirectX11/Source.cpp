#include <iostream>
#include <Windows.h>
#include "Hooks.hpp"
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx11.h>
#include <ImGui/imgui_impl_win32.h>
#include <DirectX/d3d11.h>
#include <Detours/detours.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "detours.lib")

HANDLE hProcess = GetCurrentProcess();
HANDLE hThread = NULL;
HWND hWindow = NULL;
bool isInitialized = false;
bool isMenuVisible = true;

HRESULT APIENTRY hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    static ID3D11Device* pDevice = nullptr;
    static ID3D11DeviceContext* pDeviceContext = nullptr;

    if (!pDevice || !pDeviceContext) {
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice)))) {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
        pDevice->GetImmediateContext(&pDeviceContext);
        if (!pDeviceContext) {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    if (!isInitialized) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        DXGI_SWAP_CHAIN_DESC swapChainDesc;
        pSwapChain->GetDesc(&swapChainDesc);
        hWindow = swapChainDesc.OutputWindow;

        if (hWindow) {
            SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
            ImGui_ImplWin32_Init(hWindow);
            ImGui_ImplDX11_Init(pDevice, pDeviceContext);
            isInitialized = true;
        }
    }

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        isMenuVisible = !isMenuVisible;
        ImGui::GetIO().MouseDrawCursor = isMenuVisible;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (isMenuVisible) {
        inputHandler();
        ImGui::ShowDemoWindow();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return oPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT APIENTRY hkDraw(ID3D11DeviceContext* pDeviceContext, UINT VertexCount, UINT StartVertexLocation) {
    return oDraw(pDeviceContext, VertexCount, StartVertexLocation);
}

HRESULT APIENTRY hkDrawIndexedPrimitive(ID3D11DeviceContext* pDeviceContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
    return oDrawIndexedPrimitive(pDeviceContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

HRESULT APIENTRY hkDrawIndexedInstanced(ID3D11DeviceContext* pDeviceContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) {
    return oDrawIndexedInstanced(pDeviceContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

HRESULT APIENTRY hkSetShaderResources(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) {
    return oSetShaderResources(pDeviceContext, StartSlot, NumViews, ppShaderResourceViews);
}

HRESULT APIENTRY hkSetRenderTargets(ID3D11DeviceContext* pDeviceContext, UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView) {
    return oSetRenderTargets(pDeviceContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}

HRESULT APIENTRY hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT APIENTRY hkCreateQuery(ID3D11Device* pDevice, const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery) {
    return oCreateQuery(pDevice, pQueryDesc, ppQuery);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (isInitialized && isMenuVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return TRUE;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        TerminateProcess(hProcess, 0);
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void inputHandler() {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    for (int i = 1; i < 5; i++) {
        io.MouseDown[i] = false;
    }
}

DWORD WINAPI initializeHook(LPVOID lpParam) {
    HMODULE hModuleD3D11 = GetModuleHandle("d3d11.dll");
    if (!hModuleD3D11) {
        return FALSE;
    }

    HMODULE hModuleDXGI = GetModuleHandle("dxgi.dll");
    if (!hModuleDXGI) {
        return FALSE;
    }

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = NULL;
    IDXGISwapChain* pSwapChain = nullptr;

    WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
    RegisterClassExA(&wc);
    HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

    D3D_FEATURE_LEVEL requestedFeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL obtainedLevel;
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = 0;
    swapChainDesc.BufferDesc.Height = 0;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requestedFeatureLevels, 2, D3D11_SDK_VERSION, &swapChainDesc, &pSwapChain, &pDevice, &obtainedLevel, &pContext))) {
        return FALSE;
    }

    DWORD_PTR* vTable = nullptr;
#ifdef _WIN64
    DWORD64* dvTable64 = reinterpret_cast<DWORD64*>(pSwapChain);
    vTable = reinterpret_cast<DWORD_PTR*>(dvTable64[0]);
#else
    DWORD* dvTable32 = reinterpret_cast<DWORD*>(pSwapChain);
    vTable = reinterpret_cast<DWORD_PTR*>(dvTable32[0]);
#endif

    oPresent = (Present)vTable[8];
    oDrawIndexedPrimitive = (DrawIndexedPrimitive)vTable[73];
    oDraw = (Draw)vTable[82];
    oSetShaderResources = (SetShaderResources)vTable[70];
    oSetRenderTargets = (SetRenderTargets)vTable[94];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(LPVOID&)oPresent, (PBYTE)hkPresent);
    DetourAttach(&(LPVOID&)oDrawIndexedPrimitive, (PBYTE)hkDrawIndexedPrimitive);
    DetourAttach(&(LPVOID&)oDraw, (PBYTE)hkDraw);
    DetourAttach(&(LPVOID&)oSetShaderResources, (PBYTE)hkSetShaderResources);
    DetourAttach(&(LPVOID&)oSetRenderTargets, (PBYTE)hkSetRenderTargets);
    DetourTransactionCommit();

    pDevice->Release();
    pContext->Release();
    pSwapChain->Release();
    return TRUE;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        hThread = CreateThread(NULL, 0, initializeHook, NULL, 0, NULL);
        if (!hThread) {
            return FALSE;
        }
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        if (hThread) {
            CloseHandle(hThread);
            hThread = NULL;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        break;
    }
    return TRUE;
}