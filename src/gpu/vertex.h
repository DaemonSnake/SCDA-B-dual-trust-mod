#pragma once
#include <cstdint>
#include <vector>

#include "gpu/state.h"

namespace gpu {

enum class Topology : uint8_t { Points, Lines, Triangles };

// one draw's vertices expanded to float4 per used input register, indices rebuilt as a plain list
struct Assembled {
    std::vector<float> vertices;  // count * regs * 4
    std::vector<uint32_t> indices;
    uint16_t regs = 0;             // input registers present, bit per v register
    uint32_t vertex_count = 0;
    Topology topology = Topology::Triangles;
    bool pretransformed = false;   // FVF XYZRHW
};

// the FVF bits translated to the register layout the XDK's declarations use
void FvfInputs(uint32_t fvf, VertexShader& out);

// false when there is nothing to draw
bool Assemble(const State& s, uint32_t prim, uint32_t start, uint32_t count, uint32_t indices_va, Assembled& out);

}  // namespace gpu
