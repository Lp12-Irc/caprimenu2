// C++20 / brand.cpp / MSVC DX11 + WIC / Windows x64
#include "brand.hpp"
#include "imgui.h"
#include <Windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace Brand {
static ComPtr<ID3D11ShaderResourceView> texture;
static ImVec2 uv0{0,0}, uv1{1,1};
bool init(ID3D11Device* device) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct Cleanup { HRESULT hr; ~Cleanup(){if(SUCCEEDED(hr)) CoUninitialize();} } cleanup{co};
    auto r = FindResourceW(nullptr, MAKEINTRESOURCEW(201), MAKEINTRESOURCEW(10));
    if (!r) return false;
    auto bytes = LockResource(LoadResource(nullptr, r));
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(static_cast<const BYTE*>(bytes), SizeofResource(nullptr,r)));
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICBitmapDecoder> decoder; ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> converter;
    if (!stream || FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)) ||
        FAILED(decoder->GetFrame(0,&frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return false;
    UINT w=0,h=0; converter->GetSize(&w,&h); if (!w || !h || w>8192 || h>8192) return false;
    std::vector<BYTE> pixels(w*h*4);
    if(FAILED(converter->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data()))) return false;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=w; desc.Height=h; desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0}; ComPtr<ID3D11Texture2D> tex;
    if(FAILED(device->CreateTexture2D(&desc,&data,&tex)) || FAILED(device->CreateShaderResourceView(tex.Get(),nullptr,&texture))) return false;
    if(w>h) { float pad=(1.f-float(h)/w)*.5f; uv0.x=pad; uv1.x=1-pad; }
    else { float pad=(1.f-float(w)/h)*.5f; uv0.y=pad; uv1.y=1-pad; }
    return true;
}
void shutdown(){ texture.Reset(); }
void avatar(float size) {
    auto p=ImGui::GetCursorScreenPos();
    if(texture) ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)texture.Get(),p,{p.x+size,p.y+size},uv0,uv1,IM_COL32_WHITE,size*.5f);
    ImGui::GetWindowDrawList()->AddCircle({p.x+size*.5f,p.y+size*.5f},size*.5f,IM_COL32(25,245,96,255),48,2);
    ImGui::Dummy({size,size});
}
}
