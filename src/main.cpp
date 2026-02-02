#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <vector>
#include <comdef.h>
#include <shellapi.h>
#include <locale>
#include <codecvt>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

// declare undocumented dwm functions

typedef HRESULT(WINAPI* PFN_DwmpDxGetWindowSharedSurface)(
    HWND hwnd,
    LUID luidAdapter,
    HMONITOR hmonitorAssociation,
    DWORD dwFlags,
    DWORD* pfmtWindow,
    HANDLE* phDxSurface,
    UINT64* puiUpdateId
    );

typedef HRESULT(WINAPI* PFN_DwmpDxUpdateWindowSharedSurface)(
    HWND hwnd,
    UINT64 uiUpdateId,
    DWORD dwFlags,
    HMONITOR hmonitorAssociation,
    const RECT* prc
    );

// resolves at runtime because linking directly on older builds will explode
// If this returns E_NOTIMPL DWM either changed or you're on a system that doesnt support it
HRESULT DwmpDxGetWindowSharedSurface(
    HWND hwnd,
    LUID luidAdapter,
    HMONITOR hmonitorAssociation,
    DWORD dwFlags,
    DWORD* pfmtWindow,
    HANDLE* phDxSurface,
    UINT64* puiUpdateId
) {
    // Cache the function pointer so we only pay GetProcAddress once
    // Not thread safe but this code is single threaded anyway
    static PFN_DwmpDxGetWindowSharedSurface pfn = nullptr;
    if (!pfn) {
        HMODULE hDwmapi = LoadLibrary(L"dwmapi.dll");
        if (hDwmapi) {
            pfn = (PFN_DwmpDxGetWindowSharedSurface)GetProcAddress(hDwmapi, "DwmpDxGetWindowSharedSurface");
        }
        if (!pfn) {
            return E_NOTIMPL;
        }
    }
    return pfn(hwnd, luidAdapter, hmonitorAssociation, dwFlags, pfmtWindow, phDxSurface, puiUpdateId);
}
// window capture class
class WindowCapture {
private:
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    ID3D11Texture2D* stagingTex = nullptr;
    IWICImagingFactory* wicFactory = nullptr;

    // Common GUIDs
    const GUID GUID_WICPixelFormat32bppBGRA = { 0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f} };
    const GUID GUID_WICPixelFormat24bppBGR = { 0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c} };

    // Container formats
    const GUID GUID_ContainerFormatPng = { 0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf} };
    const GUID GUID_ContainerFormatBmp = { 0xaf1d87e, 0xfcfe, 0x4188, {0xbd, 0xeb, 0xa7, 0x90, 0x64, 0x71, 0xcb, 0xe3} };
    const GUID GUID_ContainerFormatJpeg = { 0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x2, 0x8e, 0x10, 0x57} };
    const GUID GUID_ContainerFormatTiff = { 0x163bcc30, 0xe2e9, 0x4f0b, {0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3} };
    const GUID GUID_ContainerFormatGif = { 0x1f8a5601, 0x7d4d, 0x4cbd, {0x9c, 0x82, 0x1b, 0xc8, 0xd4, 0xee, 0xb9, 0xa5} };

public:
    WindowCapture() {}

    ~WindowCapture() {
        cleanup();
    }

    HRESULT initialize() {
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &d3dDevice,
            nullptr,
            &d3dContext
        );

        if (SUCCEEDED(hr)) {
            hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wicFactory)
            );
        }

        return hr;
    }
