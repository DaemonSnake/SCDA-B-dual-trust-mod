// BinkGetSummary divides bytes read * 1000 by the read time in ms + 1. Host reads take under a millisecond,
// the time stays 0 and the quotient of a large movie overflows the 32 bit div. Raise the time just enough.
#include "xdk/hle.h"

using namespace kernel;

X86_EXTERN_FUNC(__imp__sub_004BABE0);
X86_FUNC(sub_004BABE0) {
    uint32_t bink = R32(ctx.esp + 4);
    if (bink) {
        uint64_t scaled = uint64_t(R32(bink + 0x134)) * 1000;
        uint32_t time = R32(bink + 0x13C);
        if (scaled >> 32 > time) W32(bink + 0x13C, uint32_t(scaled >> 32));
    }
    __imp__sub_004BABE0(ctx, base);
}
