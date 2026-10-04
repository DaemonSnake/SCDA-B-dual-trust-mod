// Field of view. Every camera takes its angle through this constructor, zooms included, so all keep their ratio.
#include <cmath>

#include "app/app.h"
#include "xdk/hle.h"

using namespace kernel;

// FCameraSceneNode(viewport, actor, location, rotation, fov in degrees), thiscall ret 0x24
X86_EXTERN_FUNC(__imp__sub_002898CB);
X86_FUNC(sub_002898CB) {
    constexpr float kHalf = 3.14159265f / 360.0f;
    float& fov = G<float>(ctx.esp + 0x24);
    if (float s = app::FovScale(); s != 1.0f) fov = std::atan(std::tan(fov * kHalf) * s) / kHalf;
    __imp__sub_002898CB(ctx, base);
}
