#include "gpu/shaders.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace gpu {

namespace {

std::string F(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return buf;
}

const char* kCommon = R"(
struct VOut {
    float4 pos : SV_Position;
    float4 d0 : COLOR0;
    float4 d1 : COLOR1;
    float4 b0 : COLOR2;
    float4 b1 : COLOR3;
    float4 t0 : TEXCOORD0;
    float4 t1 : TEXCOORD1;
    float4 t2 : TEXCOORD2;
    float4 t3 : TEXCOORD3;
    float fog : TEXCOORD4;
};
cbuffer VC : register(b0) { float4 c[192]; };
cbuffer DC : register(b1) {
    float4 surf;
    float4 fogp;
    float4 fogcolor;
    float4 misc;
    float4 pc[18];
    float4 tfactor;
    float4 texsize[4];
    float4 bumpmat[4];
    float4 bumplum[4];
    float4x4 wvp;
};
)";

// ---------------------------------------------------------------- vertex programs

const char* kVsHelpers = R"(
float4 xmul(float4 a, float4 b) { return (a == 0 || b == 0) ? (float4)0 : a * b; }
float awayzero(float v) { float m = clamp(abs(v), 5.42101e-20, 1.84467e+19); return v < 0 ? -m : m; }
float4 xrcp(float v) { return (1.0 / v).xxxx; }
float4 xrcc(float v) { return awayzero(1.0 / v).xxxx; }
float4 xrsq(float v) { return v == 0 ? (1.#INF).xxxx : (rsqrt(abs(v))).xxxx; }
float4 xexp(float v) { return float4(exp2(floor(v)), v - floor(v), exp2(v), 1); }
float4 xlog(float v) {
    float t = abs(v);
    if (t == 0) return float4(-1.#INF, 1, -1.#INF, 1);
    float e = floor(log2(t));
    return float4(e, t / exp2(e), log2(t), 1);
}
float4 xlit(float4 s) {
    s.w = clamp(s.w, -(128.0 - 1.0 / 256.0), 128.0 - 1.0 / 256.0);
    s.x = max(s.x, 0);
    s.y = max(s.y, 0);
    return float4(1, s.x, s.x > 0 ? exp2(s.w * log2(s.y)) : 0, 1);
}
float fogfactor(float d) {
    if (fogp.w == 3) return (fogp.y - d) / (fogp.y - fogp.x);
    if (fogp.w == 1) return exp(-d * fogp.z);
    if (fogp.w == 2) return exp(-(d * fogp.z) * (d * fogp.z));
    return 1;
}
)";

struct Tok {
    const uint32_t* t;
    uint32_t f(int sub, int bit, int len) const { return (t[sub] >> bit) & ((1u << len) - 1); }
};

const char* kMask[16] = {"", "w", "z", "zw", "y", "yw", "yz", "yzw", "x", "xw", "xz", "xzw", "xy", "xyw", "xyz", "xyzw"};
const char* kFogMask[16] = {"", "x", "x", "xy", "x", "xy", "xy", "xyz", "x", "xy", "xy", "xyz", "xy", "xyz", "xyz", "xyzw"};
const char* kOut[16] = {"oPos", "", "", "oD0", "oD1", "oFog", "oPts", "oB0", "oB1", "oT0", "oT1", "oT2", "oT3", "", "", ""};

// operand A 0, B 1, C 2
std::string Src(const Tok& k, int which, uint16_t regs) {
    uint32_t neg, sw[4], reg, mux;
    if (which == 0) {
        neg = k.f(1, 8, 1);
        sw[0] = k.f(1, 6, 2), sw[1] = k.f(1, 4, 2), sw[2] = k.f(1, 2, 2), sw[3] = k.f(1, 0, 2);
        reg = k.f(2, 28, 4), mux = k.f(2, 26, 2);
    } else if (which == 1) {
        neg = k.f(2, 25, 1);
        sw[0] = k.f(2, 23, 2), sw[1] = k.f(2, 21, 2), sw[2] = k.f(2, 19, 2), sw[3] = k.f(2, 17, 2);
        reg = k.f(2, 13, 4), mux = k.f(2, 11, 2);
    } else {
        neg = k.f(2, 10, 1);
        sw[0] = k.f(2, 8, 2), sw[1] = k.f(2, 6, 2), sw[2] = k.f(2, 4, 2), sw[3] = k.f(2, 2, 2);
        reg = k.f(2, 0, 2) << 2 | k.f(3, 30, 2), mux = k.f(3, 28, 2);
        uint32_t ilu = k.f(1, 25, 3);
        if (ilu >= 2 && ilu <= 6) sw[1] = sw[2] = sw[3] = sw[0];  // scalar ops read x
    }
    std::string base;
    if (mux == 1) {
        base = reg == 12 ? "oPos" : F("R%u", reg);
    } else if (mux == 2) {
        uint32_t v = k.f(1, 9, 4);
        base = (regs >> v) & 1 ? F("i.v%u", v) : (v == 3 ? "float4(1,1,1,1)" : "float4(0,0,0,1)");
    } else {
        uint32_t cr = k.f(1, 13, 8);
        base = k.f(3, 1, 1) ? F("c[clamp(a0 + %u, 0, 191)]", cr) : F("c[%u]", cr);
    }
    const char* xyzw = "xyzw";
    std::string s = F("(%s(%s).%c%c%c%c)", neg ? "-" : "", base.c_str(), xyzw[sw[0]], xyzw[sw[1]], xyzw[sw[2]], xyzw[sw[3]]);
    return s;
}

std::string MacExpr(uint32_t op, const std::string& a, const std::string& b, const std::string& c) {
    switch (op) {
        case 1: return a;
        case 2: return "xmul(" + a + ", " + b + ")";
        case 3: return "(" + a + " + " + c + ")";
        case 4: return "(xmul(" + a + ", " + b + ") + " + c + ")";
        case 5: return "dot(" + a + ".xyz, " + b + ".xyz).xxxx";
        case 6: return "dot(float4(" + a + ".xyz, 1), " + b + ").xxxx";
        case 7: return "dot(" + a + ", " + b + ").xxxx";
        case 8: return "float4(1, " + a + ".y * " + b + ".y, " + a + ".z, " + b + ".w)";
        case 9: return "min(" + a + ", " + b + ")";
        case 10: return "max(" + a + ", " + b + ")";
        case 11: return "(float4)(" + a + " < " + b + ")";
        case 12: return "(float4)(" + a + " >= " + b + ")";
        case 13: return a;
        default: return "(float4)0";
    }
}

std::string IluExpr(uint32_t op, const std::string& c) {
    switch (op) {
        case 1: return c;
        case 2: return "xrcp(" + c + ".x)";
        case 3: return "xrcc(" + c + ".x)";
        case 4: return "xrsq(" + c + ".x)";
        case 5: return "xexp(" + c + ".x)";
        case 6: return "xlog(" + c + ".x)";
        case 7: return "xlit(" + c + ")";
        default: return "(float4)0";
    }
}

std::string RegDest(uint32_t r) { return r == 12 ? "oPos" : F("R%u", r); }

std::string VsInputStruct(uint16_t regs) {
    std::string s = "struct VIn {\n";
    for (int r = 0; r < 16; r++)
        if ((regs >> r) & 1) s += F("    float4 v%d : ATTR%d;\n", r, r);
    if (!regs) s += "    uint id : SV_VertexID;\n";
    return s + "};\n";
}

const char* kVsEpilogue = R"(
    VOut o;
    float4 p = oPos;
    p.w = awayzero(p.w);
    o.pos.x = p.x * 2 / surf.x - 1;
    o.pos.y = 1 - p.y * 2 / surf.y;
    o.pos.z = p.z * surf.z;
    o.pos.xyz *= p.w;
    o.pos.w = p.w;
    o.d0 = saturate(oD0);
    o.d1 = saturate(oD1);
    o.b0 = saturate(oB0);
    o.b1 = saturate(oB1);
    o.t0 = oT0;
    o.t1 = oT1;
    o.t2 = oT2;
    o.t3 = oT3;
    o.fog = fogfactor(oFog.x);
    return o;
}
)";

const char* kVsPrologue = R"(
    int a0 = 0;
    float4 R0 = 0, R1 = 0, R2 = 0, R3 = 0, R4 = 0, R5 = 0, R6 = 0, R7 = 0, R8 = 0, R9 = 0, R10 = 0, R11 = 0;
    float4 oPos = 0, oD0 = float4(0, 0, 0, 1), oD1 = 0, oB0 = float4(0, 0, 0, 1), oB1 = 0, oFog = 0, oPts = 1;
    float4 oT0 = float4(0, 0, 0, 1), oT1 = float4(0, 0, 0, 1), oT2 = float4(0, 0, 0, 1), oT3 = float4(0, 0, 0, 1);
)";

// ---------------------------------------------------------------- combiners

struct In {
    uint32_t reg, chan, mod;
};
In ParseIn(uint32_t v) { return {v & 0xF, v & 0x10, v & 0xE0}; }

struct Ps {
    const PixelKey& k;
    int stage = 0;
    std::string e, f;  // final combiner E F for EF_PROD
    bool clamp_sum = false, inv_v1 = false, inv_r0 = false;
    std::string code;

    std::string Var(uint32_t reg) {
        uint32_t flags = k.count >> 8;
        switch (reg) {
            case 0: return "float4(0,0,0,0)";
            case 1: return F("pc[%d]", stage == 8 ? 16 : ((flags & 0x10) ? stage * 2 : 0));
            case 2: return F("pc[%d]", stage == 8 ? 17 : ((flags & 0x100) ? stage * 2 + 1 : 1));
            case 3: return "pFog";
            case 4: return "v0";
            case 5: return "v1";
            case 8: return "t0";
            case 9: return "t1";
            case 10: return "t2";
            case 11: return "t3";
            case 12: return "r0";
            case 13: return "r1";
            case 14:
                return F("%s(float4(%s.rgb + %s.rgb, 0))", clamp_sum ? "saturate" : "", inv_v1 ? "(1 - v1)" : "v1",
                         inv_r0 ? "(1 - r0)" : "r0");
            case 15: return "float4(" + e + " * " + f + ", 0)";
            default: return "float4(0,0,0,0)";
        }
    }

    std::string Input(In in, bool alpha) {
        std::string r = Var(in.reg);
        if (!alpha) r += in.chan ? ".aaa" : ".rgb";
        else r += in.chan ? ".a" : ".b";
        switch (in.mod) {
            case 0x00: return "max(" + r + ", 0)";
            case 0x20: return "(1 - saturate(" + r + "))";
            case 0x40: return "(2 * max(" + r + ", 0) - 1)";
            case 0x60: return "(-2 * max(" + r + ", 0) + 1)";
            case 0x80: return "(max(" + r + ", 0) - 0.5)";
            case 0xA0: return "(-max(" + r + ", 0) + 0.5)";
            case 0xC0: return r;
            default: return "-" + r;
        }
    }

    static std::string Map(const std::string& v, uint32_t m) {
        switch (m) {
            case 0x08: return "(" + v + " - 0.5)";
            case 0x10: return "(" + v + " * 2)";
            case 0x18: return "((" + v + " - 0.5) * 2)";
            case 0x20: return "(" + v + " * 4)";
            case 0x30: return "(" + v + " / 2)";
            default: return v;
        }
    }

    static const char* Dest(uint32_t reg) {
        switch (reg) {
            case 4: return "v0";
            case 5: return "v1";
            case 8: return "t0";
            case 9: return "t1";
            case 10: return "t2";
            case 11: return "t3";
            case 12: return "r0";
            case 13: return "r1";
            default: return nullptr;
        }
    }

    void Stage(uint32_t inputs, uint32_t output, bool alpha) {
        In d = ParseIn(inputs & 0xFF), c = ParseIn((inputs >> 8) & 0xFF), b = ParseIn((inputs >> 16) & 0xFF),
           a = ParseIn(inputs >> 24);
        uint32_t cd_dst = output & 0xF, ab_dst = (output >> 4) & 0xF, sum_dst = (output >> 8) & 0xF, fl = output >> 12;
        const char* m = alpha ? "a" : "rgb";
        const char* T = alpha ? "float" : "float3";
        std::string A = Input(a, alpha), B = Input(b, alpha), C = Input(c, alpha), D = Input(d, alpha);
        std::string ab = (fl & 2) ? "dot(" + A + ", " + B + ")" : "(" + A + " * " + B + ")";
        std::string cd = (fl & 1) ? "dot(" + C + ", " + D + ")" : "(" + C + " * " + D + ")";
        uint32_t map = fl & 0x38;
        std::string post;
        code += "{\n";
        code += F("%s ab = clamp((%s)%s, -1, 1);\n", T, T, Map(ab, map).c_str());
        code += F("%s cd = clamp((%s)%s, -1, 1);\n", T, T, Map(cd, map).c_str());
        std::string sum = (fl & 4) ? F("((%s) ? (%s)%s : (%s)%s)",
                                       ((k.count >> 8) & 1) ? "r0.a >= 0.5" : "(uint(r0.a * 255) & 1) == 1", T,
                                       cd.c_str(), T, ab.c_str())
                                   : "(" + ab + " + " + cd + ")";
        code += F("%s ms = clamp((%s)%s, -1, 1);\n", T, T, Map(sum, map).c_str());
        if (const char* r = Dest(ab_dst)) {
            post += F("%s.%s = ab;\n", r, m);
            if (!alpha && (fl & 0x80)) post += F("%s.a = ab.b;\n", r);
        }
        if (const char* r = Dest(cd_dst)) {
            post += F("%s.%s = cd;\n", r, m);
            if (!alpha && (fl & 0x40)) post += F("%s.a = cd.b;\n", r);
        }
        if (const char* r = Dest(sum_dst)) post += F("%s.%s = ms;\n", r, m);
        code += post + "}\n";
    }
};

// texture fetch for combiner texture modes
std::string TexFetch(const PixelKey& k, int i) {
    uint32_t mode = (k.tex_modes >> (i * 5)) & 0x1F;
    const TexInfo& t = k.tex[i];
    uint32_t in = i == 1 ? 0 : i == 2 ? (k.input_texture >> 16) & 0xF : (k.input_texture >> 20) & 0xF;
    uint32_t dm = i == 0 ? 0 : (k.dot_mapping >> ((i - 1) * 4)) & 0xF;
    const char* dotmap[] = {"dot01", "dotd3d", "dotgl", "dotm11", "dot01", "dot01", "dot01", "dot01"};
    std::string norm = t.rect ? F(" / texsize[%d].xy", i) : "";
    auto sample2 = [&](const std::string& uv) {
        if (t.kind == kTex2D) return F("tex%d.Sample(smp%d, (%s)%s)", i, i, uv.c_str(), norm.c_str());
        if (t.kind == kTexCube) return F("tex%d.Sample(smp%d, float3(%s, 1))", i, i, uv.c_str());
        if (t.kind == kTex3D) return F("tex%d.Sample(smp%d, float3(%s, 0))", i, i, uv.c_str());
        return std::string("float4(0,0,0,1)");
    };
    auto sample3 = [&](const std::string& uvw) {
        if (t.kind == kTex2D) return F("tex%d.Sample(smp%d, (%s).xy%s)", i, i, uvw.c_str(), norm.c_str());
        if (t.kind == kTex3D || t.kind == kTexCube) return F("tex%d.Sample(smp%d, %s)", i, i, uvw.c_str());
        return std::string("float4(0,0,0,1)");
    };
    std::string s;
    switch (mode) {
        case 0: return F("float4 t%d = float4(0,0,0,1);\n", i);
        case 1:  // project 2d
            if (t.shadow) return F("float4 t%d = shadow(tex%d.Sample(smp%d, (pT%d.xy / pT%d.w)%s).x, 0, %d);\n", i, i, i, i, i, norm.c_str(), t.shadow);
            return F("float4 t%d = %s;\n", i, sample2(F("pT%d.xy / pT%d.w", i, i)).c_str());
        case 2:  // project 3d
            if (t.shadow)
                return F("float4 t%d = shadow(tex%d.Sample(smp%d, (pT%d.xy / pT%d.w)%s).x, pT%d.z / pT%d.w, %d);\n", i, i, i, i, i,
                         norm.c_str(), i, i, t.shadow);
            return F("float4 t%d = %s;\n", i, sample3(F("pT%d.xyz / pT%d.w", i, i)).c_str());
        case 3: return F("float4 t%d = %s;\n", i, sample3(F("pT%d.xyz", i)).c_str());
        case 4: return F("float4 t%d = pT%d;\n", i, i);
        case 5:  // clip plane
            s = F("float4 t%d = 0;\n", i);
            for (int j = 0; j < 4; j++)
                s += F("if (pT%d.%c %s 0) discard;\n", i, "xyzw"[j], ((k.compare_mode >> (i * 4 + j)) & 1) ? ">=" : "<");
            return s;
        case 6:
        case 7:  // bump env map
            s = F("float2 dsdt%d = float2(sgn3(t%u.b), sgn3(t%u.g));\n", i, in, in);
            s += F("dsdt%d = float2(dot(bumpmat[%d].xz, dsdt%d), dot(bumpmat[%d].yw, dsdt%d));\n", i, i, i, i, i);
            s += F("float4 t%d = %s;\n", i, sample2(F("pT%d.xy + dsdt%d", i, i)).c_str());
            if (mode == 7) s += F("t%d *= bumplum[%d].x * t%u.r + bumplum[%d].y;\n", i, i, in, i);
            return s;
        case 9:  // dot st
            return F("float dot%d = dot(pT%d.xyz, %s(t%u));\nfloat4 t%d = %s;\n", i, i, dotmap[dm], in, i,
                     sample2(F("float2(dot%d, dot%d)", i - 1, i)).c_str());
        case 0xA:
        case 0x11:  // dot zw, dot product
            return F("float dot%d = dot(pT%d.xyz, %s(t%u));\nfloat4 t%d = 0;\n", i, i, dotmap[dm], in, i);
        case 0xB: {  // reflect diffuse, stage 2
            uint32_t dm3 = (k.dot_mapping >> 8) & 0xF, in3 = (k.input_texture >> 20) & 0xF;
            return F("float dot%d = dot(pT%d.xyz, %s(t%u));\nfloat dot%d_n = dot(pT%d.xyz, %s(t%u));\nfloat4 t%d = %s;\n", i, i,
                     dotmap[dm], in, i, i + 1, dotmap[dm3], in3, i, sample3(F("float3(dot%d, dot%d, dot%d_n)", i - 1, i, i)).c_str());
        }
        case 0xC:  // reflect specular, stage 3
            s = F("float dot%d = dot(pT%d.xyz, %s(t%u));\n", i, i, dotmap[dm], in);
            s += F("float3 n%d = float3(dot%d, dot%d, dot%d);\nfloat3 e%d = float3(pT%d.w, pT%d.w, pT%d.w);\n", i, i - 2, i - 1, i, i,
                   i - 2, i - 1, i);
            s += F("float3 rv%d = 2 * n%d * dot(n%d, e%d) / dot(n%d, n%d) - e%d;\n", i, i, i, i, i, i, i);
            return s + F("float4 t%d = %s;\n", i, sample3(F("rv%d", i)).c_str());
        case 0xD:
        case 0xE:  // dot str 3d, cube
            return F("float dot%d = dot(pT%d.xyz, %s(t%u));\nfloat4 t%d = %s;\n", i, i, dotmap[dm], in, i,
                     sample3(F("float3(dot%d, dot%d, dot%d)", i - 2, i - 1, i)).c_str());
        case 0xF: return F("float4 t%d = %s;\n", i, sample2(F("t%u.ar", in)).c_str());
        case 0x10: return F("float4 t%d = %s;\n", i, sample2(F("t%u.gb", in)).c_str());
        default: return F("float4 t%d = 0;\n", i);
    }
}

const char* kPsHelpers = R"(
float sgn3(float x) { x *= 255; return x >= 128 ? (x - 256) / 127 : x / 127; }
float sgn1(float x) { return (x * 255 - 128) / 127; }
float sgn2(float x) { x *= 255; return x >= 128 ? (x - 255.5) / 127.5 : (x + 0.5) / 127.5; }
float3 dot01(float4 c) { return c.rgb; }
float3 dotd3d(float4 c) { return float3(sgn1(c.r), sgn1(c.g), sgn1(c.b)); }
float3 dotgl(float4 c) { return float3(sgn2(c.r), sgn2(c.g), sgn2(c.b)); }
float3 dotm11(float4 c) { return float3(sgn3(c.r), sgn3(c.g), sgn3(c.b)); }
float4 shadow(float depth, float z, int fmt) {
    float mx = fmt == 2 ? 65535.0 : 16777215.0;
    float d = depth * mx;
    z = clamp(z, 0, mx);
    int f = (int)misc.z;
    bool r = f == 1 ? d < z : f == 2 ? d == z : f == 3 ? d <= z : f == 4 ? d > z : f == 5 ? d != z : f == 6 ? d >= z : f == 7;
    return r ? (float4)1 : (float4)0;
}
void alphatest(float a) {
    int f = (int)misc.y;
    int v = (int)round(a * 255), r = (int)misc.x;
    bool ok = f == 0 ? false : f == 1 ? v < r : f == 2 ? v == r : f == 3 ? v <= r : f == 4 ? v > r : f == 5 ? v != r : f == 6 ? v >= r : true;
    if (!ok) discard;
}
)";

// fixed function texture stage arguments, D3DTA
std::string FfArg(uint32_t a, int stage, bool alpha) {
    std::string v;
    switch (a & 0xF) {
        case 0: v = "v0"; break;
        case 1: v = "cur"; break;
        case 2: v = F("t%d", stage); break;
        case 3: v = "tfactor"; break;
        case 4: v = "v1"; break;
        case 5: v = "tmp"; break;
        default: v = "cur"; break;
    }
    if (a & 0x20) v = "(" + v + ").aaaa";
    if (a & 0x10) v = "(1 - " + v + ")";
    return "(" + v + ")" + (alpha ? ".a" : ".rgb");
}

std::string FfOp(uint32_t op, const std::string& a1, const std::string& a2, const std::string& a0, int stage, bool alpha) {
    switch (op) {
        case 2: return a1;
        case 3: return a2;
        case 4: return a1 + " * " + a2;
        case 5: return a1 + " * " + a2 + " * 2";
        case 6: return a1 + " * " + a2 + " * 4";
        case 7: return a1 + " + " + a2;
        case 8: return a1 + " + " + a2 + " - 0.5";
        case 9: return "(" + a1 + " + " + a2 + " - 0.5) * 2";
        case 10: return a1 + " - " + a2;
        case 11: return a1 + " + " + a2 + " * (1 - " + a1 + ")";
        case 12: return "lerp(" + a2 + ", " + a1 + ", v0.a)";
        case 13: return "lerp(" + a2 + ", " + a1 + ", cur.a)";
        case 14: return F("lerp(%s, %s, t%d.a)", a2.c_str(), a1.c_str(), stage);
        case 15: return "lerp(" + a2 + ", " + a1 + ", tfactor.a)";
        case 16: return F("%s + %s * (1 - t%d.a)", a1.c_str(), a2.c_str(), stage);
        case 18: return alpha ? a1 : a1 + " + " + a2 + " * " + a1.substr(0, a1.size() - 4) + ".a";
        case 19: return alpha ? a1 : a1 + " * " + a2 + " + " + a1.substr(0, a1.size() - 4) + ".a";
        case 20: return alpha ? a1 : a1 + " + " + a2 + " * (1 - " + a1.substr(0, a1.size() - 4) + ".a)";
        case 21: return alpha ? a1 : "(1 - " + a1 + ") * " + a2 + " + " + a1.substr(0, a1.size() - 4) + ".a";
        case 22: return F("saturate(dot((%s - 0.5) * 2, (%s - 0.5) * 2))", alpha ? ("float3(" + a1 + ",0,0)").c_str() : a1.c_str(),
                          alpha ? ("float3(" + a2 + ",0,0)").c_str() : a2.c_str());
        case 23: return a0 + " + " + a1 + " * " + a2;
        case 24: return "lerp(" + a2 + ", " + a1 + ", " + a0 + ")";
        default: return alpha ? "cur.a" : "cur.rgb";
    }
}

}  // namespace

bool PixelKey::operator==(const PixelKey& o) const { return std::memcmp(this, &o, sizeof(*this)) == 0; }

uint64_t Hash(const void* p, size_t n, uint64_t h) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

PixelKey MakePixelKey(const State& s, const TexInfo tex[4]) {
    PixelKey k;
    std::memset(&k, 0, sizeof(k));
    for (int i = 0; i < 4; i++) {
        k.tex[i] = tex[i];
        if (s.tss[size_t(i)][kTssAlphaKill] == 4) k.alphakill |= 1u << i;
    }
    k.flat = s.simple[kMShadeMode >> 2] == 0x1D00;
    k.two_sided = s.rs[kRsTwoSidedLighting] != 0;
    if (s.ps) {
        const uint32_t* d = s.ps->def;
        k.programmable = 1;
        k.count = d[53];
        for (int i = 0; i < 8; i++) {
            k.a_in[i] = d[i];
            k.a_out[i] = d[26 + i];
            k.rgb_in[i] = d[34 + i];
            k.rgb_out[i] = d[45 + i];
        }
        k.final_abcd = d[8];
        k.final_efg = d[9];
        // the stage modes follow the bound textures, as D3D's lazy shader stage setup does
        uint32_t modes = s.rs[kRsPsTextureModes];
        for (int i = 0; i < 4; i++) {
            uint32_t m = (modes >> (i * 5)) & 0x1F;
            const TexInfo& t = tex[i];
            if (!t.kind && m != 4 && m != 5 && m != 0xA && m != 0x11) m = 0;
            else if (m >= 1 && m <= 3) m = t.kind == kTexCube ? 3 : (t.kind == kTex3D || t.shadow) ? 2 : 1;
            else if (m == 0xD || m == 0xE) m = t.kind == kTexCube ? 0xE : 0xD;
            k.tex_modes |= m << (i * 5);
        }
        k.dot_mapping = d[55];
        k.input_texture = d[56];
        k.compare_mode = d[42];
        return k;
    }
    for (int i = 0; i < 4; i++) {
        const auto& t = s.tss[size_t(i)];
        uint32_t v[9] = {t[kTssColorOp], t[kTssColorArg1], t[kTssColorArg2], t[kTssColorArg0], t[kTssAlphaOp],
                         t[kTssAlphaArg1],  t[kTssAlphaArg2], t[kTssAlphaArg0], t[kTssResultArg]};
        std::memcpy(k.ff[i], v, sizeof(v));
        if (t[kTssColorOp] <= 1) break;
    }
    return k;
}

std::string VertexProgramHlsl(const std::vector<uint32_t>& prog, uint16_t regs) {
    std::string s = kCommon;
    s += kVsHelpers;
    s += VsInputStruct(regs);
    s += "VOut main(VIn i) {\n";
    s += kVsPrologue;
    for (size_t n = 0; n + 4 <= prog.size(); n += 4) {
        Tok k{&prog[n]};
        uint32_t mac = k.f(1, 21, 4), ilu = k.f(1, 25, 3);
        if (mac || ilu) {
            std::string a = Src(k, 0, regs), b = Src(k, 1, regs), c = Src(k, 2, regs);
            s += "    {\n";
            if (mac) s += "        float4 mr = " + MacExpr(mac, a, b, c) + ";\n";
            if (ilu) s += "        float4 ir = " + IluExpr(ilu, c) + ";\n";
            uint32_t r = k.f(3, 20, 4), omask = k.f(3, 12, 4);
            if (mac == 13) {
                s += "        a0 = (int)floor(mr.x + 0.001);\n";
            } else if (mac) {
                uint32_t m = k.f(3, 24, 4);
                if (ilu && r == 1) m = 0;
                if (m && r <= 12) s += F("        %s.%s = mr.%s;\n", RegDest(r).c_str(), kMask[m], kMask[m]);
            }
            if (ilu) {
                uint32_t m = k.f(3, 16, 4), ir = mac ? 1 : r;
                if (m && ir <= 12) s += F("        %s.%s = ir.%s;\n", RegDest(ir).c_str(), kMask[m], kMask[m]);
            }
            bool use_ilu = k.f(3, 2, 1) == 1;
            if (omask && k.f(3, 11, 1) && ((use_ilu && ilu) || (!use_ilu && mac && mac != 13))) {
                uint32_t o = k.f(3, 3, 8) & 0xF;
                const char* src = use_ilu ? "ir" : "mr";
                if (kOut[o][0]) {
                    const char* msk = o == 5 ? kFogMask[omask] : kMask[omask];
                    s += F("        %s.%s = %s.%s;\n", kOut[o], msk, src, msk);
                }
            }
            s += "    }\n";
        }
        if (k.f(3, 0, 1)) break;
    }
    s += kVsEpilogue;
    return s;
}

std::string FixedVertexHlsl(uint16_t regs, bool pretransformed) {
    std::string s = kCommon;
    s += kVsHelpers;
    s += VsInputStruct(regs);
    s += "VOut main(VIn i) {\n";
    s += kVsPrologue;
    auto v = [&](int r, const char* def) { return (regs >> r) & 1 ? F("i.v%d", r) : std::string(def); };
    if (pretransformed) {
        s += "    float4 q = " + v(0, "float4(0,0,0,1)") + ";\n";
        s += "    float w = q.w == 0 ? 1 : 1 / q.w;\n";
        s += "    oPos = float4(q.xy, q.z / surf.z, w);\n";
    } else {
        s += "    float4 cp = mul(float4(" + v(0, "float4(0,0,0,1)") + ".xyz, 1), wvp);\n";
    }
    s += "    oD0 = " + v(3, "float4(1,1,1,1)") + ";\n";
    s += "    oD1 = " + v(4, "float4(0,0,0,0)") + ";\n";
    s += "    oB0 = oD0;\n    oB1 = oD1;\n";
    for (int t = 0; t < 4; t++) s += F("    oT%d = %s;\n", t, v(9 + t, "float4(0,0,0,1)").c_str());
    s += "    oFog = " + v(5, "float4(0,0,0,0)") + ";\n";
    if (pretransformed) return s + kVsEpilogue;
    s += R"(
    VOut o;
    o.pos = cp;
    o.d0 = saturate(oD0);
    o.d1 = saturate(oD1);
    o.b0 = o.d0;
    o.b1 = o.d1;
    o.t0 = oT0;
    o.t1 = oT1;
    o.t2 = oT2;
    o.t3 = oT3;
    o.fog = fogfactor(abs(cp.z));
    return o;
}
)";
    return s;
}

std::string PixelHlsl(const PixelKey& k) {
    std::string s = kCommon;
    s += kPsHelpers;
    for (int i = 0; i < 4; i++) {
        const TexInfo& t = k.tex[i];
        const char* type = t.shadow ? "Texture2D<float4>" : t.kind == kTex3D ? "Texture3D" : t.kind == kTexCube ? "TextureCube" : "Texture2D";
        s += F("%s tex%d : register(t%d);\nSamplerState smp%d : register(s%d);\n", type, i, i, i, i);
    }
    s += F("float4 main(VOut i, bool front : SV_IsFrontFace) : SV_Target {\n");
    s += "    float4 pT0 = i.t0, pT1 = i.t1, pT2 = i.t2, pT3 = i.t3;\n";
    s += k.two_sided ? "    float4 v0 = front ? i.d0 : i.b0, v1 = front ? i.d1 : i.b1;\n" : "    float4 v0 = i.d0, v1 = i.d1;\n";
    s += "    if (misc.w == 0) v1 = 0;\n";
    s += "    float4 pFog = float4(fogcolor.rgb, saturate(i.fog));\n";
    std::string body;
    if (k.programmable) {
        for (int i = 0; i < 4; i++) body += TexFetch(k, i);
        for (int i = 0; i < 4; i++)
            if ((k.alphakill >> i) & 1) body += F("if (t%d.a == 0) discard;\n", i);
        uint32_t mode0 = k.tex_modes & 0x1F;
        body += F("float4 r0 = float4(0, 0, 0, %s), r1 = 0;\n", mode0 ? "t0.a" : "1");
        Ps ps{k};
        uint32_t n = std::min<uint32_t>(k.count & 0xFF, 8);
        for (uint32_t st = 0; st < n; st++) {
            ps.stage = int(st);
            ps.Stage(k.rgb_in[st], k.rgb_out[st], false);
            ps.Stage(k.a_in[st], k.a_out[st], true);
        }
        body += ps.code;
        if (k.final_abcd || k.final_efg) {
            ps.stage = 8;
            uint32_t fl = k.final_efg & 0xFF;
            ps.clamp_sum = fl & 0x80;
            ps.inv_v1 = fl & 0x40;
            ps.inv_r0 = fl & 0x20;
            ps.e = ps.Input(ParseIn(k.final_efg >> 24), false);
            ps.f = ps.Input(ParseIn((k.final_efg >> 16) & 0xFF), false);
            std::string A = ps.Input(ParseIn(k.final_abcd >> 24), false), B = ps.Input(ParseIn((k.final_abcd >> 16) & 0xFF), false),
                        C = ps.Input(ParseIn((k.final_abcd >> 8) & 0xFF), false), D = ps.Input(ParseIn(k.final_abcd & 0xFF), false),
                        G = ps.Input(ParseIn((k.final_efg >> 8) & 0xFF), true);
            body += "float4 outc;\n";
            body += "outc.rgb = " + D + " + lerp(" + C + ", " + B + ", " + A + ");\n";
            body += "outc.a = " + G + ";\n";
        } else {
            body += "float4 outc = float4(lerp(pFog.rgb, r0.rgb, pFog.a), r0.a);\n";
        }
    } else {
        body += "float4 cur = v0, tmp = 0;\n";
        for (int i = 0; i < 4; i++) {
            const uint32_t* f = k.ff[i];
            if (f[0] <= 1) break;
            const TexInfo& t = k.tex[i];
            std::string uv = F("pT%d.xy%s", i, t.rect ? F(" / texsize[%d].xy", i).c_str() : "");
            if (t.kind == kTex2D) body += F("float4 t%d = tex%d.Sample(smp%d, %s);\n", i, i, i, uv.c_str());
            else if (t.kind) body += F("float4 t%d = tex%d.Sample(smp%d, pT%d.xyz);\n", i, i, i, i);
            else body += F("float4 t%d = float4(1,1,1,1);\n", i);
            if ((k.alphakill >> i) & 1) body += F("if (t%d.a == 0) discard;\n", i);
            const char* dst = f[8] == 5 ? "tmp" : "cur";
            std::string rgb = FfOp(f[0], FfArg(f[1], i, false), FfArg(f[2], i, false), FfArg(f[3], i, false), i, false);
            std::string a = f[4] <= 1 ? std::string("cur.a") : FfOp(f[4], FfArg(f[5], i, true), FfArg(f[6], i, true), FfArg(f[7], i, true), i, true);
            body += F("{ float3 c3 = saturate(%s); float c1 = saturate(%s); %s = float4(c3, c1); }\n", rgb.c_str(), a.c_str(), dst);
        }
        body += "cur.rgb = saturate(cur.rgb + v1.rgb);\n";
        body += "float4 outc = float4(lerp(pFog.rgb, cur.rgb, pFog.a), cur.a);\n";
    }
    body += "alphatest(outc.a);\nreturn outc;\n}\n";
    s += body;
    return s;
}

}  // namespace gpu
