#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include "../source/mesh_io.h"
using namespace mesh_io;
static std::FILE* file(Header h, const Vertex* verts, const std::uint16_t* indices, int truncate=0) {
    std::FILE* f=std::tmpfile();assert(f);
    std::fwrite(&h,sizeof h,1,f);
    std::fwrite(verts,sizeof(Vertex),3,f);
    std::fwrite(indices,sizeof(std::uint16_t),3-truncate,f);
    std::rewind(f);return f;
}
int main() {
    Header h{{'T','M','H','1'},3,3,0},read;
    Vertex v[3]={{{0,0,0},{0,0}},{{1,0,0},{1,0}},{{0,1,0},{0,1}}},out[3];
    std::uint16_t idx[3]={0,1,2},copied[3];
    auto f=file(h,v,idx);assert(readHeader(f,read));
    assert(readPayload(f,read,out,copied));assert(!std::memcmp(v,out,sizeof v));std::fclose(f);
    f=file(h,v,idx);assert(readHeader(f,read));assert(readPayload(f,read,out,nullptr,idx));std::fclose(f);
    std::uint16_t other[3]={2,1,0};f=file(h,v,idx);assert(readHeader(f,read));
    assert(!readPayload(f,read,out,nullptr,other));std::fclose(f);
    idx[2]=3;f=file(h,v,idx);assert(readHeader(f,read));assert(!readPayload(f,read,out,copied));std::fclose(f);idx[2]=2;
    v[0].position[0]=std::numeric_limits<float>::infinity();f=file(h,v,idx);assert(readHeader(f,read));
    assert(!readPayload(f,read,out,copied));std::fclose(f);v[0].position[0]=0;
    f=file(h,v,idx,1);assert(!readHeader(f,read));std::fclose(f);
    h.reserved=1;f=file(h,v,idx);assert(!readHeader(f,read));std::fclose(f);h.reserved=0;
    h.vertices=65537;f=file(h,v,idx);assert(!readHeader(f,read));std::fclose(f);
    assert(!readHeader(nullptr,read));
    std::puts("Mesh IO: exact payload, shared topology, truncation, bounds and non-finite rejection OK");
}
