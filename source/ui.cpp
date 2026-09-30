#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "circle_points.h"


void TouchState::update() {
    const u32 kDown = hidKeysDown();
    const u32 kHeld = hidKeysHeld();
    const u32 kUp   = hidKeysUp();

    began = (kDown & KEY_TOUCH) != 0;
    held  = (kHeld & KEY_TOUCH) != 0;
    ended = (kUp   & KEY_TOUCH) != 0;

    if (held) {
        touchPosition tp;
        hidTouchRead(&tp);
        x = tp.px;   // koordinat layar bawah: 0..319
        y = tp.py;   //                        0..239
    }
    // Saat ended, x/y masih berisi posisi frame sebelumnya.
}


bool TextRenderer::init(size_t maxGlyphs) {
    buf_ = C2D_TextBufNew(maxGlyphs);
    cacheCapacity_ = maxGlyphs;
    cacheBuf_ = C2D_TextBufNew(maxGlyphs);
    return buf_ != nullptr;
}

void TextRenderer::shutdown() {
    cache_.clear();
    if (cacheBuf_) C2D_TextBufDelete(cacheBuf_);
    cacheBuf_ = nullptr;
    if (buf_) {
        C2D_TextBufDelete(buf_);
        buf_ = nullptr;
    }
}

void TextRenderer::beginFrame() {
    C2D_TextBufClear(buf_);
    if (resetCache_ && cacheBuf_) {
        cache_.clear();
        C2D_TextBufClear(cacheBuf_);
        resetCache_ = false;
    }
}

C2D_Text TextRenderer::parse(const char* str) {
    const auto found = cache_.find(str);
    if (found != cache_.end()) return found->second;
    C2D_Text text;
    // Panjang byte UTF-8 adalah batas atas konservatif untuk jumlah glyph.
    // Teks yang meluap tetap di buffer frame; jangan invalidate teks cache di tengah frame.
    const size_t bytes = std::strlen(str);
    if (cacheBuf_ && cache_.size() < 128 &&
        bytes <= cacheCapacity_ - C2D_TextBufGetNumGlyphs(cacheBuf_)) {
        C2D_TextParse(&text, cacheBuf_, str);
        C2D_TextOptimize(&text);
        cache_.emplace(str, text);
    } else {
        resetCache_ = cacheBuf_ != nullptr;
        C2D_TextParse(&text, buf_, str);
        C2D_TextOptimize(&text);
    }
    return text;
}

void TextRenderer::draw(const char* str, float x, float y, float scale, u32 color, float wrap) {
    C2D_Text t = parse(str);
    if (wrap > 0.f)
        C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, kZText, scale, scale, color, wrap);
    else
        C2D_DrawText(&t, C2D_WithColor, x, y, kZText, scale, scale, color);
}

void TextRenderer::drawCentered(const char* str, float cx, float cy, float scale, u32 color) {
    C2D_Text t = parse(str);
    float w = 0.f, h = 0.f;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, cx - w * 0.5f, cy - h * 0.5f, kZText, scale, scale, color);
}

float TextRenderer::width(const char* str, float scale) {
    C2D_Text t = parse(str);
    float w = 0.f, h = 0.f;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    return w;
}

void TextRenderer::drawRight(const char* str, float xr, float y, float scale, u32 color) {
    C2D_Text t = parse(str);
    float w = 0.f, h = 0.f;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, xr - w, y, kZText, scale, scale, color);
}

void TextRenderer::drawShadow(const char* str, float x, float y, float scale, u32 color, float wrap) {
    C2D_Text t = parse(str);
    const u32 shadow = C2D_Color32(0, 0, 0, 190);
    if (wrap > 0.f) {
        C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x + 1, y + 1, kZText - 0.01f, scale, scale, shadow, wrap);
        C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, kZText, scale, scale, color, wrap);
    } else {
        C2D_DrawText(&t, C2D_WithColor, x + 1, y + 1, kZText - 0.01f, scale, scale, shadow);
        C2D_DrawText(&t, C2D_WithColor, x, y, kZText, scale, scale, color);
    }
}

void TextRenderer::drawOutlined(const char* str, float x, float y, float scale, u32 color,
                                u32 outline, float thick) {
    C2D_Text t = parse(str);
    static const float kDir[8][2] = { {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                      {1, 0},   {-1, 1}, {0, 1},  {1, 1} };
    for (const auto& d : kDir)
        C2D_DrawText(&t, C2D_WithColor, x + d[0] * thick, y + d[1] * thick, kZText - 0.02f,
                     scale, scale, outline);
    C2D_DrawText(&t, C2D_WithColor, x, y, kZText, scale, scale, color);
}


void ButtonGroup::add(int id, float x, float y, float w, float h, const char* label,
                      bool enabled, u32 color, float scale, bool circle) {
    buttons_.push_back(Button{ id, x, y, w, h, label, enabled, color, scale, circle });
}

bool ButtonGroup::exists(int id) const {
    for (const auto& b : buttons_)
        if (b.id == id) return true;
    return false;
}

int ButtonGroup::hitId(int px, int py) const {
    for (const auto& b : buttons_)
        if (b.enabled && b.contains(px, py)) return b.id;
    return kNone;
}

int ButtonGroup::update(const TouchState& t) {
    int activated = kNone;

    if (t.began) pressedId_ = hitId(t.x, t.y);
    if (pressedId_ != kNone && !exists(pressedId_)) pressedId_ = kNone;

    inside_ = false;
    if (pressedId_ != kNone) {
        const bool over = (hitId(t.x, t.y) == pressedId_);
        if (t.ended) {
            if (over) activated = pressedId_;
            pressedId_ = kNone;
        } else if (t.held) {
            inside_ = over;
        } else {
            pressedId_ = kNone;
        }
    }
    return activated;
}

void ButtonGroup::draw(TextRenderer& text) const {
    for (const auto& b : buttons_) {
        if (b.color == 0) continue;
        const bool isPressed = pressed(b.id);
        const float off = isPressed ? 2.f : 0.f;
        // b.color = warna aksen: dipakai untuk garis tepi, isinya campuran tipis dengan latar.
        const u32 accent = b.enabled ? b.color : colors::grey;
        const u32 fill = mixColor(colors::bg, accent,
                                  isPressed ? 0.50f : (b.enabled ? 0.22f : 0.06f));
        const u32 border = (b.enabled && outline_ != 0) ? outline_ : accent;
        drawPill(b.x, b.y + off, b.w, b.h - off, fill, border, kZPanel);
        text.drawCentered(b.label.c_str(), b.x + b.w * 0.5f, b.y + b.h * 0.5f + off * 0.5f, b.scale,
                          b.enabled ? colors::white : colors::grey);
    }
}


u32 mixColor(u32 a, u32 b, float t) {
    auto ch = [&](int shift) {
        const float ca = static_cast<float>((a >> shift) & 0xFF);
        const float cb = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<u8>(ca + (cb - ca) * t);
    };
    return C2D_Color32(ch(0), ch(8), ch(16), 255);
}

void drawLine(float x0, float y0, float x1, float y1, float t, u32 color, float z) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) return;
    const float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
    C2D_DrawTriangle(x0 + nx, y0 + ny, color, x0 - nx, y0 - ny, color, x1 + nx, y1 + ny, color, z);
    C2D_DrawTriangle(x1 + nx, y1 + ny, color, x0 - nx, y0 - ny, color, x1 - nx, y1 - ny, color, z);
}

namespace {

// Isi panel bersudut patah: tengah + kiri + kanan + empat segitiga sudut (tidak saling menimpa).
void fillAngular(float x, float y, float w, float h, float cut, u32 fill, float z) {
    C2D_DrawRectSolid(x + cut, y, z, w - 2 * cut, h, fill);
    C2D_DrawRectSolid(x, y + cut, z, cut, h - 2 * cut, fill);
    C2D_DrawRectSolid(x + w - cut, y + cut, z, cut, h - 2 * cut, fill);
    C2D_DrawTriangle(x, y + cut, fill, x + cut, y, fill, x + cut, y + cut, fill, z);
    C2D_DrawTriangle(x + w - cut, y, fill, x + w, y + cut, fill, x + w - cut, y + cut, fill, z);
    C2D_DrawTriangle(x + w, y + h - cut, fill, x + w - cut, y + h, fill, x + w - cut, y + h - cut,
                     fill, z);
    C2D_DrawTriangle(x, y + h - cut, fill, x + cut, y + h - cut, fill, x + cut, y + h, fill, z);
}

void bracketCorners(float x, float y, float w, float h, float cut, u32 color, float z) {
    const u32 bright = color | 0xFF000000u;
    const float tb = 2.2f, len = 12.f;
    drawLine(x, y + cut, x + cut, y, tb, bright, z);
    drawLine(x, y + cut, x, y + cut + len, tb, bright, z);
    drawLine(x + cut, y, x + cut + len, y, tb, bright, z);
    drawLine(x + w, y + h - cut, x + w - cut, y + h, tb, bright, z);
    drawLine(x + w, y + h - cut, x + w, y + h - cut - len, tb, bright, z);
    drawLine(x + w - cut, y + h, x + w - cut - len, y + h, tb, bright, z);
}

}  // namespace

void drawAngular(float x, float y, float w, float h, float cut, u32 fill, u32 line, float z,
                 bool brackets) {
    cut = std::min(cut, std::min(w, h) * 0.5f);
    fillAngular(x, y, w, h, cut, fill, z);

    const float zl = z + 0.01f, t = 1.2f;
    drawLine(x + cut, y, x + w - cut, y, t, line, zl);
    drawLine(x + w, y + cut, x + w, y + h - cut, t, line, zl);
    drawLine(x + cut, y + h, x + w - cut, y + h, t, line, zl);
    drawLine(x, y + cut, x, y + h - cut, t, line, zl);
    drawLine(x, y + cut, x + cut, y, t, line, zl);
    drawLine(x + w - cut, y, x + w, y + cut, t, line, zl);
    drawLine(x + w, y + h - cut, x + w - cut, y + h, t, line, zl);
    drawLine(x + cut, y + h, x, y + h - cut, t, line, zl);

    if (brackets) bracketCorners(x, y, w, h, cut, line, z + 0.02f);
}

void drawBracketed(float x, float y, float w, float h, float cut, u32 fill, u32 bracket, float z) {
    cut = std::min(cut, std::min(w, h) * 0.5f);
    fillAngular(x, y, w, h, cut, fill, z);
    bracketCorners(x, y, w, h, cut, bracket, z + 0.02f);
}

void drawFlat(float x, float y, float w, float h, float cut, u32 fill, float z) {
    fillAngular(x, y, w, h, std::min(cut, std::min(w, h) * 0.5f), fill, z);
}

void drawGlass(float x, float y, float w, float h, float cut) {
    drawFlat(x, y, w, h, cut, colors::glass, kZPanel);
}

void drawPanel(float x, float y, float w, float h, u32 fill, u32 border) {
    drawAngular(x, y, w, h, 6.f, fill, border, kZPanel, false);
}

void drawRoundRect(float x, float y, float w, float h, float r, u32 color, float z) {
    r = std::min(r, std::min(w, h) * 0.5f);
    if (r < 1.f) {
        C2D_DrawRectSolid(x, y, z, w, h, color);
        return;
    }
    C2D_DrawRectSolid(x + r, y, z, w - 2 * r, h, color);
    if (h - 2 * r > 0.01f) {
        C2D_DrawRectSolid(x, y + r, z, r, h - 2 * r, color);
        C2D_DrawRectSolid(x + w - r, y + r, z, r, h - 2 * r, color);
    }
    const struct { float cx, cy; } corners[4] = {
        { x + r,     y + r },
        { x + w - r, y + r },
        { x + w - r, y + h - r },
        { x + r,     y + h - r },
    };
    const int kSeg = 6;
    int corner = 0;
    const int starts[] = {12, 18, 0, 6};
    const auto& points = circlePoints(24);
    for (const auto& c : corners) {
        const int start = starts[corner++];
        for (int i = 0; i < kSeg; ++i) {
            C2D_DrawTriangle(c.cx, c.cy, color,
                             c.cx + r * points[(start + i) % 24].x, c.cy + r * points[(start + i) % 24].y, color,
                             c.cx + r * points[(start + i + 1) % 24].x, c.cy + r * points[(start + i + 1) % 24].y, color, z);
        }
    }
}

void drawPill(float x, float y, float w, float h, u32 fill, u32 border, float z) {
    drawRoundRect(x, y, w, h, h * 0.5f, border, z);
    drawRoundRect(x + 1.6f, y + 1.6f, w - 3.2f, h - 3.2f, (h - 3.2f) * 0.5f, fill, z + 0.01f);
}

// Lingkaran digambar sebagai kipas segitiga, BUKAN C2D_DrawCircle. C2D_DrawCircle memakai tekstur
// prosedural GPU yang tidak didukung semua emulator (Azahar di Android menampilkannya sebagai
// kotak), sedangkan segitiga polos berjalan di mana saja. Lingkaran yang besar dan hampir buram
// diberi tepi lembut selebar 1 px (alfa turun ke 0) sebagai pengganti anti-aliasing.
void drawDisc(float cx, float cy, float r, u32 color, float z) {
    if (r <= 0.f) return;
    int n = static_cast<int>(6.f + r * 0.7f);
    n = n < 10 ? 10 : (n > 28 ? 28 : n);
    const bool soft = r >= 8.f && (color >> 24) >= 200u;
    const float rin = soft ? r - 0.5f : r;
    const u32 clear = color & 0x00FFFFFFu;
    const auto& points = circlePoints(n);
    for (int i = 0; i < n; ++i) {
        const float c0 = points[i].x, s0 = points[i].y;
        const float c1 = points[i + 1].x, s1 = points[i + 1].y;
        C2D_DrawTriangle(cx, cy, color, cx + rin * c0, cy + rin * s0, color,
                         cx + rin * c1, cy + rin * s1, color, z);
        if (!soft) continue;
        const float ro = r + 0.5f;
        C2D_DrawTriangle(cx + rin * c0, cy + rin * s0, color, cx + ro * c0, cy + ro * s0, clear,
                         cx + rin * c1, cy + rin * s1, color, z);
        C2D_DrawTriangle(cx + ro * c0, cy + ro * s0, clear, cx + ro * c1, cy + ro * s1, clear,
                         cx + rin * c1, cy + rin * s1, color, z);
    }
}

void drawRing(float cx, float cy, float r, u32 ring, float z, bool glow) {
    if (glow) {
        const u32 rgb = ring & 0x00FFFFFFu;
        drawDisc(cx, cy, r + 8, rgb | (26u << 24), z);
        drawDisc(cx, cy, r + 5.5f, rgb | (48u << 24), z + 0.002f);
        drawDisc(cx, cy, r + 3, rgb | (84u << 24), z + 0.004f);
    }
    drawDisc(cx, cy, r, ring, z + 0.006f);
    drawDisc(cx, cy, r - 2.4f, colors::disc, z + 0.008f);
}

void drawBarGrad(float x, float y, float w, float h, int cur, int max, u32 left, u32 right, u32 track) {
    C2D_DrawRectSolid(x, y, kZBar, w, h, track);
    if (max > 0 && cur > 0) {
        float r = static_cast<float>(cur) / static_cast<float>(max);
        if (r > 1.f) r = 1.f;
        // Gradasi mengikuti panjang isi: ujung kanan isi selalu warna "right".
        C2D_DrawRectangle(x, y, kZBar + 0.02f, w * r, h, left, right, left, right);
    }
}

void drawOutline(float x, float y, float w, float h, float t, u32 color, float z) {
    C2D_DrawRectSolid(x, y, z, w, t, color);
    C2D_DrawRectSolid(x, y + h - t, z, w, t, color);
    C2D_DrawRectSolid(x, y, z, t, h, color);
    C2D_DrawRectSolid(x + w - t, y, z, t, h, color);
}

void drawBar(float x, float y, float w, float h, int cur, int max, u32 fg, u32 bg) {
    C2D_DrawRectSolid(x, y, kZBar, w, h, bg);
    if (max > 0 && cur > 0) {
        float r = static_cast<float>(cur) / static_cast<float>(max);
        if (r > 1.f) r = 1.f;
        C2D_DrawRectSolid(x, y, kZBar + 0.02f, w * r, h, fg);
    }
}
