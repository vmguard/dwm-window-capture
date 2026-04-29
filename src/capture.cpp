#include "capture.h"
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")

#define HR_CHECK(hr, msg) if (FAILED(hr)) { throw std::runtime_error(msg); }


struct ScopedMap {
    ID3D11DeviceContext* ctx;
    ID3D11Resource* res;
    ScopedMap(ID3D11DeviceContext* c, ID3D11Resource* r) : ctx(c), res(r) {}
    ~ScopedMap() { if (ctx && res) ctx->Unmap(res, 0); }
};

ComInitializer::ComInitializer() {
    // RPC_E_CHANGED_MODE is a nonfatal success. it just means COM was already initialized with a different threading model
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        throw std::runtime_error("COM init failed");
}

ComInitializer::~ComInitializer() { CoUninitialize(); }

D3DContext::D3DContext() {
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    HR_CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1,
        D3D11_SDK_VERSION, &device_, nullptr, &context_), "Device creation failed");

    ComPtr<IDXGIDevice> dxgi;
    device_.As(&dxgi);
    ComPtr<IDXGIAdapter> adapter;
    dxgi->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(adapter.GetAddressOf()));

    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    luid_ = desc.AdapterLuid;
}

PngEncoder::PngEncoder() {
    HR_CHECK(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory_)), "WIC Factory failed");
}

void PngEncoder::Save(const std::wstring& path, const CaptureResult& res) {
    ComPtr<IWICStream> stream;
    HR_CHECK(factory_->CreateStream(&stream), "Stream creation failed");
    HR_CHECK(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "File init failed");

    ComPtr<IWICBitmapEncoder> encoder;
    HR_CHECK(factory_->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "Encoder init failed");
    encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frame;
    encoder->CreateNewFrame(&frame, nullptr);
    frame->Initialize(nullptr);
    frame->SetSize(res.width, res.height);

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    frame->SetPixelFormat(&format);
    frame->WritePixels(res.height, res.pitch, res.pitch * res.height, const_cast<BYTE*>(res.pixels.data()));

    frame->Commit();
    encoder->Commit();
}

WindowCapturer::WindowCapturer(D3DContext& d3d) : d3d_(d3d) {
    LoadDwmFunctions();
}

void WindowCapturer::LoadDwmFunctions() {
    HMODULE hDwm = GetModuleHandleA("dwmapi.dll");
    if (!hDwm) hDwm = LoadLibraryA("dwmapi.dll");
    if (!hDwm) throw std::runtime_error("dwmapi.dll missing");

    getSurface_ = (pfnGetSurf)GetProcAddress(hDwm, MAKEINTRESOURCEA(100));
    updateSurface_ = (pfnUpdSurf)GetProcAddress(hDwm, MAKEINTRESOURCEA(101));

    if (!getSurface_ || !updateSurface_) throw std::runtime_error("DWM exports not found");
}

CaptureResult WindowCapturer::Capture(HWND hwnd) {
    DWORD fmt{}; HANDLE hSurface{}; UINT64 updateId{};
    HRESULT hr = getSurface_(hwnd, d3d_.AdapterLuid(), nullptr, DWM_REDIRECTION_GDI, &fmt, &hSurface, &updateId);

    if (hr != S_DWM_SURFACE_AVAILABLE) throw std::runtime_error("Window surface unavailable");

    updateSurface_(hwnd, updateId, 0, nullptr, nullptr);

    ComPtr<ID3D11Resource> res;
    d3d_.Device()->OpenSharedResource(hSurface, IID_PPV_ARGS(&res));

    ComPtr<ID3D11Texture2D> srcTex;
    res.As(&srcTex);

    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);
    // We must copy the shared surface to a staging texture with CPU_ACCESS_READ
    // Direct3D 11 does not allow the cpu to map() a default usage shared resource 
    // directly so this copy acts as our bridge from GPU to System memory.
    auto staging = CreateStagingTexture(desc);
    d3d_.Context()->CopyResource(staging.Get(), srcTex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    HR_CHECK(d3d_.Context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Texture map failed");

    ScopedMap unmap(d3d_.Context(), staging.Get()); 

    CaptureResult result;
    result.width = desc.Width;
    result.height = desc.Height;
    result.pitch = mapped.RowPitch;
    result.pixels.assign((BYTE*)mapped.pData, (BYTE*)mapped.pData + (result.pitch * result.height));

    return result;
}

ComPtr<ID3D11Texture2D> WindowCapturer::CreateStagingTexture(const D3D11_TEXTURE2D_DESC& srcDesc) {
    D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> tex;
    d3d_.Device()->CreateTexture2D(&stagingDesc, nullptr, &tex);
    return tex;
}
