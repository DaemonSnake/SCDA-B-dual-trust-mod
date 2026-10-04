#pragma once
#include <mutex>

#include "gpu/state.h"

namespace d3d {

gpu::State& State();
std::recursive_mutex& Lock();
// ambient occlusion over the scene drawn so far; m: its D3D perspective projection
void Occlusion(const float* m);
// draws from here on are magnified about the viewport's centre, 1 for none
void ViewScale(float xy);

}  // namespace d3d
