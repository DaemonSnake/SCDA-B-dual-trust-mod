// The title's Direct3D 8 calls drawn with Direct3D 12
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include "app/app.h"
#include "core/log.h"
#include "gpu/backend.h"
#include "gpu/shaders.h"
#include "gpu/texture.h"
#include "gpu/vertex.h"
#include "kernel/kernel.h"

using Microsoft::WRL::ComPtr;

namespace gpu {

namespace {

constexpr int kFrames = 3;
constexpr uint64_t kUploadSize = 64ull << 20;
constexpr uint32_t kSrvCount = 3 * 32768;
constexpr uint32_t kSamplerCount = 2048;
constexpr uint32_t kRtvCount = 1024, kDsvCount = 256;

const char* kBlitHlsl = R"(
cbuffer B : register(b1) { float4 uvrect; float4 opts; };
Texture2D img : register(t0);
Texture2D lut : register(t1);
SamplerState ls : register(s0);
SamplerState ps : register(s1);
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O vs(uint id : SV_VertexID) {
    O o;
    float2 t = float2((id << 1) & 2, id & 2);
    o.p = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uvrect.xy + t * uvrect.zw;
    return o;
}
float4 psmain(O i) : SV_Target {
    float4 c = img.Sample(ls, i.uv);
    if (opts.x > 0) c.rgb = float3(lut.Sample(ps, float2(c.r, 0.5)).r, lut.Sample(ps, float2(c.g, 0.5)).g, lut.Sample(ps, float2(c.b, 0.5)).b);
    return float4(c.rgb, opts.y > 0 ? c.a : 1);
}
)";

float Bits(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

void Color(uint32_t argb, float* o) {
    o[0] = ((argb >> 16) & 255) / 255.0f;
    o[1] = ((argb >> 8) & 255) / 255.0f;
    o[2] = (argb & 255) / 255.0f;
    o[3] = (argb >> 24) / 255.0f;
}

D3D12_BLEND Blend(uint32_t v, bool alpha) {
    switch (v) {
        case 0: return D3D12_BLEND_ZERO;
        case 1: return D3D12_BLEND_ONE;
        case 0x300: return alpha ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_SRC_COLOR;
        case 0x301: return alpha ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_INV_SRC_COLOR;
        case 0x302: return D3D12_BLEND_SRC_ALPHA;
        case 0x303: return D3D12_BLEND_INV_SRC_ALPHA;
        case 0x304: return D3D12_BLEND_DEST_ALPHA;
        case 0x305: return D3D12_BLEND_INV_DEST_ALPHA;
        case 0x306: return alpha ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_DEST_COLOR;
        case 0x307: return alpha ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_INV_DEST_COLOR;
        case 0x308: return D3D12_BLEND_SRC_ALPHA_SAT;
        case 0x8001: case 0x8003: return D3D12_BLEND_BLEND_FACTOR;
        case 0x8002: case 0x8004: return D3D12_BLEND_INV_BLEND_FACTOR;
        default: return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP BlendOp(uint32_t v) {
    switch (v) {
        case 0x800A: return D3D12_BLEND_OP_SUBTRACT;
        case 0x800B: return D3D12_BLEND_OP_REV_SUBTRACT;
        case 0x8007: return D3D12_BLEND_OP_MIN;
        case 0x8008: return D3D12_BLEND_OP_MAX;
        default: return D3D12_BLEND_OP_ADD;
    }
}

D3D12_COMPARISON_FUNC Cmp(uint32_t v) {
    uint32_t n = v >= 0x200 && v <= 0x207 ? v - 0x200 : 7;
    return D3D12_COMPARISON_FUNC(n + 1);
}

D3D12_STENCIL_OP StencilOp(uint32_t v) {
    switch (v) {
        case 0: return D3D12_STENCIL_OP_ZERO;
        case 0x1E01: return D3D12_STENCIL_OP_REPLACE;
        case 0x1E02: return D3D12_STENCIL_OP_INCR_SAT;
        case 0x1E03: return D3D12_STENCIL_OP_DECR_SAT;
        case 0x150A: return D3D12_STENCIL_OP_INVERT;
        case 0x8507: return D3D12_STENCIL_OP_INCR;
        case 0x8508: return D3D12_STENCIL_OP_DECR;
        default: return D3D12_STENCIL_OP_KEEP;
    }
}

D3D12_TEXTURE_ADDRESS_MODE Address(uint32_t v) {
    switch (v) {
        case 2: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case 3: case 5: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case 4: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

DXGI_FORMAT HostFormat(Host h) {
    switch (h) {
        case Host::BC1: return DXGI_FORMAT_BC1_UNORM;
        case Host::BC2: return DXGI_FORMAT_BC2_UNORM;
        case Host::BC3: return DXGI_FORMAT_BC3_UNORM;
        case Host::R32F: return DXGI_FORMAT_R32_FLOAT;
        default: return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
}

struct Target {
    ComPtr<ID3D12Resource> res;
    bool depth = false;
    uint32_t gw = 0, gh = 0, w = 0, h = 0;
    uint32_t view = 0;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    uint32_t data = 0, bytes = 0;
    uint64_t epoch = 0, sig = 0;  // last lock seen, guest memory as last matched
    bool drawn = false;           // the GPU wrote it since the last readback
};

uint64_t MemSig(uint32_t data, uint32_t total) {
    const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(data));
    uint64_t sig = Hash(&total, 4);
    if (total <= 65536) return Hash(p, total, sig);
    for (uint32_t i = 0; i < 64; i++) sig = Hash(p + uint64_t(total - 64) * i / 63, 64, sig);
    return sig;
}

// a lock alone proves nothing, reads lock too: only changed bytes mean the CPU wrote it
bool CpuWrote(Target& t) {
    uint64_t e = DirtyEpoch(t.data);
    if (e <= t.epoch) return false;
    t.epoch = e;
    uint64_t sig = MemSig(t.data, t.bytes);
    if (sig == t.sig) return false;
    t.sig = sig;
    return true;
}

struct Tex {
    ComPtr<ID3D12Resource> res;
    TextureDesc d;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    uint32_t levels = 1;
    uint64_t sig = 0, frame = 0;
};

struct PsoKey {
    uint64_t vs, ps;
    uint32_t regs, topo, has_rt, has_ds;
    uint32_t blend, src, dst, op, wmask;
    uint32_t zen, zwrite, zfunc;
    uint32_t sten, sfunc, smask, swmask, sfail, szfail, spass;
    uint32_t cull, ccw, fill;
    int32_t zbias;
};

class D3D12Backend final : public Backend {
public:
    bool Init(uint32_t width, uint32_t height) override;
    void Clear(const State& s, uint32_t count, uint32_t rects, uint32_t flags, uint32_t color, float z, uint32_t stencil) override;
    void Draw(const State& s, uint32_t prim, uint32_t start, uint32_t count, uint32_t indices) override;
    void Copy(const Surface& src, const Surface& dst, uint32_t count, uint32_t src_rects, uint32_t dst_points) override;
    void Present(const Surface& front) override;
    void Sync(uint32_t data, uint32_t size) override;
    void Flush() override;
    void SetGamma(const uint8_t* ramp) override;

private:
    struct Frame {
        ComPtr<ID3D12CommandAllocator> alloc;
        ComPtr<ID3D12Resource> upload;
        uint8_t* map = nullptr;
        uint64_t used = 0, fence = 0;
        std::vector<ComPtr<ID3D12Resource>> garbage;
        uint32_t srv = 0;
    };
    struct Alloc {
        ID3D12Resource* res;
        uint64_t off;
        uint8_t* cpu;
        D3D12_GPU_VIRTUAL_ADDRESS gpu;
    };

    Alloc Upload(uint64_t size, uint64_t align = 256);
    uint32_t SrvSlots(uint32_t n);
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(ID3D12DescriptorHeap* h, uint32_t i, uint32_t inc) {
        auto c = h->GetCPUDescriptorHandleForHeapStart();
        c.ptr += size_t(i) * inc;
        return c;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(ID3D12DescriptorHeap* h, uint32_t i, uint32_t inc) {
        auto g = h->GetGPUDescriptorHandleForHeapStart();
        g.ptr += uint64_t(i) * inc;
        return g;
    }
    void Transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES want);
    void Begin();
    void Submit(bool wait);
    Target* GetTarget(const Surface& s, bool depth, bool create = true);
    void UploadTarget(Target& t, const Surface& s);
    void Blit(ID3D12Resource* src, DXGI_FORMAT fmt, const float uvrect[4], bool gamma, bool alpha);
    bool BindTargets(const State& s);
    ID3D12Resource* BindTexture(const State& s, int stage, TexInfo& info, D3D12_SHADER_RESOURCE_VIEW_DESC& srv);
    uint32_t Samplers(const State& s, const TexInfo info[4]);
    ID3DBlob* Compile(const std::string& src, const char* entry, const char* profile, uint64_t& hash);
    ID3D12PipelineState* Pipeline(const PsoKey& k, ID3DBlob* vs, ID3DBlob* ps);
    ID3D12PipelineState* BlitPipeline();
    void ResizeSwap();
    float Scale() const { return scale_; }

    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> dev_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_;
    ComPtr<ID3D12Resource> bb_[kFrames];
    ComPtr<ID3D12Fence> fence_;
    HANDLE event_ = nullptr;
    uint64_t fence_value_ = 0;
    Frame frames_[kFrames];
    uint64_t frame_ = 0;
    ComPtr<ID3D12GraphicsCommandList> cl_;
    ComPtr<ID3D12DescriptorHeap> srv_heap_, smp_heap_, rtv_heap_, dsv_heap_, ui_heap_;
    uint32_t srv_inc_ = 0, smp_inc_ = 0, rtv_inc_ = 0, dsv_inc_ = 0;
    std::vector<uint32_t> free_rtv_, free_dsv_;
    ComPtr<ID3D12RootSignature> root_;
    std::unordered_map<uint64_t, ComPtr<ID3DBlob>> blobs_;
    std::unordered_map<uint64_t, uint64_t> vs_src_;  // program key to source hash
    std::unordered_map<uint64_t, ComPtr<ID3D12PipelineState>> psos_;
    std::unordered_map<uint64_t, uint32_t> samplers_;
    uint32_t smp_used_ = 0;
    std::unordered_map<uint64_t, Target> targets_;
    std::unordered_map<uint32_t, Tex> textures_;
    ComPtr<ID3D12PipelineState> blit_pso_;
    ComPtr<ID3D12Resource> lut_;
    D3D12_RESOURCE_STATES lut_state_ = D3D12_RESOURCE_STATE_COPY_DEST;
    uint8_t ramp_[768] = {};
    bool ramp_dirty_ = false, gamma_ = false;
    Target* bound_rt_ = nullptr;
    Target* bound_ds_ = nullptr;
    uint32_t guest_w_ = 640, guest_h_ = 480, bb_w_ = 0, bb_h_ = 0;
    float scale_ = 1.0f;
    bool tearing_ = false;
    uint64_t presents_ = 0;
    double fps_ = 0;
    std::chrono::steady_clock::time_point fps_t_ = std::chrono::steady_clock::now();
    uint64_t fps_n_ = 0;
};

bool D3D12Backend::Init(uint32_t width, uint32_t height) {
    guest_w_ = width;
    guest_h_ = height;
    HWND hwnd = static_cast<HWND>(app::Window());
    if (GetEnvironmentVariableW(L"SCDA_D3D12_DEBUG", nullptr, 0)) {
        ComPtr<ID3D12Debug> dbg;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
    }
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)))) return false;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev_)))) {
        LOGE("d3d12: no device");
        return false;
    }
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    dev_->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue_));
    ComPtr<IDXGIFactory5> f5;
    BOOL tear = FALSE;
    if (SUCCEEDED(factory_.As(&f5))) f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tear, sizeof(tear));
    tearing_ = tear;

    RECT rc;
    GetClientRect(hwnd, &rc);
    bb_w_ = std::max<LONG>(rc.right - rc.left, 1);
    bb_h_ = std::max<LONG>(rc.bottom - rc.top, 1);
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = bb_w_;
    sd.Height = bb_h_;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = kFrames;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(factory_->CreateSwapChainForHwnd(queue_.Get(), hwnd, &sd, nullptr, nullptr, &sc1))) {
        LOGE("d3d12: no swap chain");
        return false;
    }
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    sc1.As(&swap_);

    auto heap = [&](D3D12_DESCRIPTOR_HEAP_TYPE t, uint32_t n, bool visible, ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = t;
        hd.NumDescriptors = n;
        hd.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dev_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&out));
        return dev_->GetDescriptorHandleIncrementSize(t);
    };
    srv_inc_ = heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kSrvCount, true, srv_heap_);
    smp_inc_ = heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kSamplerCount, true, smp_heap_);
    rtv_inc_ = heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvCount + kFrames, false, rtv_heap_);
    dsv_inc_ = heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kDsvCount, false, dsv_heap_);
    heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true, ui_heap_);
    for (uint32_t i = kRtvCount; i-- > 0;) free_rtv_.push_back(i);
    for (uint32_t i = kDsvCount; i-- > 0;) free_dsv_.push_back(i);
    for (int i = 0; i < kFrames; i++) {
        swap_->GetBuffer(i, IID_PPV_ARGS(&bb_[i]));
        dev_->CreateRenderTargetView(bb_[i].Get(), nullptr, Cpu(rtv_heap_.Get(), kRtvCount + i, rtv_inc_));
    }

    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 4;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    ranges[1].NumDescriptors = 4;
    D3D12_ROOT_PARAMETER params[4] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].Descriptor.ShaderRegister = 1;
    for (int i = 0; i < 2; i++) {
        params[2 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2 + i].DescriptorTable.NumDescriptorRanges = 1;
        params[2 + i].DescriptorTable.pDescriptorRanges = &ranges[i];
        params[2 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = 4;
    rsd.pParameters = params;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> rs, err;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rs, &err))) return false;
    dev_->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&root_));

    for (auto& f : frames_) {
        dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.alloc));
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_UPLOAD};
        D3D12_RESOURCE_DESC bd{};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = kUploadSize;
        bd.Height = bd.DepthOrArraySize = bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&f.upload));
        f.upload->Map(0, nullptr, reinterpret_cast<void**>(&f.map));
    }
    dev_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].alloc.Get(), nullptr, IID_PPV_ARGS(&cl_));

    // gamma ramp as a 256 x 1 lookup
    {
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = 256;
        td.Height = td.DepthOrArraySize = td.MipLevels = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&lut_));
    }

    ImGui_ImplDX12_Init(dev_.Get(), kFrames, DXGI_FORMAT_B8G8R8A8_UNORM, ui_heap_.Get(),
                        ui_heap_->GetCPUDescriptorHandleForHeapStart(), ui_heap_->GetGPUDescriptorHandleForHeapStart());

    scale_ = app::RenderScale(guest_h_);
    Begin();
    LOGI("d3d12: %ux%u guest, %ux%u window, render scale %.3f", width, height, bb_w_, bb_h_, scale_);
    return true;
}

void D3D12Backend::Begin() {
    ID3D12DescriptorHeap* heaps[] = {srv_heap_.Get(), smp_heap_.Get()};
    cl_->SetDescriptorHeaps(2, heaps);
    cl_->SetGraphicsRootSignature(root_.Get());
    bound_rt_ = bound_ds_ = nullptr;
}

void D3D12Backend::Submit(bool wait) {
    cl_->Close();
    ID3D12CommandList* lists[] = {cl_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    queue_->Signal(fence_.Get(), ++fence_value_);
    Frame& f = frames_[frame_ % kFrames];
    f.fence = fence_value_;
    if (wait) {
        fence_->SetEventOnCompletion(fence_value_, event_);
        WaitForSingleObject(event_, INFINITE);
        f.alloc->Reset();
        f.garbage.clear();
        f.used = 0;
        cl_->Reset(f.alloc.Get(), nullptr);
        Begin();
    }
}

void D3D12Backend::Flush() { Submit(true); }

D3D12Backend::Alloc D3D12Backend::Upload(uint64_t size, uint64_t align) {
    Frame& f = frames_[frame_ % kFrames];
    uint64_t o = (f.used + align - 1) & ~(align - 1);
    if (o + size <= kUploadSize) {
        f.used = o + size;
        return {f.upload.Get(), o, f.map + o, f.upload->GetGPUVirtualAddress() + o};
    }
    // too big for the ring: a buffer of its own, released with the frame
    ComPtr<ID3D12Resource> r;
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC bd{};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = (size + 0xFFFF) & ~0xFFFFull;
    bd.Height = bd.DepthOrArraySize = bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&r));
    uint8_t* p = nullptr;
    r->Map(0, nullptr, reinterpret_cast<void**>(&p));
    f.garbage.push_back(r);
    return {r.Get(), 0, p, r->GetGPUVirtualAddress()};
}

uint32_t D3D12Backend::SrvSlots(uint32_t n) {
    Frame& f = frames_[frame_ % kFrames];
    const uint32_t per = kSrvCount / kFrames;
    if (f.srv + n > per) {
        LOG_FIRST(4, "d3d12: descriptor ring wrapped within a frame");
        f.srv = 0;
    }
    uint32_t i = uint32_t(frame_ % kFrames) * per + f.srv;
    f.srv += n;
    return i;
}

void D3D12Backend::Transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES want) {
    if (cur == want) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = cur;
    b.Transition.StateAfter = want;
    cl_->ResourceBarrier(1, &b);
    cur = want;
}

Target* D3D12Backend::GetTarget(const Surface& s, bool depth, bool create) {
    if (!s.header) return nullptr;
    uint32_t data = mem::VirtualFromPhysical(s.data);
    uint64_t key = uint64_t(data) << 1 | (depth ? 1 : 0);
    uint32_t gw, gh;
    SurfaceSize(s, gw, gh);
    auto it = targets_.find(key);
    if (it != targets_.end() && it->second.gw == gw && it->second.gh == gh) return &it->second;
    if (!create) return nullptr;
    if (it != targets_.end()) {
        if (bound_rt_ == &it->second || bound_ds_ == &it->second) bound_rt_ = bound_ds_ = nullptr;
        (it->second.depth ? free_dsv_ : free_rtv_).push_back(it->second.view);
        frames_[frame_ % kFrames].garbage.push_back(it->second.res);
        targets_.erase(it);
    }
    Target t;
    t.depth = depth;
    t.gw = gw;
    t.gh = gh;
    t.w = std::max(1u, uint32_t(std::lround(gw * scale_)));
    t.h = std::max(1u, uint32_t(std::lround(gh * scale_)));
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = t.w;
    td.Height = t.h;
    td.DepthOrArraySize = td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    D3D12_CLEAR_VALUE cv{};
    if (depth) {
        td.Format = DXGI_FORMAT_R24G8_TYPELESS;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        cv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        cv.DepthStencil.Depth = 1;
        t.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    } else {
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        cv.Format = td.Format;
        t.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, t.state, &cv, IID_PPV_ARGS(&t.res)))) return nullptr;
    auto& pool = depth ? free_dsv_ : free_rtv_;
    if (pool.empty()) return nullptr;
    t.view = pool.back();
    pool.pop_back();
    if (depth) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dev_->CreateDepthStencilView(t.res.Get(), &dv, Cpu(dsv_heap_.Get(), t.view, dsv_inc_));
        float one = 1;
        cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t.view, dsv_inc_), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, one, 0, 0, nullptr);
    } else {
        dev_->CreateRenderTargetView(t.res.Get(), nullptr, Cpu(rtv_heap_.Get(), t.view, rtv_inc_));
        float black[4] = {0, 0, 0, 0};
        cl_->ClearRenderTargetView(Cpu(rtv_heap_.Get(), t.view, rtv_inc_), black, 0, nullptr);
    }
    t.data = data;
    t.bytes = ((gw * 4 + 63) & ~63u) * gh;
    if (depth) t.bytes = gw * gh * 4;
    t.epoch = DirtyEpoch(data);
    t.sig = MemSig(data, t.bytes);
    auto& ref = targets_[key] = std::move(t);
    return &ref;
}

bool D3D12Backend::BindTargets(const State& s) {
    Target* rt = GetTarget(s.rt, false);
    Target* ds = s.rs[kRsZEnable] || s.rs[kRsStencilEnable] ? GetTarget(s.zs, true) : nullptr;
    if (rt && ds && (rt->w != ds->w || rt->h != ds->h)) ds = nullptr;
    if (!rt && !ds) return false;
    if (rt) {
        if (CpuWrote(*rt)) UploadTarget(*rt, s.rt);
        Transition(rt->res.Get(), rt->state, D3D12_RESOURCE_STATE_RENDER_TARGET);
        rt->drawn = true;
    }
    if (ds) {
        Transition(ds->res.Get(), ds->state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        ds->drawn = true;
    }
    if (rt != bound_rt_ || ds != bound_ds_) {
        D3D12_CPU_DESCRIPTOR_HANDLE rh = rt ? Cpu(rtv_heap_.Get(), rt->view, rtv_inc_) : D3D12_CPU_DESCRIPTOR_HANDLE{};
        D3D12_CPU_DESCRIPTOR_HANDLE dh = ds ? Cpu(dsv_heap_.Get(), ds->view, dsv_inc_) : D3D12_CPU_DESCRIPTOR_HANDLE{};
        cl_->OMSetRenderTargets(rt ? 1 : 0, rt ? &rh : nullptr, FALSE, ds ? &dh : nullptr);
        bound_rt_ = rt;
        bound_ds_ = ds;
    }
    return true;
}

// CPU pixels of a color surface into its host target, scaled
void D3D12Backend::UploadTarget(Target& t, const Surface& s) {
    TextureDesc d;
    if (!Describe(s.header, d) || Format(d.format).depth) return;
    std::vector<uint8_t> px;
    uint32_t pitch;
    DecodeLevel(d, 0, 0, static_cast<const uint8_t*>(kernel::HostPtr(d.data)), nullptr, px, pitch);
    uint32_t w = d.width, h = d.height;
    ComPtr<ID3D12Resource> tex;
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = w;
    td.Height = h;
    td.DepthOrArraySize = td.MipLevels = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
    uint32_t row = (w * 4 + 255) & ~255u;
    Alloc a = Upload(uint64_t(row) * h, 512);
    for (uint32_t y = 0; y < h; y++) std::memcpy(a.cpu + size_t(y) * row, &px[size_t(y) * pitch], w * 4);
    D3D12_TEXTURE_COPY_LOCATION dst{tex.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    D3D12_TEXTURE_COPY_LOCATION src{a.res, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    src.PlacedFootprint.Offset = a.off;
    src.PlacedFootprint.Footprint = {DXGI_FORMAT_B8G8R8A8_UNORM, w, h, 1, row};
    cl_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    D3D12_RESOURCE_STATES st = D3D12_RESOURCE_STATE_COPY_DEST;
    Transition(tex.Get(), st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rh = Cpu(rtv_heap_.Get(), t.view, rtv_inc_);
    cl_->OMSetRenderTargets(1, &rh, FALSE, nullptr);
    bound_rt_ = bound_ds_ = nullptr;
    D3D12_VIEWPORT vp{0, 0, float(t.w), float(t.h), 0, 1};
    D3D12_RECT sc{0, 0, LONG(t.w), LONG(t.h)};
    cl_->RSSetViewports(1, &vp);
    cl_->RSSetScissorRects(1, &sc);
    float uv[4] = {0, 0, 1, 1};
    Blit(tex.Get(), DXGI_FORMAT_B8G8R8A8_UNORM, uv, false, true);
    frames_[frame_ % kFrames].garbage.push_back(tex);
    t.epoch = DirtyEpoch(t.data);
    t.sig = MemSig(t.data, t.bytes);
}

void D3D12Backend::Blit(ID3D12Resource* src, DXGI_FORMAT fmt, const float uvrect[4], bool gamma, bool alpha) {
    uint32_t slot = SrvSlots(2);
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = fmt;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    dev_->CreateShaderResourceView(src, &sv, Cpu(srv_heap_.Get(), slot, srv_inc_));
    sv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dev_->CreateShaderResourceView(lut_.Get(), &sv, Cpu(srv_heap_.Get(), slot + 1, srv_inc_));
    // samplers: linear clamp, point clamp
    uint64_t key = 0xB117;
    auto it = samplers_.find(key);
    uint32_t si;
    if (it == samplers_.end()) {
        if (smp_used_ + 4 > kSamplerCount) {
            samplers_.clear();
            smp_used_ = 0;
        }
        si = smp_used_;
        smp_used_ += 4;
        for (int i = 0; i < 4; i++) {
            D3D12_SAMPLER_DESC sd{};
            sd.Filter = i == 0 ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sd.MaxLOD = D3D12_FLOAT32_MAX;
            dev_->CreateSampler(&sd, Cpu(smp_heap_.Get(), si + i, smp_inc_));
        }
        samplers_[key] = si;
    } else {
        si = it->second;
    }
    Alloc cb = Upload(256, 256);
    float c[8] = {uvrect[0], uvrect[1], uvrect[2], uvrect[3], gamma ? 1.0f : 0.0f, alpha ? 1.0f : 0.0f, 0, 0};
    std::memcpy(cb.cpu, c, sizeof(c));
    cl_->SetPipelineState(BlitPipeline());
    cl_->SetGraphicsRootConstantBufferView(0, cb.gpu);
    cl_->SetGraphicsRootConstantBufferView(1, cb.gpu);
    cl_->SetGraphicsRootDescriptorTable(2, Gpu(srv_heap_.Get(), slot, srv_inc_));
    cl_->SetGraphicsRootDescriptorTable(3, Gpu(smp_heap_.Get(), si, smp_inc_));
    cl_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl_->DrawInstanced(3, 1, 0, 0);
}

ID3DBlob* D3D12Backend::Compile(const std::string& src, const char* entry, const char* profile, uint64_t& hash) {
    hash = Hash(src.data(), src.size(), Hash(profile, std::strlen(profile)));
    auto it = blobs_.find(hash);
    if (it != blobs_.end()) return it->second.Get();
    ComPtr<ID3DBlob> code, err;
    HRESULT hr = D3DCompile(src.data(), src.size(), nullptr, nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL1, 0,
                            &code, &err);
    if (FAILED(hr)) {
        LOGE("d3d12: %s compile failed: %s", profile, err ? static_cast<const char*>(err->GetBufferPointer()) : "?");
        LOGE("%s", src.c_str());
        blobs_[hash] = nullptr;
        return nullptr;
    }
    blobs_[hash] = code;
    return code.Get();
}

ID3D12PipelineState* D3D12Backend::BlitPipeline() {
    if (blit_pso_) return blit_pso_.Get();
    uint64_t h;
    ID3DBlob* vs = Compile(kBlitHlsl, "vs", "vs_5_0", h);
    ID3DBlob* ps = Compile(kBlitHlsl, "psmain", "ps_5_0", h);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = root_.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    dev_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&blit_pso_));
    return blit_pso_.Get();
}

ID3D12PipelineState* D3D12Backend::Pipeline(const PsoKey& k, ID3DBlob* vs, ID3DBlob* ps) {
    uint64_t h = Hash(&k, sizeof(k));
    auto it = psos_.find(h);
    if (it != psos_.end()) return it->second.Get();
    D3D12_INPUT_ELEMENT_DESC el[16];
    uint32_t n = 0;
    for (uint32_t r = 0; r < 16; r++)
        if ((k.regs >> r) & 1) {
            el[n] = {"ATTR", r, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, n * 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
            n++;
        }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = root_.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.InputLayout = {el, n};
    auto& b = d.BlendState.RenderTarget[0];
    b.BlendEnable = k.blend;
    b.SrcBlend = Blend(k.src, false);
    b.DestBlend = Blend(k.dst, false);
    b.BlendOp = BlendOp(k.op);
    b.SrcBlendAlpha = Blend(k.src, true);
    b.DestBlendAlpha = Blend(k.dst, true);
    b.BlendOpAlpha = BlendOp(k.op);
    b.RenderTargetWriteMask = UINT8(k.wmask);
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = k.fill == 0x1B01 ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = k.cull ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    d.RasterizerState.FrontCounterClockwise = k.ccw;
    d.RasterizerState.DepthBias = k.zbias;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.DepthStencilState.DepthEnable = k.zen;
    d.DepthStencilState.DepthWriteMask = k.zwrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = Cmp(k.zfunc);
    d.DepthStencilState.StencilEnable = k.sten;
    d.DepthStencilState.StencilReadMask = UINT8(k.smask);
    d.DepthStencilState.StencilWriteMask = UINT8(k.swmask);
    D3D12_DEPTH_STENCILOP_DESC so{StencilOp(k.sfail), StencilOp(k.szfail), StencilOp(k.spass), Cmp(k.sfunc)};
    d.DepthStencilState.FrontFace = d.DepthStencilState.BackFace = so;
    d.PrimitiveTopologyType = k.topo == 0 ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
                             : k.topo == 1 ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
                                           : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = k.has_rt ? 1 : 0;
    d.RTVFormats[0] = k.has_rt ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_UNKNOWN;
    d.DSVFormat = k.has_ds ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(dev_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)))) LOGE("d3d12: pipeline creation failed");
    psos_[h] = pso;
    return pso.Get();
}

ID3D12Resource* D3D12Backend::BindTexture(const State& s, int stage, TexInfo& info, D3D12_SHADER_RESOURCE_VIEW_DESC& sv) {
    info = {};
    sv = {};
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sv.Texture2D.MipLevels = 1;
    TextureDesc d;
    if (!s.textures[size_t(stage)] || !Describe(s.textures[size_t(stage)], d)) return nullptr;
    const FormatInfo& fi = Format(d.format);
    info.kind = d.cube ? kTexCube : d.depth > 1 ? kTex3D : kTex2D;
    info.rect = d.linear;

    // rendered by the GPU: sample the target itself
    auto tit = targets_.find(uint64_t(d.data) << 1 | (fi.depth ? 1 : 0));
    if (tit != targets_.end() && !d.cube && d.depth == 1) {
        Target& t = tit->second;
        if (&t == bound_rt_ || &t == bound_ds_) return nullptr;
        if (!t.depth && CpuWrote(t)) {
            Surface su;
            su.header = s.textures[size_t(stage)];
            UploadTarget(t, su);
        }
        Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sv.Format = t.depth ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_B8G8R8A8_UNORM;
        if (t.depth) info.shadow = d.format == 0x2C || d.format == 0x30 ? 2 : 1;
        info.kind = kTex2D;
        return t.res.Get();
    }

    Tex& tx = textures_[d.data];
    uint32_t total = TotalSize(d);
    bool same = tx.res && tx.d.format == d.format && tx.d.width == d.width && tx.d.height == d.height && tx.d.depth == d.depth &&
                tx.d.levels == d.levels && tx.d.cube == d.cube && tx.d.pitch == d.pitch;
    if (!same) {
        if (tx.res) frames_[frame_ % kFrames].garbage.push_back(tx.res);
        tx = Tex{};
        tx.d = d;
        tx.fmt = HostFormat(fi.host);
        bool bc = fi.compressed;
        uint32_t w = d.width, h = d.height;
        tx.levels = d.levels;
        if (bc && ((w & 3) || (h & 3))) {
            w = (w + 3) & ~3u;
            h = (h + 3) & ~3u;
            tx.levels = 1;
        }
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC td{};
        td.Dimension = d.depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = UINT16(d.cube ? 6 : d.depth);
        td.MipLevels = UINT16(tx.levels);
        td.Format = tx.fmt;
        td.SampleDesc.Count = 1;
        if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tx.res)))) {
            textures_.erase(d.data);
            return nullptr;
        }
    }
    // cheap change check once per frame: lock epochs plus a sample of the bytes
    if (tx.frame != presents_ + 1 || !same) {
        tx.frame = presents_ + 1;
        const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(d.data));
        uint64_t sig = MemSig(d.data, total) ^ DirtyEpoch(d.data);
        if (sig != tx.sig || !same) {
            tx.sig = sig;
            D3D12_RESOURCE_STATES st = same ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_DEST;
            Transition(tx.res.Get(), st, D3D12_RESOURCE_STATE_COPY_DEST);
            uint32_t faces = d.cube ? 6 : 1;
            std::vector<uint8_t> px;
            for (uint32_t f = 0; f < faces; f++)
                for (uint32_t l = 0; l < tx.levels; l++) {
                    uint32_t pitch;
                    DecodeLevel(d, l, f, p, nullptr, px, pitch);
                    D3D12_RESOURCE_DESC rd = tx.res->GetDesc();
                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
                    UINT rows;
                    UINT64 row_size, bytes;
                    uint32_t sub = f * tx.levels + l;
                    dev_->GetCopyableFootprints(&rd, sub, 1, 0, &fp, &rows, &row_size, &bytes);
                    Alloc a = Upload(bytes, 512);
                    uint32_t slices = fp.Footprint.Depth;
                    size_t src_rows = pitch ? px.size() / pitch / std::max(slices, 1u) : 0;
                    for (uint32_t z = 0; z < slices; z++)
                        for (uint32_t y = 0; y < rows; y++) {
                            uint8_t* dst = a.cpu + (size_t(z) * rows + y) * fp.Footprint.RowPitch;
                            if (y < src_rows) std::memcpy(dst, &px[(size_t(z) * src_rows + y) * pitch], std::min<size_t>(pitch, row_size));
                            else std::memset(dst, 0, size_t(row_size));
                        }
                    fp.Offset = a.off;
                    D3D12_TEXTURE_COPY_LOCATION dl{tx.res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
                    dl.SubresourceIndex = sub;
                    D3D12_TEXTURE_COPY_LOCATION sl{a.res, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
                    sl.PlacedFootprint = fp;
                    cl_->CopyTextureRegion(&dl, 0, 0, 0, &sl, nullptr);
                }
            Transition(tx.res.Get(), st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
    }
    sv.Format = tx.fmt;
    if (d.cube) {
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        sv.TextureCube.MipLevels = tx.levels;
    } else if (d.depth > 1) {
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        sv.Texture3D.MipLevels = tx.levels;
    } else {
        sv.Texture2D.MipLevels = tx.levels;
    }
    if (fi.depth) info.shadow = d.format == 0x2C || d.format == 0x30 ? 2 : 1;
    return tx.res.Get();
}

uint32_t D3D12Backend::Samplers(const State& s, const TexInfo info[4]) {
    D3D12_SAMPLER_DESC sd[4] = {};
    uint32_t aniso = app::Get().aniso;
    for (int i = 0; i < 4; i++) {
        const auto& t = s.tss[size_t(i)];
        uint32_t mag = t[kTssMagFilter], min = t[kTssMinFilter], mip = t[kTssMipFilter];
        auto lin = [](uint32_t f) { return f >= 2; };
        D3D12_FILTER_TYPE fmin = lin(min) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
        D3D12_FILTER_TYPE fmag = lin(mag) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
        D3D12_FILTER_TYPE fmip = mip == 2 ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
        sd[i].Filter = D3D12_FILTER(fmin << 4 | fmag << 2 | fmip);
        uint32_t an = min == 3 || mag == 3 ? std::max<uint32_t>(t[kTssMaxAnisotropy], 1) : 1;
        if (aniso && lin(min) && !info[i].shadow) an = aniso;
        if (an > 1) {
            sd[i].Filter = D3D12_FILTER_ANISOTROPIC;
            sd[i].MaxAnisotropy = std::min<uint32_t>(an, 16);
        }
        sd[i].AddressU = Address(t[kTssAddressU]);
        sd[i].AddressV = Address(t[kTssAddressV]);
        sd[i].AddressW = Address(t[kTssAddressW]);
        sd[i].MipLODBias = Bits(t[kTssMipLodBias]);
        sd[i].MinLOD = float(t[kTssMaxMipLevel]);
        sd[i].MaxLOD = mip ? D3D12_FLOAT32_MAX : sd[i].MinLOD;
        Color(t[kTssBorderColor], sd[i].BorderColor);
        if (info[i].shadow) sd[i].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
    uint64_t key = Hash(sd, sizeof(sd));
    auto it = samplers_.find(key);
    if (it != samplers_.end()) return it->second;
    if (smp_used_ + 4 > kSamplerCount) {
        samplers_.clear();
        smp_used_ = 0;
    }
    uint32_t base = smp_used_;
    smp_used_ += 4;
    for (int i = 0; i < 4; i++) dev_->CreateSampler(&sd[i], Cpu(smp_heap_.Get(), base + i, smp_inc_));
    samplers_[key] = base;
    return base;
}

void D3D12Backend::Draw(const State& s, uint32_t prim, uint32_t start, uint32_t count, uint32_t indices) {
    Assembled a;
    if (!Assemble(s, prim, start, count, indices, a)) return;
    if (!BindTargets(s)) return;

    // shaders
    bool prog = (s.vs_handle & 1) && s.vs;
    uint64_t vkey = prog ? Hash(s.vs->program.data(), s.vs->program.size() * 4, a.regs) : Hash(&s.vs_handle, 4, a.regs | 0x10000);
    if (!prog && a.pretransformed) vkey ^= 0x5A5A;
    uint64_t vh;
    ID3DBlob* vs;
    auto vit = vs_src_.find(vkey);
    if (vit != vs_src_.end()) {
        vh = vit->second;
        vs = blobs_[vh].Get();
    } else {
        std::string src = prog ? VertexProgramHlsl(s.vs->program, a.regs) : FixedVertexHlsl(a.regs, a.pretransformed);
        vs = Compile(src, "main", "vs_5_0", vh);
        vs_src_[vkey] = vh;
    }
    TexInfo info[4];
    D3D12_SHADER_RESOURCE_VIEW_DESC sv[4];
    ID3D12Resource* res[4];
    for (int i = 0; i < 4; i++) res[i] = BindTexture(s, i, info[i], sv[i]);
    // textures may have recorded copies that unbound the targets
    if (!BindTargets(s)) return;
    PixelKey pk = MakePixelKey(s, info);
    uint64_t pkh = Hash(&pk, sizeof(pk));
    uint64_t ph;
    ID3DBlob* ps;
    auto pit = vs_src_.find(pkh ^ 0x9E3779B97F4A7C15ull);
    if (pit != vs_src_.end()) {
        ph = pit->second;
        ps = blobs_[ph].Get();
    } else {
        ps = Compile(PixelHlsl(pk), "main", "ps_5_0", ph);
        vs_src_[pkh ^ 0x9E3779B97F4A7C15ull] = ph;
    }
    if (!vs || !ps) return;

    // pipeline
    auto sm = [&](uint32_t method) { return s.simple[method >> 2]; };
    PsoKey k;
    std::memset(&k, 0, sizeof(k));
    k.vs = vh;
    k.ps = ph;
    k.regs = a.regs;
    k.topo = uint32_t(a.topology);
    k.has_rt = bound_rt_ != nullptr;
    k.has_ds = bound_ds_ != nullptr;
    k.blend = sm(kMBlendEnable) != 0;
    if (k.blend) {
        k.src = sm(kMSrcBlend);
        k.dst = sm(kMDestBlend);
        k.op = sm(kMBlendOp);
    }
    uint32_t cw = sm(kMColorWriteEnable);
    k.wmask = ((cw >> 16) & 1) | ((cw >> 8) & 1) << 1 | (cw & 1) << 2 | ((cw >> 24) & 1) << 3;
    if (k.has_ds) {
        k.zen = s.rs[kRsZEnable] != 0;
        if (k.zen) {
            k.zwrite = sm(kMZWriteEnable) != 0;
            k.zfunc = sm(kMZFunc);
        }
        k.sten = s.rs[kRsStencilEnable] != 0;
        if (k.sten) {
            k.sfunc = sm(kMStencilFunc);
            k.smask = sm(kMStencilMask) & 0xFF;
            k.swmask = sm(kMStencilWriteMask) & 0xFF;
            k.sfail = s.rs[kRsStencilFail];
            k.szfail = sm(kMStencilZFail);
            k.spass = sm(kMStencilPass);
        }
        k.zbias = -int32_t(s.rs[kRsZBias]) * 16;
    }
    uint32_t cull = s.rs[kRsCullMode];
    k.cull = cull != 0 && a.topology == Topology::Triangles;
    k.ccw = cull == 0x900;  // culling clockwise leaves counter clockwise as the front
    k.fill = s.rs[kRsFillMode];
    ID3D12PipelineState* pso = Pipeline(k, vs, ps);
    if (!pso) return;
    cl_->SetPipelineState(pso);

    // constants
    Alloc vc = Upload(sizeof(s.vsc), 256);
    std::memcpy(vc.cpu, s.vsc.data(), sizeof(s.vsc));
    Alloc dc = Upload(sizeof(DrawConstants), 256);
    DrawConstants c{};
    uint32_t gw, gh;
    SurfaceSize(bound_rt_ ? s.rt : s.zs, gw, gh);
    c.surf[0] = float(gw);
    c.surf[1] = float(gh);
    c.surf[2] = 1.0f / s.zscale;
    if (s.rs[kRsFogEnable]) {
        c.fog[0] = Bits(s.rs[kRsFogStart]);
        c.fog[1] = Bits(s.rs[kRsFogEnd]);
        c.fog[2] = Bits(s.rs[kRsFogDensity]);
        c.fog[3] = float(s.rs[kRsFogTableMode]);
    }
    Color(s.rs[kRsFogColor], c.fog_color);
    c.misc[0] = float(sm(kMAlphaRef) & 0xFF);
    c.misc[1] = sm(kMAlphaTestEnable) ? float(Cmp(sm(kMAlphaFunc)) - 1) : 7.0f;
    c.misc[2] = float(Cmp(s.rs[kRsShadowFunc]) - 1);
    c.misc[3] = s.rs[kRsSpecularEnable] ? 1.0f : 0.0f;
    for (int i = 0; i < 8; i++) {
        Color(sm(kMPsConstant0 + i * 4), c.pc[i * 2]);
        Color(sm(kMPsConstant1 + i * 4), c.pc[i * 2 + 1]);
    }
    Color(sm(kMPsFinalConstant0), c.pc[16]);
    Color(sm(kMPsFinalConstant1), c.pc[17]);
    Color(s.rs[kRsTextureFactor], c.tfactor);
    for (int i = 0; i < 4; i++) {
        TextureDesc d;
        if (s.textures[size_t(i)] && Describe(s.textures[size_t(i)], d)) {
            c.texsize[i][0] = float(d.width);
            c.texsize[i][1] = float(d.height);
        }
        const auto& t = s.tss[size_t(i)];
        c.bump[i][0] = Bits(t[kTssBumpEnvMat00]);
        c.bump[i][1] = Bits(t[kTssBumpEnvMat01]);
        c.bump[i][2] = Bits(t[kTssBumpEnvMat10]);
        c.bump[i][3] = Bits(t[kTssBumpEnvMat11]);
        c.bumpl[i][0] = Bits(t[kTssBumpEnvLScale]);
        c.bumpl[i][1] = Bits(t[kTssBumpEnvLOffset]);
    }
    if (!prog && !a.pretransformed) {
        // world, view, projection, then the viewport inside the target
        auto mul = [](const float* x, const float* y, float* o) {
            for (int r = 0; r < 4; r++)
                for (int col = 0; col < 4; col++)
                    o[r * 4 + col] = x[r * 4] * y[col] + x[r * 4 + 1] * y[4 + col] + x[r * 4 + 2] * y[8 + col] + x[r * 4 + 3] * y[12 + col];
        };
        float wv[16], wvp[16];
        mul(s.transforms[256].data(), s.transforms[2].data(), wv);
        mul(wv, s.transforms[3].data(), wvp);
        const auto& v = s.viewport;
        float sx = float(v.w) / gw, sy = float(v.h) / gh;
        float ox = (2.0f * v.x + v.w) / gw - 1, oy = 1 - (2.0f * v.y + v.h) / gh;
        float vpm[16] = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, v.max_z - v.min_z, 0, ox, oy, v.min_z, 1};
        float full[16];
        mul(wvp, vpm, full);
        // HLSL mul(row vector, matrix) with column major packing wants the transpose
        for (int r = 0; r < 4; r++)
            for (int col = 0; col < 4; col++) c.wvp[col * 4 + r] = full[r * 4 + col];
    }
    std::memcpy(dc.cpu, &c, sizeof(c));
    cl_->SetGraphicsRootConstantBufferView(0, vc.gpu);
    cl_->SetGraphicsRootConstantBufferView(1, dc.gpu);

    // textures and samplers
    uint32_t slot = SrvSlots(4);
    for (int i = 0; i < 4; i++) {
        if (!res[i]) {
            sv[i].ViewDimension = info[i].kind == kTexCube ? D3D12_SRV_DIMENSION_TEXTURECUBE
                                  : info[i].kind == kTex3D ? D3D12_SRV_DIMENSION_TEXTURE3D
                                                           : D3D12_SRV_DIMENSION_TEXTURE2D;
            sv[i].Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        }
        dev_->CreateShaderResourceView(res[i], &sv[i], Cpu(srv_heap_.Get(), slot + i, srv_inc_));
    }
    cl_->SetGraphicsRootDescriptorTable(2, Gpu(srv_heap_.Get(), slot, srv_inc_));
    cl_->SetGraphicsRootDescriptorTable(3, Gpu(smp_heap_.Get(), Samplers(s, info), smp_inc_));

    // viewport covers the target, the guest viewport is in the shader; scissor clips to it
    float W = float(bound_rt_ ? bound_rt_->w : bound_ds_->w), H = float(bound_rt_ ? bound_rt_->h : bound_ds_->h);
    D3D12_VIEWPORT vp{0, 0, W, H, 0, 1};
    cl_->RSSetViewports(1, &vp);
    float sc = W / gw;
    LONG x0 = LONG(s.viewport.x * sc), y0 = LONG(s.viewport.y * sc), x1 = LONG((s.viewport.x + s.viewport.w) * sc),
         y1 = LONG((s.viewport.y + s.viewport.h) * sc);
    if (s.scissor_count) {
        x0 = std::max(x0, LONG(s.scissors[0] * sc));
        y0 = std::max(y0, LONG(s.scissors[1] * sc));
        x1 = std::min(x1, LONG(s.scissors[2] * sc));
        y1 = std::min(y1, LONG(s.scissors[3] * sc));
    }
    D3D12_RECT rect{std::clamp<LONG>(x0, 0, LONG(W)), std::clamp<LONG>(y0, 0, LONG(H)), std::clamp<LONG>(x1, 0, LONG(W)),
                    std::clamp<LONG>(y1, 0, LONG(H))};
    cl_->RSSetScissorRects(1, &rect);
    float bf[4];
    Color(sm(kMBlendColor), bf);
    cl_->OMSetBlendFactor(bf);
    cl_->OMSetStencilRef(sm(kMStencilRef) & 0xFF);

    // geometry
    uint32_t stride = 0;
    for (int r = 0; r < 16; r++) stride += ((a.regs >> r) & 1) * 16;
    uint64_t vbytes = a.vertices.size() * 4, ibytes = a.indices.size() * 4;
    Alloc vb = Upload(vbytes, 16);
    std::memcpy(vb.cpu, a.vertices.data(), vbytes);
    Alloc ib = Upload(ibytes, 4);
    std::memcpy(ib.cpu, a.indices.data(), ibytes);
    D3D12_VERTEX_BUFFER_VIEW vbv{vb.gpu, UINT(vbytes), stride};
    D3D12_INDEX_BUFFER_VIEW ibv{ib.gpu, UINT(ibytes), DXGI_FORMAT_R32_UINT};
    cl_->IASetVertexBuffers(0, 1, &vbv);
    cl_->IASetIndexBuffer(&ibv);
    cl_->IASetPrimitiveTopology(a.topology == Topology::Points ? D3D_PRIMITIVE_TOPOLOGY_POINTLIST
                                : a.topology == Topology::Lines ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
                                                                : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl_->DrawIndexedInstanced(UINT(a.indices.size()), 1, 0, 0, 0);
}

void D3D12Backend::Clear(const State& s, uint32_t count, uint32_t rects, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
    Target* rt = (flags & kClearTarget) ? GetTarget(s.rt, false) : nullptr;
    Target* ds = (flags & (kClearZ | kClearStencil)) ? GetTarget(s.zs, true) : nullptr;
    uint32_t gw, gh;
    SurfaceSize(rt ? s.rt : s.zs, gw, gh);
    std::vector<D3D12_RECT> rs;
    auto add = [&](Target* t, int x0, int y0, int x1, int y1) {
        float sc = float(t->w) / t->gw;
        rs.push_back({std::clamp<LONG>(LONG(x0 * sc), 0, t->w), std::clamp<LONG>(LONG(y0 * sc), 0, t->h),
                      std::clamp<LONG>(LONG(x1 * sc), 0, t->w), std::clamp<LONG>(LONG(y1 * sc), 0, t->h)});
    };
    for (Target* t : {rt, ds}) {
        if (!t) continue;
        rs.clear();
        if (count && rects) {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t r = rects + i * 16;
                add(t, int32_t(kernel::R32(r)), int32_t(kernel::R32(r + 4)), int32_t(kernel::R32(r + 8)), int32_t(kernel::R32(r + 12)));
            }
        } else {
            const auto& v = s.viewport;
            add(t, int(v.x), int(v.y), int(v.x + v.w), int(v.y + v.h));
        }
        if (t->depth) {
            Transition(t->res.Get(), t->state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            D3D12_CLEAR_FLAGS cf = D3D12_CLEAR_FLAGS(((flags & kClearZ) ? D3D12_CLEAR_FLAG_DEPTH : 0) | ((flags & kClearStencil) ? D3D12_CLEAR_FLAG_STENCIL : 0));
            cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t->view, dsv_inc_), cf, std::clamp(z, 0.0f, 1.0f), UINT8(stencil), UINT(rs.size()), rs.data());
        } else {
            Transition(t->res.Get(), t->state, D3D12_RESOURCE_STATE_RENDER_TARGET);
            float c[4];
            Color(color, c);
            cl_->ClearRenderTargetView(Cpu(rtv_heap_.Get(), t->view, rtv_inc_), c, UINT(rs.size()), rs.data());
        }
        t->drawn = true;
    }
}

void D3D12Backend::Copy(const Surface& src, const Surface& dst, uint32_t count, uint32_t src_rects, uint32_t dst_points) {
    Target* s = GetTarget(src, false, false);
    // pixels the CPU made: the copy happens in memory, textures and targets pick it up from there
    if (!s || !s->drawn) {
        if (!CopySurfaceMemory(src, dst, count, src_rects, dst_points)) LOG_FIRST(8, "d3d12: CopyRects %08X to %08X skipped", src.data, dst.data);
        else if (Target* d = GetTarget(dst, false, false)) UploadTarget(*d, dst);
        return;
    }
    Target* d = GetTarget(dst, false);
    if (!d || s == d) return;
    Transition(s->res.Get(), s->state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(d->res.Get(), d->state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dl{d->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    D3D12_TEXTURE_COPY_LOCATION sl{s->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    float sc = float(d->w) / d->gw;
    uint32_t n = count && src_rects ? count : 1;
    for (uint32_t i = 0; i < n; i++) {
        D3D12_BOX b{0, 0, 0, std::min(s->w, d->w), std::min(s->h, d->h), 1};
        uint32_t dx = 0, dy = 0;
        if (count && src_rects) {
            uint32_t r = src_rects + i * 16;
            b.left = UINT(kernel::R32(r) * sc);
            b.top = UINT(kernel::R32(r + 4) * sc);
            b.right = std::min(UINT(kernel::R32(r + 8) * sc), s->w);
            b.bottom = std::min(UINT(kernel::R32(r + 12) * sc), s->h);
            if (dst_points) {
                dx = UINT(kernel::R32(dst_points + i * 8) * sc);
                dy = UINT(kernel::R32(dst_points + i * 8 + 4) * sc);
            } else {
                dx = b.left;
                dy = b.top;
            }
        }
        if (b.right <= b.left || b.bottom <= b.top || dx >= d->w || dy >= d->h) continue;
        b.right = std::min<UINT>(b.right, b.left + d->w - dx);
        b.bottom = std::min<UINT>(b.bottom, b.top + d->h - dy);
        cl_->CopyTextureRegion(&dl, dx, dy, 0, &sl, &b);
    }
    d->drawn = true;
    bound_rt_ = bound_ds_ = nullptr;
}

void D3D12Backend::ResizeSwap() {
    HWND hwnd = static_cast<HWND>(app::Window());
    RECT rc;
    GetClientRect(hwnd, &rc);
    uint32_t w = std::max<LONG>(rc.right - rc.left, 1), h = std::max<LONG>(rc.bottom - rc.top, 1);
    if (w == bb_w_ && h == bb_h_) return;
    Flush();
    for (auto& f : frames_) {
        if (f.fence) {
            fence_->SetEventOnCompletion(f.fence, event_);
            WaitForSingleObject(event_, INFINITE);
        }
    }
    for (auto& b : bb_) b.Reset();
    swap_->ResizeBuffers(kFrames, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    for (int i = 0; i < kFrames; i++) {
        swap_->GetBuffer(i, IID_PPV_ARGS(&bb_[i]));
        dev_->CreateRenderTargetView(bb_[i].Get(), nullptr, Cpu(rtv_heap_.Get(), kRtvCount + i, rtv_inc_));
    }
    bb_w_ = w;
    bb_h_ = h;
}

void D3D12Backend::Present(const Surface& front) {
    ResizeSwap();
    Target* t = GetTarget(front, false);
    if (t && CpuWrote(*t)) UploadTarget(*t, front);
    if (ramp_dirty_) {
        ramp_dirty_ = false;
        Alloc a = Upload(256 * 4 + 512, 512);
        bool identity = true;
        for (int i = 0; i < 256; i++) {
            uint8_t* p = a.cpu + i * 4;
            p[0] = ramp_[512 + i];
            p[1] = ramp_[256 + i];
            p[2] = ramp_[i];
            p[3] = 255;
            identity &= ramp_[i] == i && ramp_[256 + i] == i && ramp_[512 + i] == i;
        }
        gamma_ = !identity;
        Transition(lut_.Get(), lut_state_, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dl{lut_.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        D3D12_TEXTURE_COPY_LOCATION sl{a.res, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
        sl.PlacedFootprint.Offset = a.off;
        sl.PlacedFootprint.Footprint = {DXGI_FORMAT_B8G8R8A8_UNORM, 256, 1, 1, 1024};
        cl_->CopyTextureRegion(&dl, 0, 0, 0, &sl, nullptr);
    }
    Transition(lut_.Get(), lut_state_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    uint32_t bi = swap_->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_STATES bs = D3D12_RESOURCE_STATE_PRESENT;
    Transition(bb_[bi].Get(), bs, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rh = Cpu(rtv_heap_.Get(), kRtvCount + bi, rtv_inc_);
    float black[4] = {0, 0, 0, 1};
    cl_->ClearRenderTargetView(rh, black, 0, nullptr);
    cl_->OMSetRenderTargets(1, &rh, FALSE, nullptr);
    bound_rt_ = bound_ds_ = nullptr;
    if (t) {
        // 16:9, the console reports a widescreen display
        float aspect = 16.0f / 9.0f, w = float(bb_w_), h = float(bb_h_);
        float vw = std::min(w, h * aspect), vh = vw / aspect;
        D3D12_VIEWPORT vp{(w - vw) / 2, (h - vh) / 2, vw, vh, 0, 1};
        D3D12_RECT sc{0, 0, LONG(bb_w_), LONG(bb_h_)};
        cl_->RSSetViewports(1, &vp);
        cl_->RSSetScissorRects(1, &sc);
        Transition(t->res.Get(), t->state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        float uv[4] = {0, 0, 1, 1};
        Blit(t->res.Get(), DXGI_FORMAT_B8G8R8A8_UNORM, uv, gamma_, false);
    }
    {
        std::lock_guard lock(app::UiLock());
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app::DrawUi(fps_);
        ImGui::Render();
        ID3D12DescriptorHeap* uh[] = {ui_heap_.Get()};
        cl_->SetDescriptorHeaps(1, uh);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cl_.Get());
    }
    Transition(bb_[bi].Get(), bs, D3D12_RESOURCE_STATE_PRESENT);
    Submit(false);
    bool vsync = app::Get().vsync;
    swap_->Present(vsync ? 1 : 0, !vsync && tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0);

    presents_++;
    frame_++;
    // textures unused for ten seconds go, levels stream theirs in and out
    if (presents_ % 60 == 0 && textures_.size() > 2048)
        for (auto it = textures_.begin(); it != textures_.end();) {
            if (it->second.frame + 600 < presents_) {
                frames_[frame_ % kFrames].garbage.push_back(it->second.res);
                it = textures_.erase(it);
            } else {
                ++it;
            }
        }
    Frame& f = frames_[frame_ % kFrames];
    if (f.fence && fence_->GetCompletedValue() < f.fence) {
        fence_->SetEventOnCompletion(f.fence, event_);
        WaitForSingleObject(event_, INFINITE);
    }
    f.alloc->Reset();
    f.garbage.clear();
    f.used = 0;
    f.srv = 0;
    cl_->Reset(f.alloc.Get(), nullptr);
    Begin();

    fps_n_++;
    auto now = std::chrono::steady_clock::now();
    double el = std::chrono::duration<double>(now - fps_t_).count();
    if (el >= 0.5) {
        fps_ = fps_n_ / el;
        fps_n_ = 0;
        fps_t_ = now;
    }
    app::Pace();

    float want = app::RenderScale(guest_h_);
    if (std::fabs(want - scale_) > 1e-4f) {
        Flush();
        for (auto& [key, tg] : targets_) (tg.depth ? free_dsv_ : free_rtv_).push_back(tg.view);
        targets_.clear();
        scale_ = want;
        bound_rt_ = bound_ds_ = nullptr;
    }
}

// the CPU reads a surface the GPU drew: bring the pixels back
void D3D12Backend::Sync(uint32_t data, uint32_t) {
    auto it = targets_.find(uint64_t(data) << 1);
    if (it == targets_.end() || !it->second.drawn) return;
    Target& t = it->second;
    if (t.w != t.gw || t.h != t.gh) {
        LOG_FIRST(4, "d3d12: surface %08X read back at a render scale, skipped", data);
        return;
    }
    D3D12_RESOURCE_DESC rd = t.res->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    UINT rows;
    UINT64 row_size, bytes;
    dev_->GetCopyableFootprints(&rd, 0, 1, 0, &fp, &rows, &row_size, &bytes);
    ComPtr<ID3D12Resource> rb;
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_READBACK};
    D3D12_RESOURCE_DESC bd{};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = bytes;
    bd.Height = bd.DepthOrArraySize = bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb)))) return;
    Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dl{rb.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    dl.PlacedFootprint = fp;
    D3D12_TEXTURE_COPY_LOCATION sl{t.res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    cl_->CopyTextureRegion(&dl, 0, 0, 0, &sl, nullptr);
    Flush();
    uint8_t* p = nullptr;
    rb->Map(0, nullptr, reinterpret_cast<void**>(&p));
    // color targets are 32 bit linear in guest memory, the pitch rounds to 64 bytes
    uint32_t pitch = (t.gw * 4 + 63) & ~63u;
    for (uint32_t y = 0; y < t.gh; y++)
        std::memcpy(static_cast<uint8_t*>(kernel::HostPtr(data)) + size_t(y) * pitch, p + size_t(y) * fp.Footprint.RowPitch, t.gw * 4);
    rb->Unmap(0, nullptr);
    t.drawn = false;
    t.sig = MemSig(t.data, t.bytes);
}

void D3D12Backend::SetGamma(const uint8_t* ramp) {
    std::memcpy(ramp_, ramp, sizeof(ramp_));
    ramp_dirty_ = true;
}

}  // namespace

std::unique_ptr<Backend> CreateBackend() { return std::make_unique<D3D12Backend>(); }

}  // namespace gpu
