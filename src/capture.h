#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

static constexpr DWORD DWM_REDIRECTION_GDI = 0x10;
static constexpr HRESULT S_DWM_SURFACE_AVAILABLE = (HRESULT)0x00263005;

struct CaptureResult {
    std::vector<BYTE> pixels;
    UINT width = 0;
    UINT height = 0;
    UINT pitch = 0;
};

class ComInitializer {
public:
    ComInitializer();
    ~ComInitializer();
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;
};

class D3DContext {
public:
    D3DContext();
    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }
    LUID AdapterLuid() const { return luid_; }
    // D3DContext manages the singleton like lifecycle of the GPU device.
    // We delete copy/assignment to prevent accidental reference count spikes or resource leaks across threads.
    D3DContext(const D3DContext&) = delete;
    D3DContext& operator=(const D3DContext&) = delete;

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    LUID luid_{};
};

class PngEncoder {
public:
    PngEncoder();
    void Save(const std::wstring& path, const CaptureResult& res);

private:
    ComPtr<IWICImagingFactory> factory_;
};

class WindowCapturer {
public:
    WindowCapturer(D3DContext& d3d);
    CaptureResult Capture(HWND hwnd);

private:
    D3DContext& d3d_;

    typedef HRESULT(WINAPI* pfnGetSurf)(HWND, LUID, HANDLE, DWORD, DWORD*, HANDLE*, UINT64*);
    typedef HRESULT(WINAPI* pfnUpdSurf)(HWND, UINT64, DWORD, HANDLE, RECT*);

    pfnGetSurf getSurface_ = nullptr;
    pfnUpdSurf updateSurface_ = nullptr;

    void LoadDwmFunctions();
    ComPtr<ID3D11Texture2D> CreateStagingTexture(const D3D11_TEXTURE2D_DESC& srcDesc);
};
