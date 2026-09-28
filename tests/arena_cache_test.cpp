#include "../source/arena_cache.h"
#include "../source/cinematic_view.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

static std::map<void*, std::size_t> allocations;
static std::vector<std::function<void()>> queue;
static int allocationCalls = 0, failAllocation = -1, copies = 0;

u32 C3D_CalcColorBufSize(u32 w, u32 h, int fmt) { return w*h*(fmt == 0 ? 4 : 2); }
u32 C3D_CalcDepthBufSize(u32 w, u32 h, int fmt) { return w*h*(fmt == 0 ? 2 : 4); }
void* vramAlloc(std::size_t n) {
    if (++allocationCalls == failAllocation) return nullptr;
    void* p = std::malloc(n);
    assert(p);
    allocations[p] = n;
    return p;
}
void vramFree(void* p) {
    assert(queue.empty()); // do not free a buffer still referenced by queued GPU work
    assert(allocations.erase(p) == 1);
    std::free(p);
}
void C3D_SyncTextureCopy(u32* src, u32 inDim, u32* dst, u32 outDim, u32 size, u32 flags) {
    assert(inDim == 0 && outDim == 0 && flags == 8 && size % 16 == 0);
    ++copies;
    queue.push_back([=] { std::memcpy(dst, src, size); });
}
static void finishFrame() {
    for (auto& command : queue) command();
    queue.clear();
}

static void testCinematicShots() {
    using namespace cinematic_view;
    assert(select(true, false, -1, -1) == Shot::Battle);
    assert(select(true, true, -1, -1) == Shot::Prologue);
    for (int f = 0; f < 300; ++f) {
        assert(select(true, false, f, -1) == Shot::Closeup);
        assert(select(false, false, f, -1) == Shot::Moving);
    }
    for (int f = 0; f <= defeat_cutscene::Duration; ++f) {
        assert(select(true, false, -1, f) ==
               (f < defeat_cutscene::TreeStart ? Shot::Closeup : Shot::TreeWide));
        assert(select(false, false, -1, f) == (f < 340 ? Shot::Moving : Shot::TreeWide));
    }
    // Simulate the actual snapshot policy over awakening, battle, death, victory, restart.
    std::vector<u32> color(240*400), depth(240*400);
    C3D_FrameBuf fb = {color.data(), depth.data(), 240, 400, 0, 0, false};
    ArenaCache cache;
    assert(cache.prepare(fb));
    Shot previous = Shot::Moving;
    int arenaDraws = 0;
    auto frame = [&](Shot shot, u32 marker) {
        if (shot != previous) { cache.invalidate(); previous = shot; }
        if (!cache.restore(fb)) {
            ++arenaDraws;
            color[0] = marker; depth[0] = marker;
            cache.capture(fb);
        }
        finishFrame();
        assert(color[0] == marker && depth[0] == marker);
        color[0] = 999; depth[0] = 999; // actor/HUD must never leak into the snapshot
    };
    for (int f = 0; f < 300; ++f) frame(select(true,false,f,-1), 1);
    assert(arenaDraws == 1);
    frame(Shot::Battle, 2);
    for (int f = 0; f < 420; ++f)
        frame(select(true,false,-1,f), f < defeat_cutscene::TreeStart ? 3 : 4);
    assert(arenaDraws == 4); // 1 awaken + 1 battle + 2 death shots
    for (int f = 0; f < 120; ++f) frame(select(true,false,-1,420), 4);
    assert(arenaDraws == 4);
    frame(select(true,false,-1,-1), 5); // restart invalidates tree snapshot
    assert(arenaDraws == 5);
    cache.release();
    assert(allocations.empty());
}

int main() {
    testCinematicShots();
    allocationCalls = 0; copies = 0;
    std::vector<u32> color(240*400, 0);
    std::vector<std::uint16_t> depth(240*400, 0);
    C3D_FrameBuf fb = {color.data(), depth.data(), 240, 400, 0, 0, false};
    ArenaCache cache;
    assert(cache.prepare(fb));
    assert(allocations.size() == 2);
    std::size_t bytes = 0;
    for (const auto& a : allocations) bytes += a.second;
    assert(bytes == 576000);
    assert(!cache.restore(fb));

    // Queued arena rendering, then snapshot, then monster/HUD changes.
    queue.push_back([&] {
        for (std::size_t i = 0; i < color.size(); ++i) {
            color[i] = static_cast<u32>(i);
            depth[i] = i % 2 ? 200 : 50;
        }
    });
    cache.capture(fb);
    queue.push_back([&] { color[0] = 999999; depth[0] = 0; });
    finishFrame(); // next FrameBegin waits here in the real renderer
    assert(cache.valid() && copies == 2);
    const int initialAllocations = allocationCalls;
    for (int frame = 0; frame < 4; ++frame) {
        assert(cache.prepare(fb));
        assert(allocationCalls == initialAllocations);
        assert(cache.restore(fb));
        finishFrame();
        for (std::size_t i = 0; i < color.size(); ++i) {
            assert(color[i] == i);
            assert(depth[i] == (i % 2 ? 200 : 50));
        }
        // GEQUAL depth: a monster pixel at 100 passes behind shallow ground,
        // but not in front of the nearer building (200).
        assert(100 >= depth[0] && !(100 >= depth[1]));
        color[0] = 999999;
        std::fill(depth.begin(), depth.end(), 0); // HUD clears depth every frame
    }
    cache.invalidate();
    assert(!cache.restore(fb));
    cache.capture(fb);
    finishFrame();
    // Dialogue -> battle -> restart: replace both color and depth without
    // reallocating the shared snapshot or restoring the previous camera.
    for (u32 scene = 1; scene <= 3; ++scene) {
        cache.invalidate();
        assert(!cache.valid() && !cache.restore(fb));
        assert(cache.prepare(fb));
        assert(allocationCalls == initialAllocations);
        std::fill(color.begin(), color.end(), scene * 100);
        std::fill(depth.begin(), depth.end(), scene * 10);
        cache.capture(fb);
        finishFrame();
        color[0] = 999999;
        depth[0] = 0;
        assert(cache.restore(fb));
        finishFrame();
        assert(color[0] == scene * 100 && depth[0] == scene * 10);
    }
    cache.release();
    cache.release(); // shutdown after a partially initialized renderer is safe
    assert(allocations.empty());

    // Partial allocation failure frees the first buffer; no per-frame retries.
    failAllocation = allocationCalls + 2;
    assert(!cache.prepare(fb));
    assert(allocations.empty());
    const int failedCalls = allocationCalls, previousCopies = copies;
    assert(!cache.prepare(fb) && allocationCalls == failedCalls);
    assert(!cache.restore(fb));
    cache.capture(fb);
    assert(copies == previousCopies);

    // A new framebuffer layout triggers reallocation and a new snapshot.
    failAllocation = -1;
    fb.width = 320;
    fb.height = 240; // fits the existing test storage
    assert(cache.prepare(fb) && !cache.valid());
    cache.capture(fb);
    finishFrame();
    fb.block32 = true;
    assert(!cache.restore(fb));
    assert(cache.prepare(fb) && !cache.valid());
    cache.release();
    assert(allocations.empty());
    std::puts("PASS: cached color/depth, queue order, occlusion, reuse, invalidation, fallback, resize, cleanup");
}
