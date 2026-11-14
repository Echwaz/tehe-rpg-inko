#pragma once
#include "citro3d.h"
using Tex3DS_Texture = void*;
Tex3DS_Texture Tex3DS_TextureImport(const void*,std::size_t,C3D_Tex*,void*,bool);
inline void Tex3DS_TextureFree(Tex3DS_Texture) {}
