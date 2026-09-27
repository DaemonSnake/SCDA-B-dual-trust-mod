#pragma once
#include "kernel/kernel.h"

// Host implementation of a guest function. addr is the 8 digit guest address the recompiler named it by,
// the strong definition replaces the recompiled weak one.
#define HLE_STDCALL(addr, name, nargs)                                                   \
    static uint32_t hle_##name(X86Context& ctx, const ::kernel::Args& a);                \
    X86_FUNC(sub_##addr) {                                                               \
        (void)base;                                                                      \
        ::kernel::Args args{ctx};                                                        \
        uint32_t r = hle_##name(ctx, args);                                              \
        ::kernel::Return(ctx, r, nargs);                                                 \
    }                                                                                    \
    static uint32_t hle_##name([[maybe_unused]] X86Context& ctx, [[maybe_unused]] const ::kernel::Args& a)

// __fastcall: first two arguments in ecx and edx, the rest on the stack
#define HLE_FASTCALL(addr, name, stack_args)                                             \
    static uint32_t hle_##name(X86Context& ctx, const ::kernel::Args& a);                \
    X86_FUNC(sub_##addr) {                                                               \
        (void)base;                                                                      \
        ::kernel::Args args{ctx};                                                        \
        uint32_t r = hle_##name(ctx, args);                                              \
        ::kernel::Return(ctx, r, stack_args);                                            \
    }                                                                                    \
    static uint32_t hle_##name([[maybe_unused]] X86Context& ctx, [[maybe_unused]] const ::kernel::Args& a)

// __cdecl: the caller pops the arguments
#define HLE_CDECL(addr, name) HLE_STDCALL(addr, name, 0)

// float arguments passed on the stack
inline float ArgF(const ::kernel::Args& a, int i) {
    uint32_t v = a[i];
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
inline float GF(uint32_t va) {
    float f;
    std::memcpy(&f, ::kernel::HostPtr(va), 4);
    return f;
}
