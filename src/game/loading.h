#pragma once
// Mission loading screen and relaunch hand-off (loading.cpp), called from the ShowScreen hook in hud.cpp
struct X86Context;

namespace game::loading {

// before and after the render device's ShowScreen(path, persist, fade, clear): notes the picture it loads, and in an
// instance started by a relaunch loads the previous instance's last picture again in place of the kept display
void BeforeShowScreen(X86Context& ctx);
void AfterShowScreen(X86Context& ctx);

}  // namespace game::loading
