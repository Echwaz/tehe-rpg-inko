#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace mesh_io {
struct Vertex { float position[3]; float uv[2]; };
static_assert(sizeof(Vertex) == 20, "Mesh vertex format must remain 20 bytes");
struct Header { char magic[4]; std::uint32_t vertices, indices, reserved; };
static_assert(sizeof(Header) == 16, "Mesh file header changed");
inline bool readHeader(std::FILE* file, Header& h) {
    if (!file || std::fread(&h, sizeof h, 1, file) != 1 || std::memcmp(h.magic, "TMH1", 4) ||
        h.reserved != 0 || h.vertices == 0 || h.vertices > 65536 ||
        h.indices == 0 || h.indices > 196608 || h.indices % 3) return false;
    const long expected = sizeof h + h.vertices * sizeof(Vertex) + h.indices * sizeof(std::uint16_t);
    if (std::fseek(file, 0, SEEK_END) || std::ftell(file) != expected) return false;
    return std::fseek(file, sizeof h, SEEK_SET) == 0;
}
inline bool readPayload(std::FILE* file, const Header& h, Vertex* vertices,
                        std::uint16_t* indices, const std::uint16_t* shared = nullptr) {
    if (!vertices || (!indices && !shared) ||
        std::fread(vertices, sizeof(Vertex), h.vertices, file) != h.vertices) return false;
    for (std::uint32_t i = 0; i < h.vertices; ++i) {
        for (float value : vertices[i].position) if (!std::isfinite(value)) return false;
        for (float value : vertices[i].uv) if (!std::isfinite(value)) return false;
    }
    if (shared) {
        std::uint16_t chunk[128];
        for (std::uint32_t at = 0; at < h.indices;) {
            const unsigned count = h.indices - at < 128 ? h.indices - at : 128;
            if (std::fread(chunk, sizeof(chunk[0]), count, file) != count ||
                std::memcmp(chunk, shared + at, count * sizeof(chunk[0]))) return false;
            for (unsigned i = 0; i < count; ++i) if (chunk[i] >= h.vertices) return false;
            at += count;
        }
    } else {
        if (std::fread(indices, sizeof(indices[0]), h.indices, file) != h.indices) return false;
        for (std::uint32_t i = 0; i < h.indices; ++i) if (indices[i] >= h.vertices) return false;
    }
    return true;
}
}
