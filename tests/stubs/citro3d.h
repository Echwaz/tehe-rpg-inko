#pragma once
#include <cstddef>
#include <cstdint>
using u32 = std::uint32_t;
struct C3D_FrameBuf {
    void* colorBuf;
    void* depthBuf;
    unsigned short width, height;
    int colorFmt, depthFmt;
    bool block32;
};
u32 C3D_CalcColorBufSize(u32, u32, int);
u32 C3D_CalcDepthBufSize(u32, u32, int);
void* vramAlloc(std::size_t);
void vramFree(void*);
void C3D_SyncTextureCopy(u32*, u32, u32*, u32, u32, u32);
