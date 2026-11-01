#pragma once
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
class RenderCapture {
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> image_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    UINT width_=0,height_=0;
    bool active_=false;
public:
    ~RenderCapture(){shutdown();}
    ID3D11Device* device() const { return device_.Get(); }
    bool init(UINT width,UINT height) {
        width_=width; height_=height;
        D3D_FEATURE_LEVEL level;
        if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_))) return false;
        D3D11_TEXTURE2D_DESC desc{}; desc.Width=width; desc.Height=height; desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count=1; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        if (FAILED(device_->CreateTexture2D(&desc,nullptr,&image_)) || FAILED(device_->CreateRenderTargetView(image_.Get(),nullptr,&target_))) return false;
        active_=ImGui_ImplDX11_Init(device_.Get(),context_.Get()); return active_;
    }
    void begin() {
        const float clear[]{.025f,.035f,.05f,1};
        context_->ClearRenderTargetView(target_.Get(),clear); ID3D11RenderTargetView* view=target_.Get();
        context_->OMSetRenderTargets(1,&view,nullptr); ImGui_ImplDX11_NewFrame();
    }
    bool save(const std::filesystem::path& path) {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        D3D11_TEXTURE2D_DESC desc{}; image_->GetDesc(&desc); desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&desc,nullptr,&staging))) return false;
        context_->CopyResource(staging.Get(),image_.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
        const HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        bool ok=false;
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> factory; Microsoft::WRL::ComPtr<IWICStream> stream;
            Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder; Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
            WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
            ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) &&
                SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE)) &&
                SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)) && SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)) &&
                SUCCEEDED(encoder->CreateNewFrame(&frame,nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(width_,height_)) &&
                SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA) &&
                SUCCEEDED(frame->WritePixels(height_,mapped.RowPitch,mapped.RowPitch*height_,static_cast<BYTE*>(mapped.pData))) &&
                SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
        }
        if (SUCCEEDED(co)) CoUninitialize();
        context_->Unmap(staging.Get(),0); return ok;
    }
    void shutdown() { if (active_ && ImGui::GetCurrentContext()) ImGui_ImplDX11_Shutdown(); active_=false; }
};
