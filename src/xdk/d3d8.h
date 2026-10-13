#pragma once
#include <mutex>

#include "gpu/state.h"

namespace d3d {

gpu::State& State();
std::recursive_mutex& Lock();
// ambient occlusion over the scene drawn so far; m: its D3D perspective projection; mode: a gpu::AoMode
void Occlusion(const float* m, uint32_t mode);
// draws from here on are the sky zone's, seen from its own camera
void SkyScene(bool on);
// draws from here on are magnified about the viewport's centre, 1 for none
void ViewScale(float xy);

}  // namespace d3d
