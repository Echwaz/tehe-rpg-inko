#include <cassert>
#include <cmath>
#include <cstdio>
#include "../source/mesh_arena_data.h"
#include "../source/mesh_slug_data.h"
#include "../source/mesh_slug_awaken_data.h"

template <class Vertex, size_t V, size_t I>
void check(const char* name, const Vertex (&vertices)[V], const unsigned short (&indices)[I]) {
    static_assert(sizeof(Vertex) == 8 * sizeof(float), "GPU vertex stride changed");
    static_assert(V <= 65536 && I % 3 == 0, "16-bit triangle indices required");
    for (const auto& vertex : vertices) {
        for (float value : vertex.pos) assert(std::isfinite(value));
        for (float value : vertex.uv) assert(std::isfinite(value));
        for (float value : vertex.nrm) assert(std::isfinite(value));
    }
    for (auto index : indices) assert(index < V);
    std::printf("%s: %zu vertices, %zu triangles, %zu GPU bytes\n", name, V, I / 3,
                sizeof(vertices) + sizeof(indices));
}


#include "../source/defeat_cutscene.h"
struct V3 { float x,y,z; };
V3 sub(V3 a, V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
float dot(V3 a,V3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V3 cross(V3 a,V3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
V3 point(const Battle3DVertexArena& v) { return {v.pos[0],v.pos[1],v.pos[2]}; }
bool intersects(V3 eye,V3 target,V3 a,V3 b,V3 c) {
    const V3 d=sub(target,eye), e1=sub(b,a), e2=sub(c,a), h=cross(d,e2);
    const float det=dot(e1,h);
    if (std::fabs(det)<1e-7f) return false;
    const V3 s=sub(eye,a), q=cross(s,e1);
    const float u=dot(s,h)/det, v=dot(d,q)/det, t=dot(e2,q)/det;
    return u>=0.f && v>=0.f && u+v<=1.f && t>0.0001f && t<0.999f;
}
template<size_t V,size_t I>
bool blocked(V3 eye,V3 target,const Battle3DVertexArena (&verts)[V],const unsigned short (&idx)[I]) {
    for(size_t i=0;i<I;i+=3)
        if(intersects(eye,target,point(verts[idx[i]]),point(verts[idx[i+1]]),point(verts[idx[i+2]]))) return true;
    return false;
}
void checkDefeatCamera() {
    const V3 tree[]={{0,.3f,.5f},{0,1,.5f},{0,2,.5f},{0,3,.5f},{0,4,.5f},
                     {.3f,5,.5f},{.5f,6.4f,.42f},{-1.8f,2.9f,.65f},{1.85f,4.1f,.45f}};
    // Regression: the previous final camera was behind the foreground building.
    assert(blocked({1.4f,1.85f,11.f},tree[3],battle3d_arena_buildings_verts,battle3d_arena_buildings_indices));
    for(int frame=0;frame<=defeat_cutscene::Duration;++frame) {
        const auto c=defeat_cutscene::camera(frame);
        const auto p=defeat_cutscene::sample(frame);
        for(auto target:tree) {
            target.y-=6.4f*(1.f-p.growth);
            if(!p.tree || target.y<.15f) continue;
            assert(!blocked({c.x,c.y,c.z},target,battle3d_arena_buildings_verts,battle3d_arena_buildings_indices));
            assert(!blocked({c.x,c.y,c.z},target,battle3d_arena_rubble_verts,battle3d_arena_rubble_indices));
        }
    }
    std::puts("Defeat camera: visible tree landmarks clear of buildings/rubble across 421 frames");
}


V3 normalized(V3 v) {
    const float length=std::sqrt(dot(v,v));
    return {v.x/length,v.y/length,v.z/length};
}
void checkTreeSkyCoverage() {
    const auto c=defeat_cutscene::camera(defeat_cutscene::Duration);
    const V3 eye={c.x,c.y,c.z}, forward=normalized(sub({c.targetX,c.targetY,c.targetZ},eye));
    const V3 right=normalized(cross(forward,{0,1,0})), up=cross(right,forward);
    const float tanHalf=std::tan(c.fov*3.14159265f/360.f);
    for(int i=0;i<=40;++i) {
        const float sx=-1.f+i*.05f, sy=1.f;
        const V3 ray=normalized({forward.x+right.x*sx*tanHalf*5.f/3.f+up.x*sy*tanHalf,
                                forward.y+right.y*sx*tanHalf*5.f/3.f+up.y*sy*tanHalf,
                                forward.z+right.z*sx*tanHalf*5.f/3.f+up.z*sy*tanHalf});
        const V3 farPoint={eye.x+ray.x*59.f,eye.y+ray.y*59.f,eye.z+ray.z*59.f};
        assert(blocked(eye,farPoint,battle3d_arena_backdrop_verts,battle3d_arena_backdrop_indices));
    }
    std::puts("Tree sky: complete top edge covered inside the far plane");
}

int main() {
    checkTreeSkyCoverage();
    checkDefeatCamera();
    check("Ground", battle3d_arena_ground_verts, battle3d_arena_ground_indices);
    check("Buildings", battle3d_arena_buildings_verts, battle3d_arena_buildings_indices);
    check("Rubble", battle3d_arena_rubble_verts, battle3d_arena_rubble_indices);
    check("Backdrop", battle3d_arena_backdrop_verts, battle3d_arena_backdrop_indices);
    check("Death Slug", battle3d_slug_verts, battle3d_slug_indices);
    check("Death Slug Awaken", battle3d_slug_awaken_verts, battle3d_slug_indices);
    int changed = 0;
    for (int i = 0; i < BATTLE3D_SLUG_VERTEX_COUNT; ++i) {
        const auto& base = battle3d_slug_verts[i];
        const auto& awaken = battle3d_slug_awaken_verts[i];
        assert(base.uv[0] == awaken.uv[0] && base.uv[1] == awaken.uv[1]);
        float normalLength = 0.f;
        float distance = 0.f;
        for (int k = 0; k < 3; ++k) {
            normalLength += awaken.nrm[k] * awaken.nrm[k];
            distance += std::fabs(base.pos[k] - awaken.pos[k]);
        }
        assert(std::fabs(normalLength - 1.f) < 0.001f);
        assert(awaken.pos[1] >= 0.f);
        if (distance > 0.01f) ++changed;
    }
    assert(changed > BATTLE3D_SLUG_VERTEX_COUNT / 2);
    std::puts("Mesh checks OK");
}

