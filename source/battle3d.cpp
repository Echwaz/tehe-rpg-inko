#include "battle3d.h"
#include "awakening.h"
#include "defeat_cutscene.h"
#include "mesh_white_tree.h"
#include "arena_cache.h"
#include "cinematic_view.h"
#include <tex3ds.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "mesh_arena_data.h"
#include "mesh_slug_data.h"
#include "mesh_slug_awaken_data.h"
#include "battle3d_shbin.h"

extern "C" {
#define TEXTURE_SYMBOL(name) extern const u8 name##_t3x[]; extern const u8 name##_t3x_end[];
TEXTURE_SYMBOL(arena_ground_diffuse)
TEXTURE_SYMBOL(arena_ground_emissive)
TEXTURE_SYMBOL(arena_buildings_diffuse)
TEXTURE_SYMBOL(arena_buildings_emissive)
TEXTURE_SYMBOL(arena_rubble_diffuse)
TEXTURE_SYMBOL(arena_rubble_emissive)
TEXTURE_SYMBOL(arena_backdrop_diffuse)
TEXTURE_SYMBOL(death_slug_diffuse)
TEXTURE_SYMBOL(death_slug_emissive)
TEXTURE_SYMBOL(death_slug_break_emissive)
#undef TEXTURE_SYMBOL
}

namespace battle3d {
namespace {
struct RenderVertex { float position[3]; float uv[2]; };
static_assert(sizeof(RenderVertex) == 5 * sizeof(float), "Unexpected GPU vertex layout");

struct Mesh {
    void* vertices = nullptr;
    u16* indices = nullptr;
    int count = 0;
    C3D_BufInfo buffers;
};
enum { Ground, Buildings, Rubble, Backdrop, Slug, SlugAwaken, TreeLight, TreeShade, TreeHoles, MeshCount };
enum { GroundD, GroundE, BuildingsD, BuildingsE, RubbleD, RubbleE, BackdropD, SlugD, SlugE, SlugBreakE, TexCount };
Mesh meshes[MeshCount];
C3D_Tex textures[TexCount] = {};
bool loaded[TexCount] = {};
DVLB_s* shader = nullptr;
shaderProgram_s program;
bool programCreated = false;
bool ready = false;
C3D_AttrInfo attributes;
int mvpLocation = -1;
float anchorX = 270.f, anchorY = 80.f;
// Putar bos yang menghadap +Z ke arah kiri pemain di kedua pose.
constexpr float kEnemyYawDegrees = -20.f;
ArenaCache arenaCache;
cinematic_view::Shot cachedShot = cinematic_view::Shot::Moving;
bool old3dsCinematics = true;

// Kalau init gagal, layar atas dikosongkan dan HUD tetap bisa dipakai.
bool fail() {
    shutdown();
    return false;
}

template <typename Vertex, size_t V, size_t I>
bool upload(Mesh& mesh, const Vertex (&vertices)[V], const u16 (&indices)[I]) {
    static_assert(sizeof(Vertex) == 8 * sizeof(float), "Unexpected mesh vertex layout");
    static_assert(V <= 65536 && I % 3 == 0, "Invalid indexed triangle mesh");
    for (size_t i = 0; i < I; ++i) if (indices[i] >= V) return false;
    mesh.vertices = linearAlloc(V * sizeof(RenderVertex));
    mesh.indices = static_cast<u16*>(linearAlloc(sizeof(indices)));
    if (!mesh.vertices || !mesh.indices) return false;
    // Shader unlit hanya membaca posisi dan UV, jadi normal tidak diunggah.
    auto* compact = static_cast<RenderVertex*>(mesh.vertices);
    for (size_t i = 0; i < V; ++i)
        std::memcpy(&compact[i], &vertices[i], sizeof(RenderVertex));
    std::memcpy(mesh.indices, indices, sizeof(indices));
    GSPGPU_FlushDataCache(mesh.vertices, V * sizeof(RenderVertex));
    GSPGPU_FlushDataCache(mesh.indices, sizeof(indices));
    mesh.count = static_cast<int>(I);
    BufInfo_Init(&mesh.buffers);
    return BufInfo_Add(&mesh.buffers, mesh.vertices, sizeof(RenderVertex), 2, 0x10) >= 0;
}

bool loadTexture(int id, const u8* start, const u8* end, GPU_TEXTURE_WRAP_PARAM wrap,
                 bool preferVram = true) {
    // Target layar sudah dialokasikan sebelum ini. Sisa VRAM diutamakan untuk tekstur yang
    // disampel tiap frame; kalau VRAM penuh, jatuh ke linear RAM.
    Tex3DS_Texture imported = Tex3DS_TextureImport(start, end - start, &textures[id], nullptr, preferVram);
    if (!imported && preferVram) {
        textures[id] = C3D_Tex{};
        imported = Tex3DS_TextureImport(start, end - start, &textures[id], nullptr, false);
    }
    if (!imported) return false;
    loaded[id] = true;
    Tex3DS_TextureFree(imported);
    C3D_TexSetFilter(&textures[id], GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&textures[id], wrap, wrap);
    C3D_TexFlush(&textures[id]);
    return true;
}

void material(int diffuse, int emissive, float flash, bool broken) {
    C3D_TexBind(0, &textures[diffuse]);
    // Jangan pernah C3D_TexBind(unit > 0, nullptr): citro3d membaca tex->param
    // untuk unit > 0, jadi pointer null langsung data abort (crash). Unit yang
    // tidak dipakai cukup tidak disentuh; texenv di bawah tidak merujuknya.
    if (emissive >= 0) C3D_TexBind(1, &textures[emissive]);
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    if (emissive >= 0) C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_TEXTURE1);
    else               C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0);
    C3D_TexEnvFunc(env, C3D_RGB, emissive >= 0 ? GPU_ADD : GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0);
    C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);

    env = C3D_GetTexEnv(1);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
    C3D_TexEnvColor(env, broken ? 0xFFB8B8FF : 0xFFFFFFFF);

    env = C3D_GetTexEnv(2);
    C3D_TexEnvInit(env);
    const u32 amount = static_cast<u32>(std::max(0.f, std::min(1.f, flash)) * 180.f);
    C3D_TexEnvColor(env, (amount << 24) | 0x00FFFFFF);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, GPU_PREVIOUS, GPU_CONSTANT);
    C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);
}

void solidMaterial(u32 color) {
    // Material tanpa tekstur untuk kulit kayu pucat dan simpul gelap. Semua stage warisan direset.
    for (int i = 0; i < 6; ++i) C3D_TexEnvInit(C3D_GetTexEnv(i));
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvColor(env, color);
}

void draw(int id) {
    C3D_SetBufInfo(&meshes[id].buffers);
    C3D_DrawElements(GPU_TRIANGLES, meshes[id].count, C3D_UNSIGNED_SHORT, meshes[id].indices);
}

void setMatrix(const C3D_Mtx& projection, const C3D_Mtx& view, const C3D_Mtx& model, bool enemy) {
    C3D_Mtx modelView, mvp;
    Mtx_Multiply(&modelView, &view, &model);
    Mtx_Multiply(&mvp, &projection, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, mvpLocation, &mvp);
    if (enemy) {
        const C3D_FVec p = Mtx_MultiplyFVec4(&mvp, FVec4_New(0.f, 2.2f, 0.2f, 1.f));
        if (p.w > 0.001f) {
            // Batalkan putaran seperempat di Mtx_PerspTilt untuk HUD tegak 400x240.
            anchorX = std::max(20.f, std::min(380.f, (1.f - p.y / p.w) * 200.f));
            anchorY = std::max(20.f, std::min(150.f, (1.f - p.x / p.w) * 120.f));
        }
    }
}
} // namespace

bool init() {
    if (ready) return true;
    shader = DVLB_ParseFile(reinterpret_cast<u32*>(const_cast<u8*>(battle3d_shbin)), battle3d_shbin_size);
    if (!shader || !shader->numDVLE) return fail();
    if (R_FAILED(shaderProgramInit(&program))) return fail();
    programCreated = true;
    if (R_FAILED(shaderProgramSetVsh(&program, &shader->DVLE[0]))) return fail();
    mvpLocation = shaderInstanceGetUniformLocation(program.vertexShader, "mvp");
    if (mvpLocation < 0) return fail();
    AttrInfo_Init(&attributes);
    AttrInfo_AddLoader(&attributes, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attributes, 1, GPU_FLOAT, 2);

    if (!upload(meshes[Ground], battle3d_arena_ground_verts, battle3d_arena_ground_indices) ||
        !upload(meshes[Buildings], battle3d_arena_buildings_verts, battle3d_arena_buildings_indices) ||
        !upload(meshes[Rubble], battle3d_arena_rubble_verts, battle3d_arena_rubble_indices) ||
        !upload(meshes[Backdrop], battle3d_arena_backdrop_verts, battle3d_arena_backdrop_indices) ||
        !upload(meshes[Slug], battle3d_slug_verts, battle3d_slug_indices) ||
        !upload(meshes[SlugAwaken], battle3d_slug_awaken_verts, battle3d_slug_indices)) return fail();

#define LOAD(id, name, wrap) if (!loadTexture(id, name##_t3x, name##_t3x_end, wrap)) return fail()
    LOAD(GroundD, arena_ground_diffuse, GPU_REPEAT);
    LOAD(GroundE, arena_ground_emissive, GPU_REPEAT);
    LOAD(BuildingsD, arena_buildings_diffuse, GPU_REPEAT);
    LOAD(BuildingsE, arena_buildings_emissive, GPU_REPEAT);
    LOAD(RubbleD, arena_rubble_diffuse, GPU_CLAMP_TO_EDGE);
    LOAD(RubbleE, arena_rubble_emissive, GPU_CLAMP_TO_EDGE);
    LOAD(BackdropD, arena_backdrop_diffuse, GPU_CLAMP_TO_EDGE);
    LOAD(SlugD, death_slug_diffuse, GPU_CLAMP_TO_EDGE);
    LOAD(SlugE, death_slug_emissive, GPU_CLAMP_TO_EDGE);
#undef LOAD
    // Varian opsional 256x256 RGB565 (128 KiB). Disimpan di linear memory supaya cache arena
    // Old 3DS tetap punya jatah VRAM. Tanpa draw pass atau stage sampling tambahan:
    // cukup gantikan binding emissive yang ada.
    loadTexture(SlugBreakE, death_slug_break_emissive_t3x,
                death_slug_break_emissive_t3x_end, GPU_CLAMP_TO_EDGE, false);
    if (!upload(meshes[TreeLight], white_tree::light_vertices, white_tree::light_indices) ||
        !upload(meshes[TreeShade], white_tree::shade_vertices, white_tree::shade_indices) ||
        !upload(meshes[TreeHoles], white_tree::hole_vertices, white_tree::hole_indices))
        return fail();
    bool new3ds = false;
    // Kalau gagal, pakai kebijakan kamera yang lebih murah.
    old3dsCinematics = R_FAILED(APT_CheckNew3DS(&new3ds)) || !new3ds;
    cachedShot = cinematic_view::Shot::Moving;
    ready = true;
    return true;
}

void shutdown() {
    // Cleanup juga jalan setelah init yang setengah jadi; jangan digantungkan pada ready.
    ready = false;
    arenaCache.release();
    for (int i = 0; i < MeshCount; ++i) {
        if (meshes[i].vertices) linearFree(meshes[i].vertices);
        if (meshes[i].indices) linearFree(meshes[i].indices);
        meshes[i] = Mesh{};
    }
    for (int i = 0; i < TexCount; ++i) {
        if (loaded[i]) C3D_TexDelete(&textures[i]);
        loaded[i] = false;
        textures[i] = C3D_Tex{};
    }
    if (programCreated) shaderProgramFree(&program);
    programCreated = false;
    if (shader) DVLB_Free(shader);
    shader = nullptr;
}

bool enabled() { return ready; }
void enemyAnchor(float& x, float& y) { x = anchorX; y = anchorY; }

bool renderTop(C3D_RenderTarget* top, const SceneState& state) {
    if (!ready || !top) return false;
    const bool dying = state.defeatFrame >= 0;
    const defeat_cutscene::Pose death = defeat_cutscene::sample(state.defeatFrame);
    const bool cinematic = state.awakeningFrame >= 0 || dying;
    const awakening::Pose cut = awakening::sample(state.awakeningFrame);
    const auto shot = cinematic_view::select(old3dsCinematics, state.camera == Camera::Prologue,
                                             state.awakeningFrame, state.defeatFrame);
    // Satu alokasi snapshot yang sudah ada. Jangan pernah memulihkan warna/depth dari shot lain.
    if (cachedShot != shot) {
        arenaCache.invalidate();
        cachedShot = shot;
    }
    const bool canCache = cinematic_view::cacheable(shot) &&
                          arenaCache.prepare(top->frameBuf);
    const bool restored = canCache && arenaCache.restore(top->frameBuf);
    if (!restored) C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x14121AFF, 0);
    // Selalu bind ulang setelah raw copy supaya GPU membaca framebuffer hasil pemulihan.
    if (!C3D_FrameDrawOn(top)) return false;
    C3D_BindProgram(&program);
    C3D_SetAttrInfo(&attributes);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    // Unit 1 dan 2 sudah null sejak C3D_Init; jangan di-bind ke nullptr (lihat material()).
    for (int i = 0; i < 6; ++i) C3D_TexEnvInit(C3D_GetTexEnv(i));

    C3D_Mtx projection, view, model;
    const bool fixedTree = shot == cinematic_view::Shot::TreeWide;
    const bool fixedClose = shot == cinematic_view::Shot::Closeup;
    const defeat_cutscene::CameraPose deathCamera = defeat_cutscene::camera(
        fixedTree ? defeat_cutscene::Duration : state.defeatFrame);
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(dying && !fixedClose ? deathCamera.fov : 50.f),
                  C3D_AspectRatioTop, 0.1f, 60.f, false);
    // Bos di kanan statistik, di atas kartu party.
    if (fixedClose) {
        // Monster ditengahkan tanpa menggeser tampilan arena; getar aktor tetap beranimasi.
        Mtx_LookAt(&view, FVec3_New(0.f, 1.55f, 7.3f), FVec3_New(0.f, 2.05f, 0.3f),
                   FVec3_New(0.f, 1.f, 0.f), false);
    } else if (dying) {
        Mtx_LookAt(&view,
                   FVec3_New(deathCamera.x, deathCamera.y, deathCamera.z),
                   FVec3_New(deathCamera.targetX, deathCamera.targetY, deathCamera.targetZ),
                   FVec3_New(0.f, 1.f, 0.f), false);
    } else if (state.camera == Camera::Prologue) {
        // Pandangan kota dari atas: bos tetap di atas pita dialog (y=164).
        Mtx_LookAt(&view, FVec3_New(6.8f, 11.8f, 14.5f), FVec3_New(0.f, -1.f, 0.5f),
                   FVec3_New(0.f, 1.f, 0.f), false);
    } else {
        const float blend = cut.camera;
        Mtx_LookAt(&view,
                   FVec3_New(-1.8f + blend * (2.8f + cut.orbit), 1.55f - blend * 0.45f,
                             7.3f - blend * 1.3f),
                   FVec3_New(-1.8f + blend * 1.8f, 2.05f - blend * 0.65f, 0.3f),
                   FVec3_New(0.f, 1.f, 0.f), false);
    }
    if (!restored) {
        Mtx_Identity(&model);
        setMatrix(projection, view, model, false);
        material(BackdropD, -1, 0.f, false); draw(Backdrop);
        material(GroundD, GroundE, 0.f, false); draw(Ground);
        material(BuildingsD, BuildingsE, 0.f, false); draw(Buildings);
        material(RubbleD, RubbleE, 0.f, false); draw(Rubble);
        // Snapshot harus mendahului monster dan clear HUD/depth milik pemanggil.
        if (canCache) arenaCache.capture(top->frameBuf);
    }

    if (dying) {
        // Material Break yang tertunda dan pose merunduk dipertahankan sampai pecah.
        const bool blueCracks = state.broken && loaded[SlugBreakE];
        const int emissive = blueCracks ? SlugBreakE : SlugE;
        const bool breakTint = state.broken && !blueCracks;
        const float poseHeight = state.stunned ? 0.86f : 1.f;
        if (state.defeatFrame < defeat_cutscene::Burst) {
            Mtx_Identity(&model);
            Mtx_Translate(&model, death.shake, 0.f, 0.5f, true);
            Mtx_RotateY(&model, C3D_AngleFromDegrees(kEnemyYawDegrees), true);
            Mtx_Scale(&model, 1.f, poseHeight, 1.f);
            setMatrix(projection, view, model, true);
            material(SlugD, emissive, death.flash, breakTint);
            draw(SlugAwaken);
        }
        if (death.fragments) {
            // Bagi segitiga bos yang asli: siluet utuh pecah jadi 24 keping.
            Mesh& mesh = meshes[SlugAwaken];
            const int triangles = mesh.count / 3;
            C3D_SetBufInfo(&mesh.buffers);
            material(SlugD, emissive, death.flash, breakTint);
            for (int i = 0; i < 24; ++i) {
                const float angle = i * 2.399963f;
                const float t = death.scatter;
                const float radius = (2.4f + (i % 4) * 0.35f) * t;
                Mtx_Identity(&model);
                Mtx_Translate(&model, std::cos(angle) * radius,
                              t * (1.4f + (i % 5) * 0.2f) - 2.5f * t * t,
                              0.5f + std::sin(angle) * radius, true);
                Mtx_RotateY(&model, C3D_AngleFromDegrees(kEnemyYawDegrees) + t * (i % 2 ? 1.f : -1.f), true);
                Mtx_Scale(&model, 1.f - t, (1.f - t) * poseHeight, 1.f - t);
                setMatrix(projection, view, model, false);
                const int first = (triangles * i / 24) * 3;
                const int last = (triangles * (i + 1) / 24) * 3;
                C3D_DrawElements(GPU_TRIANGLES, last - first, C3D_UNSIGNED_SHORT, mesh.indices + first);
            }
        }
        if (death.tree && death.growth > 0.f) {
            Mtx_Identity(&model);
            // Ujung menembus tanah lebih dulu, disusul batang yang melebar dan akar.
            Mtx_Translate(&model, death.shake * (1.f - death.growth),
                          -6.4f * (1.f - death.growth), 0.5f, true);
            setMatrix(projection, view, model, false);
            solidMaterial(0xFFF0EEE8); draw(TreeLight);
            solidMaterial(0xFFBBB7AD); draw(TreeShade);
            solidMaterial(0xFF38312F); draw(TreeHoles);
        }
        return true;
    }

    const float time = static_cast<float>(state.frame % 36000) / 60.f;
    // Pose merunduk milik Stun; Broken saja tetap memakai pose idle biasa.
    const float breath = state.stunned || state.defeated ? 0.f : 0.012f * std::sin(time * 2.5f);
    const float recoil = state.hit * 0.09f * std::sin(time * 65.f);
    Mtx_Identity(&model);
    Mtx_Translate(&model, recoil + cut.shake, 0.f, 0.5f + state.attack * 0.35f, true);
    Mtx_RotateY(&model, C3D_AngleFromDegrees(kEnemyYawDegrees), true);
    if (cinematic) Mtx_RotateZ(&model, cut.tilt, true);
    Mtx_Scale(&model, 1.f - breath, cinematic ? cut.height :
              (state.defeated ? 0.35f : (state.stunned ? 0.86f : 1.f + breath)), 1.f);
    setMatrix(projection, view, model, true);
    // SceneState::broken mengikuti visual BREAK yang tertunda, bukan hitungan damage
    // awal. Kedua pose berbagi atlas ini; recovery membersihkannya.
    const bool blueCracks = state.broken && !cut.revealed && loaded[SlugBreakE];
    material(SlugD, blueCracks ? SlugBreakE : SlugE,
             std::max(state.hit, cut.energy * 0.55f + cut.flash * 0.45f),
             state.broken && !blueCracks && !cut.revealed);
    draw(state.awakened || cut.revealed ? SlugAwaken : Slug);
    return true;
}
} // namespace battle3d
