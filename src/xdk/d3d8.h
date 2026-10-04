#pragma once
#include <mutex>

#include "gpu/state.h"

namespace d3d {

gpu::State& State();
std::recursive_mutex& Lock();
// ambient occlusion over the scene drawn so far; m: its D3D perspective projection
void Occlusion(const float* m);

}  // namespace d3d
