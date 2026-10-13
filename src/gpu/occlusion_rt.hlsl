// Ray traced ambient occlusion, built offline with dxc for shader model 6.5 inline ray queries. The scene structure
// holds the opaque triangles the frame drew before its occlusion, placed in view space; kAoHlsl in d3d12.cpp blurs
// and applies what this pass writes. The constants, Raw, View and Normal are kAoHlsl's.
cbuffer A : register(b1) { float4 proj; float4 zp; float4 vp; float4 opt; float4 fog; };
#ifdef MS
Texture2DMS<float> depth : register(t0);
Texture2DMS<uint2> stencil : register(t2);
float Raw(int2 p) { return stencil.Load(p, 0).g & 1 ? depth.Load(p, 0) : 1; }
#else
Texture2D<float> depth : register(t0);
Texture2D<uint2> stencil : register(t2);
float Raw(int2 p) { return stencil.Load(int3(p, 0)).g & 1 ? depth.Load(int3(p, 0)) : 1; }
#endif
ByteAddressBuffer verts : register(t1);  // the structure's triangles: clip x, y and w per vertex
RaytracingAccelerationStructure scene : register(t3);
float4 vs(uint id : SV_VertexID) : SV_Position { return float4(float2((id << 1) & 2, id & 2) * float2(2, -2) + float2(-1, 1), 0, 1); }
float3 View(float2 px) {
    int2 p = clamp(int2(px), int2(vp.xy), int2(vp.xy + vp.zw) - 1);
    float z = zp.x / (Raw(p) - zp.y);
    float2 ndc = (p + 0.5 - vp.xy) / vp.zw * float2(2, -2) + float2(-1, 1);
    return float3((ndc - proj.zw) * proj.xy * z, z);
}
static const float kDither[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
float3 Normal(float2 px, float3 c) {
    float3 l = View(px - float2(1, 0)), r = View(px + float2(1, 0));
    float3 u = View(px - float2(0, 1)), d = View(px + float2(0, 1));
    float3 dx = abs(r.z - c.z) < abs(c.z - l.z) ? r - c : c - l;
    float3 dy = abs(d.z - c.z) < abs(c.z - u.z) ? d - c : c - u;
    return normalize(cross(dx, dy));
}
static const int kRays = 16;
static const uint kRayFlags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;
// a hit occludes fully up to the inner 38.5% of the radius, fading out over the rest like GTAO's falloff
static const float kFalloff = 0.615;
static const uint kTriangleBytes = 36;
// the triangle under a pixel lies within this share of the depth's distance
static const float kDepthMatch = 0.02;
// rays start this share of the view depth off the surface: the triangle's plane, else the depth buffer's precision
static const float kTriangleBias = 0.0005;
static const float kDepthBias = 0.003;
struct Surface {
    float3 p;
    float3 n;
    float bias;
};
// the traced triangle at the pixel's depth: its exact plane, where depth rebuilt normals wobble at grazing angles
bool HitSurface(float3 c, out Surface s) {
    float dist = length(c);
    RayDesc ray;
    ray.Origin = 0;
    ray.Direction = c / dist;
    ray.TMin = dist * (1 - kDepthMatch);
    ray.TMax = dist * (1 + kDepthMatch);
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    q.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
    // until traversal ends: a single Proceed can leave a nearer hit uncommitted
    while (q.Proceed()) {}
    s.p = c;
    s.n = 0;
    s.bias = kDepthBias;
    if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) return false;
    uint at = q.CommittedPrimitiveIndex() * kTriangleBytes;
    float3x4 m = q.CommittedObjectToWorld3x4();
    float3 a = mul(m, float4(asfloat(verts.Load3(at)), 1));
    float3 b = mul(m, float4(asfloat(verts.Load3(at + 12)), 1));
    float3 e = mul(m, float4(asfloat(verts.Load3(at + 24)), 1));
    s.p = ray.Direction * q.CommittedRayT();
    s.n = normalize(cross(b - a, e - a));
    s.bias = kTriangleBias;
    return true;
}
// opt.x: world radius; out: visibility over cosine weighted rays, each nearest hit weighted by its distance, view z
float2 rtao(float4 pos : SV_Position) : SV_Target {
    float3 c = View(pos.xy);
    if (Raw(int2(pos.xy)) >= 1) return float2(1, c.z);
    Surface s;
    if (!HitSurface(c, s)) s.n = Normal(pos.xy, c);
    float3 n = dot(s.n, s.p) > 0 ? -s.n : s.n;
    // a tangent from whichever axis is not near n
    float3 axis = abs(n.z) < 0.9 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 t = normalize(cross(n, axis));
    float3 b = cross(n, t);
    int2 cell = int2(pos.xy) & 3;
    float turn = kDither[cell.y * 4 + cell.x] * (6.2831853 / 16);
    // a second dither shifts the ray heights, so the blur averages 16 sets
    float lift = kDither[cell.x * 4 + cell.y] / 16;
    RayDesc ray;
    ray.Origin = s.p + n * (s.p.z * s.bias);
    ray.TMin = 0;
    ray.TMax = opt.x;
    float occluded = 0;
    for (int i = 0; i < kRays; i++) {
        // golden angle spiral over the disc, lifted onto the hemisphere: cosine weighted
        float u = (i + lift) / kRays;
        float a = i * 2.3999632 + turn;
        float2 disc = float2(cos(a), sin(a)) * sqrt(u);
        ray.Direction = t * disc.x + b * disc.y + n * sqrt(1 - u);
        RayQuery<kRayFlags> q;
        q.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
        while (q.Proceed()) {}
        if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) continue;
        occluded += saturate((ray.TMax - q.CommittedRayT()) / (ray.TMax * kFalloff));
    }
    return float2(1 - occluded / kRays, c.z);
}
