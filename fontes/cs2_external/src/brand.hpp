// C++20 / brand.hpp / MSVC DX11 / Windows x64
#pragma once
#include <d3d11.h>
namespace Brand { bool init(ID3D11Device* device); void shutdown(); void avatar(float size); }
