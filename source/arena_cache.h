#pragma once
#include <citro3d.h>

// Snapshot framebuffer mentah: layout tiled PICA dan nilai depth dipertahankan.
// Pakai sekali per frame, setelah FrameBegin menunggu kerja GPU sebelumnya.
// Scene dan kamera harus statis. Invalidate dulu sebelum mengubah salah satunya.
class ArenaCache {
public:
    bool prepare(const C3D_FrameBuf& fb) {
        if (attempted_ && matches(fb)) return color_ && depth_;
        release();
        layout_ = fb;
        attempted_ = true;
        if (!fb.colorBuf || !fb.depthBuf || !fb.width || !fb.height) return false;
        colorBytes_ = C3D_CalcColorBufSize(fb.width, fb.height, fb.colorFmt);
        depthBytes_ = C3D_CalcDepthBufSize(fb.width, fb.height, fb.depthFmt);
        // TextureCopy memindahkan blok utuh 16 byte.
        if ((colorBytes_ & 15) || (depthBytes_ & 15)) return false;
        color_ = vramAlloc(colorBytes_);
        depth_ = vramAlloc(depthBytes_);
        if (!color_ || !depth_) {
            if (color_) vramFree(color_);
            if (depth_) vramFree(depth_);
            color_ = depth_ = nullptr;
            return false; // renderer terus menggambar langsung; jangan dicoba ulang tiap frame
        }
        return true;
    }

    bool restore(const C3D_FrameBuf& fb) const {
        if (!valid_ || !matches(fb)) return false;
        copy(color_, fb.colorBuf, colorBytes_);
        copy(depth_, fb.depthBuf, depthBytes_);
        // Pemanggil bind ulang target setelah salinan ini, supaya cache framebuffer PICA
        // ter-invalidate sebelum monster beranimasi digambar.
        return true;
    }

    void capture(const C3D_FrameBuf& fb) {
        if (!color_ || !depth_ || !matches(fb)) return;
        // SyncTextureCopy memecah dan mem-flush command gambar, lalu mengantre salinan saat di
        // dalam frame. Salinan ini mendahului command monster dan HUD.
        copy(fb.colorBuf, color_, colorBytes_);
        copy(fb.depthBuf, depth_, depthBytes_);
        valid_ = true; // FrameBegin berikutnya menunggu capture yang diantre ini selesai
    }

    bool valid() const { return valid_; }
    void invalidate() { valid_ = false; }

    // Panggil hanya setelah pemakaian GPU atas buffer ini selesai.
    void release() {
        if (color_) vramFree(color_);
        if (depth_) vramFree(depth_);
        color_ = depth_ = nullptr;
        attempted_ = valid_ = false;
        colorBytes_ = depthBytes_ = 0;
    }

private:
    bool matches(const C3D_FrameBuf& fb) const {
        return fb.width == layout_.width && fb.height == layout_.height &&
               fb.colorFmt == layout_.colorFmt && fb.depthFmt == layout_.depthFmt &&
               fb.block32 == layout_.block32 && fb.colorBuf && fb.depthBuf;
    }
    static void copy(void* source, void* destination, u32 bytes) {
        // Bentuk raw copy tanpa gap, sama seperti upload tekstur VRAM di Citro3D.
        C3D_SyncTextureCopy(static_cast<u32*>(source), 0,
                           static_cast<u32*>(destination), 0, bytes, 8);
    }
    C3D_FrameBuf layout_ = {};
    void* color_ = nullptr;
    void* depth_ = nullptr;
    u32 colorBytes_ = 0, depthBytes_ = 0;
    bool attempted_ = false, valid_ = false;
};
