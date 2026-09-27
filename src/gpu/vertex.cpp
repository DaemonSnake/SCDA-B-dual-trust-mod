#include "gpu/vertex.h"

#include <algorithm>
#include <cstring>

#include "kernel/kernel.h"

namespace gpu {

namespace {

// D3DVSDT: low nibble the kind, high nibble the component count
void Convert(uint32_t type, const uint8_t* p, float* o) {
    o[0] = 0;
    o[1] = 0;
    o[2] = 0;
    o[3] = 1;
    uint32_t kind = type & 15, n = std::min<uint32_t>((type >> 4) & 15, 4);
    if (type == 0x72) {  // FLOAT2H
        float v[3];
        std::memcpy(v, p, 12);
        o[0] = v[0];
        o[1] = v[1];
        o[3] = v[2];
        return;
    }
    switch (kind) {
        case 0:  // D3DCOLOR, bytes B G R A
            o[0] = p[2] / 255.0f;
            o[1] = p[1] / 255.0f;
            o[2] = p[0] / 255.0f;
            o[3] = p[3] / 255.0f;
            break;
        case 1:  // NORMSHORT
            for (uint32_t i = 0; i < n; i++) {
                int16_t s;
                std::memcpy(&s, p + i * 2, 2);
                o[i] = std::max(s / 32767.0f, -1.0f);
            }
            break;
        case 2:
            std::memcpy(o, p, n * 4);
            break;
        case 4:  // PBYTE
            for (uint32_t i = 0; i < n; i++) o[i] = p[i] / 255.0f;
            break;
        case 5:  // SHORT
            for (uint32_t i = 0; i < n; i++) {
                int16_t s;
                std::memcpy(&s, p + i * 2, 2);
                o[i] = s;
            }
            break;
        case 6: {  // NORMPACKED3, 11 11 10 signed
            uint32_t v;
            std::memcpy(&v, p, 4);
            int32_t x = int32_t(v << 21) >> 21, y = int32_t(v << 10) >> 21, z = int32_t(v) >> 22;
            o[0] = std::max(x / 1023.0f, -1.0f);
            o[1] = std::max(y / 1023.0f, -1.0f);
            o[2] = std::max(z / 511.0f, -1.0f);
            break;
        }
        default:
            break;
    }
}

uint32_t TypeBytes(uint32_t t) {
    if (t == 0x72) return 12;
    uint32_t kind = t & 15, n = (t >> 4) & 15;
    switch (kind) {
        case 0: return 4;
        case 1: case 5: return 2 * n;
        case 4: return n;
        case 6: return 4;
        default: return 4 * n;
    }
}

}  // namespace

void FvfInputs(uint32_t fvf, VertexShader& vs) {
    vs.inputs.fill({});
    uint32_t off = 0;
    auto add = [&](int reg, uint32_t type) {
        vs.inputs[size_t(reg)] = {0, off, type};
        off += TypeBytes(type);
    };
    switch (fvf & 0xE) {
        case 0x2: add(0, 0x32); break;
        case 0x4: add(0, 0x42); break;
        case 0x6: case 0x8: case 0xA: case 0xC: {
            add(0, 0x32);
            uint32_t weights = ((fvf & 0xE) - 0x4) / 2;
            add(1, 0x02 | (weights << 4));
            break;
        }
        default: break;
    }
    if (fvf & 0x10) add(2, 0x32);
    if (fvf & 0x20) add(6, 0x12);
    if (fvf & 0x40) add(3, 0x40);
    if (fvf & 0x80) add(4, 0x40);
    uint32_t tex = (fvf >> 8) & 15;
    static const uint32_t kTexType[4] = {0x22, 0x32, 0x42, 0x12};
    for (uint32_t i = 0; i < tex && i < 4; i++) add(int(9 + i), kTexType[(fvf >> (16 + i * 2)) & 3]);
    vs.stream_stride.fill(0);
    vs.stream_stride[0] = off;
}

bool Assemble(const State& s, uint32_t prim, uint32_t start, uint32_t count, uint32_t indices_va, Assembled& out) {
    using namespace kernel;
    out.vertices.clear();
    out.indices.clear();
    out.regs = 0;
    out.vertex_count = 0;
    if (!count || prim < 1 || prim > 10) return false;

    VertexShader fvf_layout;
    const VertexShader* layout = s.vs.get();
    if (!(s.vs_handle & 1)) {
        FvfInputs(s.vs_handle, fvf_layout);
        layout = &fvf_layout;
        out.pretransformed = (s.vs_handle & 0xE) == 0x4;
    } else {
        out.pretransformed = false;
    }
    if (!layout) return false;

    // logical vertex numbers in draw order
    std::vector<uint32_t> seq(count);
    if (indices_va) {
        const uint8_t* ip = static_cast<const uint8_t*>(HostPtr(indices_va));
        for (uint32_t i = 0; i < count; i++) {
            uint16_t v;
            std::memcpy(&v, ip + i * 2, 2);
            seq[i] = v + s.base_vertex;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) seq[i] = start + i;
    }
    uint32_t lo = *std::min_element(seq.begin(), seq.end()), hi = *std::max_element(seq.begin(), seq.end());
    bool ranged = uint64_t(hi - lo) + 1 <= uint64_t(count) * 2 + 64;
    uint32_t n = ranged ? hi - lo + 1 : count;

    int regs[16];
    int nregs = 0;
    for (int r = 0; r < 16; r++) {
        const auto& in = layout->inputs[size_t(r)];
        if (in.stream < 0 || !in.type || in.type == 0x02) continue;
        const auto& st = s.streams[size_t(in.stream)];
        if (!st.vb) continue;
        regs[nregs++] = r;
        out.regs |= uint16_t(1u << r);
    }
    if (!nregs) return false;

    out.vertex_count = n;
    out.vertices.resize(size_t(n) * nregs * 4);
    for (int k = 0; k < nregs; k++) {
        const auto& in = layout->inputs[size_t(regs[k])];
        const auto& st = s.streams[size_t(in.stream)];
        uint32_t data = mem::VirtualFromPhysical(R32(st.vb + 4));
        uint32_t stride = st.stride ? st.stride : layout->stream_stride[size_t(in.stream)];
        const uint8_t* src = static_cast<const uint8_t*>(HostPtr(data + in.offset));
        for (uint32_t v = 0; v < n; v++) {
            uint32_t id = ranged ? lo + v : seq[v];
            Convert(in.type, src + size_t(id) * stride, &out.vertices[(size_t(v) * nregs + k) * 4]);
        }
    }

    auto local = [&](uint32_t i) { return ranged ? seq[i] - lo : i; };
    auto& ix = out.indices;
    out.topology = Topology::Triangles;
    switch (prim) {
        case 1:  // points
            out.topology = Topology::Points;
            for (uint32_t i = 0; i < count; i++) ix.push_back(local(i));
            break;
        case 2:  // line list
            out.topology = Topology::Lines;
            for (uint32_t i = 0; i + 1 < count; i += 2) ix.insert(ix.end(), {local(i), local(i + 1)});
            break;
        case 3:  // line loop
        case 4:  // line strip
            out.topology = Topology::Lines;
            for (uint32_t i = 0; i + 1 < count; i++) ix.insert(ix.end(), {local(i), local(i + 1)});
            if (prim == 3 && count > 2) ix.insert(ix.end(), {local(count - 1), local(0)});
            break;
        case 5:  // triangle list
            for (uint32_t i = 0; i + 2 < count; i += 3) ix.insert(ix.end(), {local(i), local(i + 1), local(i + 2)});
            break;
        case 6:  // triangle strip
            for (uint32_t i = 0; i + 2 < count; i++) {
                if (i & 1) ix.insert(ix.end(), {local(i + 1), local(i), local(i + 2)});
                else ix.insert(ix.end(), {local(i), local(i + 1), local(i + 2)});
            }
            break;
        case 7:   // fan
        case 10:  // polygon
            for (uint32_t i = 1; i + 1 < count; i++) ix.insert(ix.end(), {local(0), local(i), local(i + 1)});
            break;
        case 8:  // quad list
            for (uint32_t i = 0; i + 3 < count; i += 4)
                ix.insert(ix.end(), {local(i), local(i + 1), local(i + 2), local(i), local(i + 2), local(i + 3)});
            break;
        case 9:  // quad strip
            for (uint32_t i = 0; i + 3 < count; i += 2)
                ix.insert(ix.end(), {local(i), local(i + 1), local(i + 2), local(i + 1), local(i + 3), local(i + 2)});
            break;
    }
    return !ix.empty();
}

}  // namespace gpu
