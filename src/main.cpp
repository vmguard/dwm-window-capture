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
