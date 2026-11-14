#pragma once
#include <cstddef>
#include <cstdint>
using u8=std::uint8_t; using u16=std::uint16_t; using u32=std::uint32_t;
using GPU_TEXTURE_WRAP_PARAM=int;
struct C3D_FrameBuf { void* colorBuf; void* depthBuf; unsigned short width,height; int colorFmt,depthFmt; bool block32; };
struct C3D_RenderTarget { C3D_FrameBuf frameBuf; };
struct C3D_BufInfo {}; struct C3D_AttrInfo {}; struct C3D_TexEnv {};
struct C3D_Tex { int id=0; bool vram=false; void* data=nullptr; };
struct C3D_Mtx {}; struct C3D_FVec { float x=0,y=0,z=0,w=1; };
struct DVLB_s { int numDVLE; int DVLE[1]; }; struct shaderProgram_s { int vertexShader; };
#define R_FAILED(x) ((x)<0)
enum {GPU_ADD,GPU_ALWAYS,GPU_BLEND_ADD,GPU_CLAMP_TO_EDGE,GPU_CONSTANT,GPU_CULL_NONE,GPU_FLOAT,GPU_GEQUAL,
 GPU_INTERPOLATE,GPU_LINEAR,GPU_MODULATE,GPU_ONE,GPU_PREVIOUS,GPU_REPEAT,GPU_REPLACE,GPU_TEVOP_RGB_SRC_ALPHA,
 GPU_TEVOP_RGB_SRC_COLOR,GPU_TEXTURE0,GPU_TEXTURE1,GPU_TRIANGLES,GPU_VERTEX_SHADER,GPU_WRITE_ALL,GPU_ZERO,
 C3D_Alpha,C3D_Both,C3D_CLEAR_ALL,C3D_RGB,C3D_UNSIGNED_SHORT};
constexpr float C3D_AspectRatioTop=5.f/3.f;
void* linearAlloc(std::size_t); void linearFree(void*);
void* vramAlloc(std::size_t); void vramFree(void*);
u32 C3D_CalcColorBufSize(u32,u32,int); u32 C3D_CalcDepthBufSize(u32,u32,int);
void C3D_SyncTextureCopy(u32*,u32,u32*,u32,u32,u32);
void C3D_TexDelete(C3D_Tex*); void C3D_DrawElements(int,int,int,const void*);
inline void GSPGPU_FlushDataCache(void*,std::size_t) {}
inline int APT_CheckNew3DS(bool* value) {*value=false;return 0;}
inline DVLB_s* DVLB_ParseFile(u32*,unsigned) {static DVLB_s d{1,{0}};return &d;}
inline void DVLB_Free(DVLB_s*) {}
inline int shaderProgramInit(shaderProgram_s*) {return 0;}
inline int shaderProgramSetVsh(shaderProgram_s*,int*) {return 0;}
inline void shaderProgramFree(shaderProgram_s*) {}
inline int shaderInstanceGetUniformLocation(int,const char*) {return 0;}
inline void BufInfo_Init(C3D_BufInfo*) {}
inline int BufInfo_Add(C3D_BufInfo*,void*,std::size_t,int,int) {return 0;}
inline void AttrInfo_Init(C3D_AttrInfo*) {}
inline void AttrInfo_AddLoader(C3D_AttrInfo*,int,int,int) {}
bool C3D_FrameBegin(u8);
void C3D_FrameEnd(u8);
bool C3D_FrameDrawOn(C3D_RenderTarget*);
inline C3D_TexEnv* C3D_GetTexEnv(int) {static C3D_TexEnv e;return &e;}
#define NOOP(name) template<class... T> inline void name(T...) {}
NOOP(C3D_TexSetFilter) NOOP(C3D_TexSetWrap) NOOP(C3D_TexFlush)
void C3D_TexBind(int, const C3D_Tex*);
NOOP(C3D_TexEnvInit) NOOP(C3D_TexEnvSrc) NOOP(C3D_TexEnvFunc) NOOP(C3D_TexEnvColor) NOOP(C3D_TexEnvOpRgb)
NOOP(C3D_SetBufInfo) NOOP(C3D_FVUnifMtx4x4) NOOP(C3D_BindProgram) NOOP(C3D_SetAttrInfo)
NOOP(C3D_CullFace) NOOP(C3D_DepthTest) NOOP(C3D_AlphaTest) NOOP(C3D_AlphaBlend) NOOP(C3D_RenderTargetClear)
NOOP(Mtx_Identity) NOOP(Mtx_LookAt) NOOP(Mtx_Multiply) NOOP(Mtx_PerspTilt) NOOP(Mtx_RotateY)
NOOP(Mtx_RotateZ) NOOP(Mtx_Scale) NOOP(Mtx_Translate)
#undef NOOP
inline float C3D_AngleFromDegrees(float n) {return n;}
inline C3D_FVec FVec3_New(float,float,float) {return {};}
inline C3D_FVec FVec4_New(float,float,float,float) {return {};}
inline C3D_FVec Mtx_MultiplyFVec4(C3D_Mtx*,C3D_FVec) {return {};}
