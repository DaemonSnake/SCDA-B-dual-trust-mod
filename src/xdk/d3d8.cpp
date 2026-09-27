// Direct3D 8 (XDK 5849) entry points the title calls, implemented on the host renderer.
// Resource creation, locking and the XG helpers stay recompiled: they only manage memory.
#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "gpu/backend.h"
#include "gpu/texture.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

using namespace kernel;

namespace d3d {

namespace {

// D3D globals of this build
constexpr uint32_t kDevice = 0x004AB670;       // the static CDevice, g_pDevice points at it
constexpr uint32_t kDevicePtr = 0x004AB668;    // g_pDevice
constexpr uint32_t kIndexData = 0x004AB1C4;    // index data base the inline draws read
constexpr uint32_t kDirtyFlags = 0x004AB1C8;
constexpr uint32_t kTextureState = 0x004AB1D0;  // [4][32]
constexpr uint32_t kRenderState = 0x004AB3D0;   // [166]
constexpr uint32_t kStreams = 0x004ADB10;       // [16] stride, offset, buffer
constexpr uint32_t kSimpleEncode = 0x004F54E8;  // D3DSIMPLERENDERSTATEENCODE in .rdata

// recompiled D3D helpers kept as guest code
constexpr uint32_t kInternalAddRef = 0x0049BD00;
constexpr uint32_t kInternalRelease = 0x0049BF30;
constexpr uint32_t kInitDefaultState = 0x004A1A80;

// CDevice fields mirrored so the recompiled getters and resource code see what the XDK would
constexpr uint32_t kDevPush = 0x00, kDevThreshold = 0x04, kDevBaseVertex = 0x1C, kDevIndexBuffer = 0x7A0,
                   kDevTextures = 0xF88, kDevRenderTarget = 0x1A04, kDevDepth = 0x1A08, kDevSwapCount = 0x2478,
                   kDevFrameBuffer = 0x1A14, kDevFrameBufferSize = 0x1A18, kDevZScale = 0x948, kDevSuperX = 0x95C,
                   kDevSuperY = 0x960;

gpu::State g_state;
std::unique_ptr<gpu::Backend> g_backend;
std::recursive_mutex g_lock;

std::unordered_map<uint32_t, std::shared_ptr<gpu::VertexShader>> g_vs;
std::unordered_map<uint32_t, std::shared_ptr<gpu::PixelShader>> g_ps;
std::array<uint32_t, 3> g_buffers{};  // front, back surfaces
uint32_t g_buffer_count = 2;
uint32_t g_depth = 0;
uint32_t g_fence = 0;
uint32_t g_scratch_push = 0;

void AddRef(uint32_t res) {
    if (res) CallGuest(CurrentContext(), kInternalAddRef, {res});
}
void Release(uint32_t res) {
    if (res) CallGuest(CurrentContext(), kInternalRelease, {res});
}

uint32_t Dev(uint32_t off) { return R32(kDevice + off); }
void SetDev(uint32_t off, uint32_t v) { W32(kDevice + off, v); }

gpu::Surface SurfaceOf(uint32_t header) {
    gpu::Surface s;
    s.header = header;
    if (header) {
        s.data = R32(header + 4);
        s.format = R32(header + 12);
        s.size = R32(header + 16);
    }
    return s;
}

uint32_t NewSurface(uint32_t w, uint32_t h, uint32_t fmt, uint32_t bpp) {
    uint32_t pitch = (w * bpp + 63) & ~63u;
    uint32_t data = mem::AllocContiguous(pitch * h, 0, 0xFFFFFFFF, 0x1000);
    uint32_t hdr = mem::AllocSystem(24);
    W32(hdr + 0, 0x000D0001);  // surface, one reference, one internal reference held by the device
    W32(hdr + 4, mem::PhysicalFromVirtual(data));
    W32(hdr + 8, 0);
    W32(hdr + 12, 0x00010021 | (fmt << 8));  // dma A, 2D, one level
    W32(hdr + 16, (w - 1) | (h - 1) << 12 | (pitch / 64 - 1) << 24);
    W32(hdr + 20, 0);
    return hdr;
}

void SyncDeferred() {
    // deferred states live in the title's memory, the inline SetRenderState and SetTextureStageState write them
    for (uint32_t i = gpu::kRsDeferredFirst; i < gpu::kRsComplexFirst; i++) g_state.rs[i] = R32(kRenderState + i * 4);
    for (int s = 0; s < 4; s++)
        for (uint32_t i = 0; i < gpu::kTssBumpEnvMat00; i++) g_state.tss[s][i] = R32(kTextureState + (s * 32 + i) * 4);
}

float ZScale(const gpu::Surface& zs) {
    uint32_t f = zs.header ? (zs.format >> 8) & 0xFF : 0x2E;
    return f == 0x2C || f == 0x30 ? 65535.0f : 16777215.0f;
}

// c-38 and c-37 hold the viewport scale and offset, as D3D_GetViewportOffsetAndScale computes them
void UpdateViewportConstants() {
    auto& v = g_state.viewport;
    g_state.zscale = ZScale(g_state.zs);
    float hw = v.w * 0.5f, hh = v.h * 0.5f;
    g_state.vsc[58] = {hw, -hh, (v.max_z - v.min_z) * g_state.zscale, 0};
    g_state.vsc[59] = {v.x + g_state.ss_offset_x + hw, v.y + g_state.ss_offset_y + hh, v.min_z * g_state.zscale, 0};
}

void SetComplex(uint32_t rs, uint32_t v) {
    g_state.rs[rs] = v;
    W32(kRenderState + rs * 4, v);
}

}  // namespace

gpu::State& State() { return g_state; }
std::recursive_mutex& Lock() { return g_lock; }

}  // namespace d3d

using namespace d3d;

// ---------------------------------------------------------------- device

HLE_STDCALL(0049D210, Direct3D_CreateDevice, 6) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t pp = a[4];
    uint32_t w = R32(pp), h = R32(pp + 4), fmt = R32(pp + 8), count = std::max<uint32_t>(R32(pp + 12), 1);
    uint32_t auto_depth = R32(pp + 0x20), depth_fmt = R32(pp + 0x24), interval = R32(pp + 0x30);
    if (!w) w = 640;
    if (!h) h = 480;
    LOGI("d3d: CreateDevice %ux%u format %X buffers %u depth %u format %X interval %X flags %X", w, h, fmt, count,
         auto_depth, depth_fmt, interval, R32(pp + 0x28));
    std::memset(HostPtr(kDevice), 0, 0x928 * 4);
    W32(kDevicePtr, kDevice);
    // anything still pushing hardware commands writes into a scratch buffer that MakeSpace rewinds
    g_scratch_push = mem::AllocContiguous(0x10000, 0, 0xFFFFFFFF, 0x1000);
    SetDev(kDevPush, g_scratch_push);
    SetDev(kDevThreshold, g_scratch_push + 0xF000);

    uint32_t color_fmt = gpu::LinearColorFormat(fmt);
    g_buffer_count = std::min<uint32_t>(count + 1, 3);
    for (uint32_t i = 0; i < g_buffer_count; i++) g_buffers[i] = NewSurface(w, h, color_fmt, 4);
    g_depth = 0;
    if (auto_depth) {
        uint32_t zf = gpu::LinearDepthFormat(depth_fmt);
        g_depth = NewSurface(w, h, zf, zf == 0x30 || zf == 0x31 ? 2 : 4);
    }
    if (!g_backend) {
        g_backend = gpu::CreateBackend();
        if (!g_backend->Init(w, h)) LOGE("d3d: renderer init failed");
    }
    g_state = gpu::State{};
    g_state.viewport = {0, 0, w, h, 0.0f, 1.0f};
    SetDev(kDevRenderTarget, g_buffers[1]);
    SetDev(kDevDepth, g_depth);
    SetDev(kDevFrameBuffer, g_buffers[1]);
    SetDev(kDevFrameBufferSize, g_buffers[1]);
    float one = 1.0f, zs = g_depth && (R32(g_depth + 12) >> 8 & 0xFF) == 0x30 ? 65535.0f : 16777215.0f;
    std::memcpy(HostPtr(kDevice + kDevSuperX), &one, 4);
    std::memcpy(HostPtr(kDevice + kDevSuperY), &one, 4);
    std::memcpy(HostPtr(kDevice + kDevZScale), &zs, 4);
    g_state.rt = SurfaceOf(g_buffers[1]);
    g_state.zs = SurfaceOf(g_depth);
    UpdateViewportConstants();
    // the XDK's own default state setup, its pushes land in the scratch buffer
    CallGuest(ctx, kInitDefaultState, {});
    if (a[5]) W32(a[5], kDevice);
    return 0;
}

HLE_STDCALL(0049A5B0, D3DDevice_Release, 0) { return 1; }

// ---------------------------------------------------------------- synchronisation, the renderer never runs behind

HLE_STDCALL(004A17C0, D3D_BlockOnResource, 1) {
    uint32_t res = a[0];
    if (res && g_backend) {
        uint32_t type = R32(res) & 0x70000;
        if (type == 0x40000 || type == 0x50000) {
            uint32_t data = mem::VirtualFromPhysical(R32(res + 4));
            g_backend->Sync(data, 0);
        }
    }
    gpu::MarkDirty(res);
    return 0;
}
HLE_STDCALL(0049BF20, D3DResource_BlockUntilNotBusy, 1) {
    gpu::MarkDirty(a[0]);
    return 0;
}
HLE_STDCALL(0049BE60, D3DResource_IsBusy, 1) { return 0; }
HLE_STDCALL(004A1490, D3D_BlockOnTime, 2) { return 0; }
HLE_STDCALL(0049A600, D3DDevice_BlockOnFence, 1) { return 0; }
HLE_STDCALL(0049A700, D3DDevice_InsertFence, 0) { return ++g_fence; }
HLE_STDCALL(004A1290, D3DDevice_IsFencePending, 1) { return 0; }
HLE_STDCALL(004A13D0, D3D_SetFence, 1) { return ++g_fence; }
HLE_STDCALL(004A17A0, D3D_KickOffAndWaitForIdle, 0) {
    if (g_backend) g_backend->Flush();
    return 0;
}
HLE_FASTCALL(004A12B0, CDevice_KickOff, 0) { return 0; }

HLE_STDCALL(004A1740, D3DDevice_MakeSpace, 0) {
    LOG_FIRST(16, "d3d: MakeSpace from %08X, some caller still pushes hardware commands", R32(ctx.esp));
    SetDev(kDevPush, g_scratch_push);
    return g_scratch_push;
}
HLE_STDCALL(004A1600, D3D_MakeRequestedSpace, 2) {
    LOG_FIRST(16, "d3d: MakeRequestedSpace from %08X", R32(ctx.esp));
    SetDev(kDevPush, g_scratch_push);
    return g_scratch_push;
}

HLE_STDCALL(0049A610, D3DDevice_BlockUntilVerticalBlank, 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    return 0;
}
HLE_STDCALL(0049A650, D3DDevice_SetFlickerFilter, 1) { return 0; }
HLE_STDCALL(0049A6A0, D3DDevice_SetSoftDisplayFilter, 1) { return 0; }
HLE_STDCALL(0049C100, D3DDevice_EnableOverlay, 1) { return 0; }
HLE_STDCALL(0049B800, D3DDevice_PersistDisplay, 0) { return 0; }
HLE_STDCALL(0049B370, D3DDevice_FlushVertexCache, 0) { return 0; }

HLE_STDCALL(0049A3D0, D3DDevice_SetGammaRamp, 2) {
    uint8_t ramp[768];
    std::memcpy(ramp, HostPtr(a[1]), 768);
    if (g_backend) g_backend->SetGamma(ramp);
    return 0;
}

// ---------------------------------------------------------------- targets and presentation

HLE_STDCALL(0049A850, D3DDevice_SetRenderTarget, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t rt = a[0] ? a[0] : Dev(kDevRenderTarget), zs = a[1];
    AddRef(rt);
    uint32_t old_rt = Dev(kDevRenderTarget), old_zs = Dev(kDevDepth);
    SetDev(kDevRenderTarget, rt);
    SetDev(kDevDepth, zs);
    AddRef(zs);
    Release(old_rt);
    Release(old_zs);
    g_state.rt = SurfaceOf(rt);
    g_state.zs = SurfaceOf(zs);
    // SetRenderTarget resets the viewport to the whole target
    uint32_t w, h;
    gpu::SurfaceSize(g_state.rt, w, h);
    g_state.viewport = {0, 0, w, h, 0.0f, 1.0f};
    UpdateViewportConstants();
    return 0;
}

HLE_STDCALL(0049AAD0, D3DDevice_GetBackBuffer2, 1) {
    int32_t i = int32_t(a[0]);
    uint32_t s = i < 0 ? g_buffers[0] : g_buffers[std::min<uint32_t>(uint32_t(i) + 1, g_buffer_count - 1)];
    W32(s, R32(s) + 1);
    return s;
}

HLE_STDCALL(0049AE50, D3DDevice_GetRenderTarget2, 0) {
    uint32_t s = Dev(kDevRenderTarget);
    if (s) W32(s, R32(s) + 1);
    return s;
}

HLE_STDCALL(0049AE70, D3DDevice_GetDepthStencilSurface2, 0) {
    uint32_t s = Dev(kDevDepth);
    if (s) W32(s, R32(s) + 1);
    return s;
}

HLE_STDCALL(0049CC50, D3DDevice_Swap, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t back = g_buffers[1];
    if (g_backend) g_backend->Present(SurfaceOf(back));
    // rotate: the presented buffer becomes the front one
    uint32_t front = g_buffers[0];
    for (uint32_t i = 0; i + 1 < g_buffer_count; i++) g_buffers[i] = g_buffers[i + 1];
    g_buffers[g_buffer_count - 1] = front;
    if (Dev(kDevRenderTarget) == back) {
        Release(back);
        AddRef(g_buffers[1]);
        SetDev(kDevRenderTarget, g_buffers[1]);
        g_state.rt = SurfaceOf(g_buffers[1]);
    }
    SetDev(kDevFrameBuffer, g_buffers[1]);
    SetDev(kDevFrameBufferSize, g_buffers[1]);
    uint32_t n = Dev(kDevSwapCount) + 1;
    SetDev(kDevSwapCount, n);
    return n;
}

HLE_STDCALL(0049CE00, D3DDevice_Clear, 6) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->Clear(g_state, a[0], a[1], a[2], a[3], ArgF(a, 4), a[5]);
    return 0;
}

HLE_STDCALL(0049AB20, D3DDevice_CopyRects, 5) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->Copy(SurfaceOf(a[0]), SurfaceOf(a[3]), a[2], a[1], a[4]);
    return 0;
}

HLE_STDCALL(0049AE90, D3DDevice_SetViewport, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t v = a[0];
    g_state.viewport = {R32(v), R32(v + 4), R32(v + 8), R32(v + 12), GF(v + 16), GF(v + 20)};
    g_state.scissor_count = 0;
    UpdateViewportConstants();
    return 0;
}

HLE_STDCALL(0049B480, D3DDevice_SetScissors, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_state.scissor_count = std::min<uint32_t>(a[0], 8);
    for (uint32_t i = 0; i < g_state.scissor_count * 4; i++) g_state.scissors[i] = R32(a[2] + i * 4);
    return 0;
}

HLE_STDCALL(0049B930, D3DDevice_SetScreenSpaceOffset, 2) {
    g_state.ss_offset_x = ArgF(a, 0);
    g_state.ss_offset_y = ArgF(a, 1);
    UpdateViewportConstants();
    return 0;
}

// ---------------------------------------------------------------- render states

HLE_FASTCALL(0049EFC0, D3DDevice_SetRenderState_Simple, 0) {
    uint32_t method = ctx.ecx & 0x1FFC, value = ctx.edx;
    g_state.simple[method >> 2] = value;
    return 0;
}

HLE_STDCALL(0049EFF0, D3DDevice_SetRenderStateNotInline, 2) {
    uint32_t rs = a[0], v = a[1];
    if (rs < gpu::kRsDeferredFirst) {
        uint32_t method = R32(kSimpleEncode + rs * 4) & 0x1FFC;
        g_state.simple[method >> 2] = v;
    } else if (rs < gpu::kRsMax) {
        SetComplex(rs, v);
    }
    return 0;
}

#define COMPLEX_STATE(addr, name, rs) \
    HLE_STDCALL(addr, D3DDevice_SetRenderState_##name, 1) { \
        SetComplex(rs, a[0]);                               \
        return 0;                                           \
    }
COMPLEX_STATE(0049EF90, PSTextureModes, gpu::kRsPsTextureModes)
COMPLEX_STATE(0049F7E0, VertexBlend, gpu::kRsVertexBlend)
COMPLEX_STATE(0049F340, FogColor, gpu::kRsFogColor)
COMPLEX_STATE(0049F6B0, FillMode, gpu::kRsFillMode)
COMPLEX_STATE(0049F700, BackFillMode, gpu::kRsBackFillMode)
COMPLEX_STATE(0049F760, TwoSidedLighting, gpu::kRsTwoSidedLighting)
COMPLEX_STATE(0049F440, NormalizeNormals, gpu::kRsNormalizeNormals)
COMPLEX_STATE(004A0210, ZEnable, gpu::kRsZEnable)
COMPLEX_STATE(004A02A0, StencilEnable, gpu::kRsStencilEnable)
COMPLEX_STATE(004A0340, StencilFail, gpu::kRsStencilFail)
COMPLEX_STATE(0049F400, FrontFace, gpu::kRsFrontFace)
COMPLEX_STATE(0049F390, CullMode, gpu::kRsCullMode)
COMPLEX_STATE(0049F480, TextureFactor, gpu::kRsTextureFactor)
COMPLEX_STATE(0049F5D0, ZBias, gpu::kRsZBias)
COMPLEX_STATE(0049F650, LogicOp, gpu::kRsLogicOp)
COMPLEX_STATE(0049F2C0, EdgeAntiAlias, gpu::kRsEdgeAntiAlias)
COMPLEX_STATE(004A0580, MultiSampleAntiAlias, gpu::kRsMultiSampleAntiAlias)
COMPLEX_STATE(004A0600, MultiSampleMask, gpu::kRsMultiSampleMask)
COMPLEX_STATE(004A0520, MultiSampleMode, gpu::kRsMultiSampleMode)
COMPLEX_STATE(004A0550, MultiSampleRenderTargetMode, gpu::kRsMultiSampleRenderTargetMode)
COMPLEX_STATE(0049F300, ShadowFunc, gpu::kRsShadowFunc)
COMPLEX_STATE(0049F4E0, LineWidth, gpu::kRsLineWidth)
COMPLEX_STATE(004A0650, SampleAlpha, gpu::kRsSampleAlpha)
COMPLEX_STATE(0049F550, Dxt1NoiseEnable, gpu::kRsDxt1NoiseEnable)
COMPLEX_STATE(004A03B0, YuvEnable, gpu::kRsYuvEnable)
COMPLEX_STATE(004A03E0, OcclusionCullEnable, gpu::kRsOcclusionCullEnable)
COMPLEX_STATE(004A0450, StencilCullEnable, gpu::kRsStencilCullEnable)
COMPLEX_STATE(004A04C0, RopZCmpAlwaysRead, gpu::kRsRopZCmpAlwaysRead)
COMPLEX_STATE(004A04E0, RopZRead, gpu::kRsRopZRead)
COMPLEX_STATE(004A0500, DoNotCullUncompressed, gpu::kRsDoNotCullUncompressed)

HLE_STDCALL(0049F830, D3DDevice_SetTextureState_TexCoordIndex, 2) {
    g_state.tss[a[0] & 3][gpu::kTssTexCoordIndex] = a[1];
    W32(kTextureState + ((a[0] & 3) * 32 + gpu::kTssTexCoordIndex) * 4, a[1]);
    return 0;
}
HLE_STDCALL(0049F940, D3DDevice_SetTextureState_BumpEnv, 3) {
    if (a[1] >= gpu::kTssBumpEnvMat00 && a[1] <= gpu::kTssBumpEnvLOffset) g_state.tss[a[0] & 3][a[1]] = a[2];
    return 0;
}
HLE_STDCALL(0049F9B0, D3DDevice_SetTextureState_BorderColor, 2) {
    g_state.tss[a[0] & 3][gpu::kTssBorderColor] = a[1];
    return 0;
}
HLE_STDCALL(0049F9F0, D3DDevice_SetTextureState_ColorKeyColor, 2) {
    g_state.tss[a[0] & 3][gpu::kTssColorKeyColor] = a[1];
    return 0;
}
HLE_STDCALL(0049FA30, D3DDevice_SetTextureStageStateNotInline, 3) {
    uint32_t stage = a[0] & 3, type = a[1], v = a[2];
    if (type < gpu::kTssCount) {
        g_state.tss[stage][type] = v;
        W32(kTextureState + (stage * 32 + type) * 4, v);
    }
    return 0;
}

HLE_STDCALL(0049A460, D3DDevice_SetTransform, 2) {
    uint32_t t = a[0];
    if (t < g_state.transforms.size()) std::memcpy(g_state.transforms[t].data(), HostPtr(a[1]), 64);
    return 0;
}

HLE_STDCALL(0049A580, D3DDevice_SetMaterial, 1) {
    std::memcpy(g_state.material.data(), HostPtr(a[0]), 17 * 4);
    return 0;
}

// ---------------------------------------------------------------- resources in use

HLE_STDCALL(0049B0C0, D3DDevice_SetTexture, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t stage = a[0] & 3, tex = a[1];
    uint32_t old = Dev(kDevTextures + stage * 4);
    if (tex) CallGuest(ctx, kInternalAddRef, {tex});
    SetDev(kDevTextures + stage * 4, tex);
    if (old) CallGuest(ctx, kInternalRelease, {old});
    g_state.textures[stage] = tex;
    W32(kDirtyFlags, R32(kDirtyFlags) | 0x4800);
    return 0;
}

HLE_STDCALL(0049E9E0, D3DDevice_SetStreamSource, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t n = a[0] & 15, vb = a[1], stride = a[2];
    uint32_t old = R32(kStreams + n * 12 + 8);
    if (vb) CallGuest(ctx, kInternalAddRef, {vb});
    W32(kStreams + n * 12, stride);
    W32(kStreams + n * 12 + 8, vb);
    if (old) CallGuest(ctx, kInternalRelease, {old});
    g_state.streams[n] = {vb, stride};
    return 0;
}

HLE_STDCALL(0049B270, D3DDevice_SetIndices, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t ib = a[0];
    uint32_t old = Dev(kDevIndexBuffer);
    if (ib) CallGuest(ctx, kInternalAddRef, {ib});
    W32(kIndexData, ib ? R32(ib + 4) : 0);
    SetDev(kDevIndexBuffer, ib);
    SetDev(kDevBaseVertex, a[1]);
    if (old) CallGuest(ctx, kInternalRelease, {old});
    g_state.index_buffer = ib;
    g_state.index_data = ib ? R32(ib + 4) : 0;
    g_state.base_vertex = a[1];
    return 0;
}

// ---------------------------------------------------------------- shaders

HLE_STDCALL(0049E770, D3DDevice_CreateVertexShader, 4) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t decl = a[0], func = a[1], out = a[2];
    auto vs = std::make_shared<gpu::VertexShader>();
    if (decl) {
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t t = R32(decl + i * 4);
            vs->decl.push_back(t);
            if (t == 0xFFFFFFFF) break;
        }
    }
    if (func) {
        uint32_t hdr = R32(func);
        vs->program_type = hdr & 0xFFFF;
        uint32_t n = hdr >> 16;
        for (uint32_t i = 0; i < n * 4; i++) vs->program.push_back(R32(func + 4 + i * 4));
    }
    gpu::ParseDeclaration(*vs);
    uint32_t rec = mem::AllocSystem(16, 16);
    vs->handle = rec | 1;
    g_vs[vs->handle] = vs;
    W32(out, vs->handle);
    LOG_FIRST(64, "d3d: vertex shader %08X, %zu decl tokens, %zu instructions, type %X", vs->handle, vs->decl.size(),
              vs->program.size() / 4, vs->program_type);
    return 0;
}

HLE_STDCALL(0049ED10, D3DDevice_DeleteVertexShader, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    auto it = g_vs.find(a[0]);
    if (it != g_vs.end()) {
        mem::FreeSystem(a[0] & ~1u);
        g_vs.erase(it);
    }
    return 0;
}

HLE_STDCALL(0049EE20, D3DDevice_SetVertexShader, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t h = a[0];
    g_state.vs_handle = h;
    g_state.vs = nullptr;
    if (h & 1) {
        auto it = g_vs.find(h);
        if (it != g_vs.end()) {
            g_state.vs = it->second;
            // constants declared with the shader load when it is selected
            for (auto& [reg, v] : it->second->constants)
                if (reg < 192) g_state.vsc[reg] = v;
        } else {
            LOG_FIRST(8, "d3d: SetVertexShader with unknown handle %08X", h);
        }
    }
    return 0;
}

HLE_FASTCALL(0049E8B0, D3DDevice_SetVertexShaderConstant1Fast, 0) {
    uint32_t reg = ctx.ecx;
    if (reg < 192) std::memcpy(g_state.vsc[reg].data(), HostPtr(ctx.edx), 16);
    return 0;
}

HLE_FASTCALL(0049E900, D3DDevice_SetVertexShaderConstantNotInlineFast, 1) {
    uint32_t reg = ctx.ecx, src = ctx.edx, dwords = a[0];
    for (uint32_t i = 0; i < dwords / 4 && reg + i < 192; i++) std::memcpy(g_state.vsc[reg + i].data(), HostPtr(src + i * 16), 16);
    return 0;
}

HLE_STDCALL(0049EAB0, D3DDevice_LoadVertexShader, 2) {
    LOG_FIRST(4, "d3d: LoadVertexShader %08X at %u is not supported", a[0], a[1]);
    return 0;
}
HLE_STDCALL(0049EB10, D3DDevice_SelectVertexShader, 2) {
    LOG_FIRST(4, "d3d: SelectVertexShader %08X at %u is not supported", a[0], a[1]);
    return 0;
}
HLE_STDCALL(0049EBB0, D3DDevice_SetShaderConstantMode, 1) { return 0; }

HLE_STDCALL(004A5C60, D3DDevice_CreatePixelShader, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    auto ps = std::make_shared<gpu::PixelShader>();
    std::memcpy(ps->def, HostPtr(a[0]), sizeof(ps->def));
    uint32_t rec = mem::AllocSystem(16, 16);
    ps->handle = rec;
    g_ps[rec] = ps;
    W32(a[1], rec);
    return 0;
}

HLE_STDCALL(004A5CB0, D3DDevice_DeletePixelShader, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_ps.erase(a[0])) mem::FreeSystem(a[0]);
    return 0;
}

HLE_STDCALL(004A5CD0, D3DDevice_SetPixelShader, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_state.ps = nullptr;
    if (!a[0]) return 0;
    auto it = g_ps.find(a[0]);
    if (it == g_ps.end()) {
        LOG_FIRST(8, "d3d: SetPixelShader with unknown handle %08X", a[0]);
        return 0;
    }
    g_state.ps = it->second;
    // the definition loads the combiner state like the XDK pushes it
    const uint32_t* d = it->second->def;
    auto put = [&](uint32_t method, uint32_t v) { g_state.simple[method >> 2] = v; };
    for (int i = 0; i < 8; i++) {
        put(gpu::kMPsAlphaInputs + i * 4, d[0 + i]);
        put(gpu::kMPsConstant0 + i * 4, d[10 + i]);
        put(gpu::kMPsConstant1 + i * 4, d[18 + i]);
        put(gpu::kMPsAlphaOutputs + i * 4, d[26 + i]);
        put(gpu::kMPsRgbInputs + i * 4, d[34 + i]);
        put(gpu::kMPsRgbOutputs + i * 4, d[45 + i]);
    }
    put(gpu::kMPsFinalAbcd, d[8]);
    put(gpu::kMPsFinalEfgh, d[9]);
    put(gpu::kMPsCompareMode, d[42]);
    put(gpu::kMPsFinalConstant0, d[43]);
    put(gpu::kMPsFinalConstant1, d[44]);
    put(gpu::kMPsCombinerCount, d[53]);
    put(gpu::kMPsDotMapping, d[55]);
    put(gpu::kMPsInputTexture, d[56]);
    SetComplex(gpu::kRsPsTextureModes, d[54]);
    gpu::ApplyPixelConstants(g_state);
    return 0;
}

HLE_STDCALL(004A5ED0, D3DDevice_SetPixelShaderConstant, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t reg = a[0], src = a[1], n = a[2];
    for (uint32_t i = 0; i < n && reg + i < 16; i++) std::memcpy(g_state.psc[reg + i].data(), HostPtr(src + i * 16), 16);
    gpu::ApplyPixelConstants(g_state);
    return 0;
}

// ---------------------------------------------------------------- drawing

HLE_STDCALL(004A61A0, D3DDevice_DrawVertices, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    SyncDeferred();
    if (g_backend) g_backend->Draw(g_state, a[0], a[1], a[2], 0);
    return 0;
}

HLE_STDCALL(004A6240, D3DDevice_DrawIndexedVertices, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    SyncDeferred();
    if (g_backend) g_backend->Draw(g_state, a[0], 0, a[1], a[2]);
    return 0;
}

HLE_STDCALL(004A64E0, D3DDevice_Begin, 1) {
    LOG_FIRST(4, "d3d: Begin/End immediate mode is not supported");
    return 0;
}
HLE_STDCALL(004A6520, D3DDevice_End, 0) { return 0; }
