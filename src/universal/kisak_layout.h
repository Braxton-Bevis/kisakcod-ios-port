// The decompilation hard-asserts x86-32 struct layouts (sizeof checks against
// the original .exe). On arm64 any pointer-bearing struct grows, so on iOS the
// asserts that fire are individually relaxed through this macro — it keeps a
// grep-able paper trail of every layout the port has knowingly diverged from.
// Serialized structs (fastfile/network) still need real layout work later;
// relaxing is a probe tool, not a fix.
#pragma once

#if defined(KISAK_IOS)
#define KISAK_LAYOUT_ASSERT(...) static_assert(true, "x86-32 layout assert relaxed for iOS probe")
#else
#define KISAK_LAYOUT_ASSERT(...) static_assert(__VA_ARGS__)
#endif
