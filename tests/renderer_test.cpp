#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <set>
#include <cstring>
#include <vector>
#include <tex3ds.h>
#include "../source/battle3d.h"

static std::map<void*,std::size_t> linear, vram;
static std::set<int> textures;
static std::size_t vramBytes=0, textureBytes=0;
static int imports=0, allocations=0, failTexture=0;
static bool gpuPending=false, failLinear=false, inFrame=false;
static std::size_t capacity=2*1024*1024;
void* linearAlloc(std::size_t n) {assert(!inFrame);if (failLinear) {failLinear=false;return nullptr;} void* p=std::malloc(n);linear[p]=n;++allocations;return p;}
void linearFree(void* p) {assert(!gpuPending && !inFrame);assert(linear.erase(p)==1);std::free(p);}
void* vramAlloc(std::size_t n) {
    if (vramBytes+n>capacity) return nullptr;
    void* p=std::malloc(n);vram[p]=n;vramBytes+=n;++allocations;return p;
}
void vramFree(void* p) {assert(!gpuPending);vramBytes-=vram.at(p);vram.erase(p);std::free(p);}
u32 C3D_CalcColorBufSize(u32 w,u32 h,int) {return w*h*4;}
u32 C3D_CalcDepthBufSize(u32 w,u32 h,int) {return w*h*4;}
bool C3D_FrameBegin(u8) {
    if (inFrame) return false;
    gpuPending=false;
    inFrame=true;
    return true;
}
void C3D_FrameEnd(u8) {assert(inFrame);inFrame=false;}
bool C3D_FrameDrawOn(C3D_RenderTarget*) {assert(inFrame);return true;}
void C3D_SyncTextureCopy(u32*,u32,u32*,u32,u32,u32) {gpuPending=inFrame;}
static const C3D_Tex* bound[2]={};
void C3D_TexBind(int unit,const C3D_Tex* tex) {assert(unit<2 && tex);bound[unit]=tex;}
void C3D_DrawElements(int,int,int,const void*) {
    assert(bound[0] && textures.count(bound[0]->id));
    if (bound[1]) assert(textures.count(bound[1]->id));
    gpuPending=true;
}
static std::size_t texBytes(int id) {return (id==11 || id==15) ? 1024*1024 : id==4 ? 512*1024 : 128*1024;}
Tex3DS_Texture Tex3DS_TextureImport(const void* bytes,std::size_t,C3D_Tex* tex,void*,bool preferVram) {
    // Tex3DS frees its linear staging allocation immediately after the VRAM transfer.
    // Inside a frame that transfer is only queued, so importing there is a lifetime bug.
    assert(!inFrame && !gpuPending);
    ++imports;
    const int id=*static_cast<const u8*>(bytes);
    if (id==failTexture) return nullptr;
    const auto n=texBytes(id);
    void* data=preferVram ? vramAlloc(n) : linearAlloc(n);
    if (!data) return nullptr;
    assert(textures.insert(id).second);
    tex->id=id;tex->data=data;tex->vram=preferVram;textureBytes+=n;
    return tex;
}
void C3D_TexDelete(C3D_Tex* tex) {
    assert(!gpuPending && textures.erase(tex->id)==1);
    textureBytes-=texBytes(tex->id);
    if (tex->vram) vramFree(tex->data); else linearFree(tex->data);
}
extern "C" {
#define TEX(name,id) extern const u8 name##_t3x[]={id}; asm(".global " #name "_t3x_end\n.set " #name "_t3x_end," #name "_t3x+1");
TEX(skitter_diffuse,1) TEX(skitter_break_emissive,2) TEX(skitter_palette,3) TEX(skitter_asphalt,4)
TEX(arena_ground_diffuse,5) TEX(arena_ground_emissive,6) TEX(arena_buildings_diffuse,7)
TEX(arena_buildings_emissive,8) TEX(arena_rubble_diffuse,9) TEX(arena_rubble_emissive,10)
TEX(arena_backdrop_diffuse,11) TEX(death_slug_diffuse,12) TEX(death_slug_emissive,13)
TEX(death_slug_break_emissive,14)
TEX(skitter_backdrop_diffuse,15)
TEX(exploration_palette,16)
#undef TEX
}
static bool frame(C3D_RenderTarget& top,EnemyKind kind,bool broken=false) {
    const bool prepared=battle3d::prepareScene(&top,kind);
    assert(!inFrame);
    assert(C3D_FrameBegin(0));
    battle3d::SceneState state;state.enemyKind=kind;state.broken=broken;
    const bool rendered=battle3d::renderTop(&top,state);
    C3D_FrameEnd(0);
    assert(prepared==rendered);
    return rendered;
}
static void stop() {
    gpuPending=false;battle3d::shutdown();
    assert(linear.empty() && vram.empty() && textures.empty() && textureBytes==0);
}
int main() {
    std::vector<u32> color(240*400),depth(240*400);
    C3D_RenderTarget top{{color.data(),depth.data(),240,400,0,0,false}};
    assert(battle3d::init(&top));
    assert(vram.size()==2 && textures.empty()); // Cache reserved before any texture import.
    assert(frame(top,EnemyKind::Skitter));
    assert(textures==std::set<int>({1,2,3,4,5,7,15}));
    assert(textureBytes==2176*1024);
    const int loaded=imports,allocated=allocations;
    for (int n=0;n<120;++n) assert(frame(top,EnemyKind::Skitter,n%2));
    assert(imports==loaded && allocations==allocated);
    for (int n=0;n<5;++n) {
        assert(frame(top,EnemyKind::Hellspider));
        assert(textures==std::set<int>({5,6,7,8,9,10,11,12,13,14}));
        assert(textureBytes==2176*1024);
        assert(frame(top,EnemyKind::Skitter));
        assert(textures==std::set<int>({1,2,3,4,5,7,15}));
    }
    stop();
    failTexture=3;
    assert(battle3d::init(&top));
    assert(!frame(top,EnemyKind::Skitter));
    assert(textures==std::set<int>({5,7}));
    const int failedImports=imports;
    assert(!frame(top,EnemyKind::Skitter) && imports==failedImports);
    failTexture=0;
    assert(frame(top,EnemyKind::Hellspider));stop();
    failTexture=2;
    assert(battle3d::init(&top));
    assert(frame(top,EnemyKind::Skitter,true)); // Optional Break texture retains the tint fallback.
    assert(!textures.count(2));stop();
    failTexture=0;
    assert(battle3d::init(&top));failLinear=true;
    assert(!frame(top,EnemyKind::Skitter));stop();
    capacity=0;
    assert(battle3d::init(&top));
    assert(frame(top,EnemyKind::Skitter));
    const int noCacheImports=imports,noCacheAllocations=allocations;
    assert(frame(top,EnemyKind::Skitter));
    assert(imports==noCacheImports && allocations==noCacheAllocations);
    stop();capacity=2*1024*1024;
    assert(battle3d::init(&top));
    exploration::World world;
    for(int swap=0;swap<4;++swap) {
        assert(battle3d::prepareExploration(&top));
        assert(textures==std::set<int>({4,5,7,15,16}));
        assert(textureBytes==1920*1024);
        const int beforeImports=imports,beforeAllocations=allocations;
        for(int i=0;i<60;++i) {
            assert(battle3d::prepareExploration(&top));
            world.update(.2f,.3f,.1f,false,false,1.f/60);
            assert(C3D_FrameBegin(0));
            assert(battle3d::renderExploration(&top,world,i));
            C3D_FrameEnd(0);
        }
        assert(imports==beforeImports && allocations==beforeAllocations);
        assert(frame(top,swap%2?EnemyKind::Hellspider:EnemyKind::Skitter));
    }
    stop();
    std::puts("Renderer: lazy assets, cache priority, stable frames, encounter swaps, failure cleanup and GPU fences OK");
}
