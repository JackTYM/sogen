// Implements sogen_ios_device_jit_mmap(), declared in cmake/unicorn-ios-device-jit-shim.h and
// force-included (via -include, see cmake/unicorn-ios-device-jit.cmake) as a #define replacing
// every call to mmap() in Unicorn's own real-device-target compilation. See that header's own
// comment for the full story of why this exists and what it replaced.
//
// Deliberately NOT compiled with that same force-include: this file's own fallback call to the
// real mmap() below would otherwise recurse into itself through the very macro it implements.
// This lives in emulator-common rather than alongside Unicorn's sources specifically so it never
// receives that force-include, which is scoped (in unicorn-ios-device-jit.cmake) to the unicorn /
// unicorn-common / x86_64-softmmu targets only.
//
// Live device testing conclusively proved that JIT26 blessing is tied to the *exact virtual
// mapping* jit26_prepare_region() itself returns -- not the underlying physical pages, and not
// any other mapping of them (a writable alias of an already-blessed region, even re-blessed via
// jit26_prepare_region() a second time with the alias itself as the hint, is never actually
// executable, despite mprotect() reporting clean success both times). So Unicorn cannot use a
// single pointer for both writing generated code and executing it, the way it normally does --
// it needs the QEMU upstream "splitwx" (split write/execute) architecture, adapted for our case:
// the RX side is exactly the jit26_prepare_region(nil, ...) pointer (proven executable); the RW
// side is a jit26_writable_alias() of it (proven writable). deps/unicorn is patched (see that
// fork's own commits) to write generated code through the RW side while keeping every address
// that becomes a jump target or gets called from C code in RX-space, converting between the two
// via a runtime-computed offset (sogen_tcg_splitwx_diff, set below) wherever needed. This file
// only needs to perform the allocation and hand back the RW (alias) pointer as "mmap's result" --
// exactly what Unicorn already expects to receive and write generated code through.

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR

#include <array>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>

// Defined in tools/sogen-ios/Sources/JIT/JIT26.c (jit26_prepare_region, jit26_writable_alias) and
// tools/sogen-ios/Sources/JIT/DeviceJITLog.swift (sogen_jit26_device_log), both part of the
// SogenIOS app target rather than this CMake target; tied together only at final link time inside
// the app's own Mach-O image, the same way src/backends/unicorn-emulator/ios_device_jit_bless.cpp
// (now removed -- see git history) already relied on for the same two symbols.
extern "C" void* jit26_prepare_region(void* address, size_t length);
extern "C" void* jit26_writable_alias(void* rx_address, size_t length, int* out_kern_return, unsigned int* out_cur_prot,
                                       unsigned int* out_max_prot);
extern "C" void sogen_jit26_device_log(const char* line);

// Defined in deps/unicorn/qemu/tcg/tcg.c (this fork's own splitwx patch), 0 by default. Setting
// this is the ONLY thing this file needs to do to make deps/unicorn's dual-pointer code work --
// see that fork's own tcg.h for tcg_splitwx_to_rx()/_to_rw(), which use this offset.
extern "C" intptr_t sogen_tcg_splitwx_diff;

namespace
{
    // Every RW (alias) region sogen_ios_device_jit_mmap() has handed to Unicorn, so
    // sogen_ios_device_jit_mprotect() can recognize a later mprotect() call landing inside one of
    // them (see that function's own comment for why that matters). This whole init sequence runs
    // on a single thread with no concurrent allocation happening at the same time, so a plain
    // fixed-size array with no locking is enough -- realistically there is only ever one such
    // region (one Unicorn engine, one TCG buffer) in this app.
    struct blessed_region
    {
        std::uintptr_t start{};
        std::uintptr_t end{};
    };

    std::array<blessed_region, 8> g_blessed_regions{};
    size_t g_blessed_region_count = 0;

    void record_blessed_region(void* address, const size_t length)
    {
        if (g_blessed_region_count >= g_blessed_regions.size())
        {
            return;
        }
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        g_blessed_regions[g_blessed_region_count++] = {start, start + length};
    }

    bool range_within_a_blessed_region(void* address, const size_t length)
    {
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        const auto end = start + length;
        for (size_t i = 0; i < g_blessed_region_count; ++i)
        {
            if (start >= g_blessed_regions[i].start && end <= g_blessed_regions[i].end)
            {
                return true;
            }
        }
        return false;
    }

    // Ongoing sanity check, not a one-off diagnostic: proves the RX (original) pointer is
    // genuinely executable, via the one pattern proven to actually work on real TXM/SPTM hardware
    // (write through the alias, invalidate icache at the ORIGINAL address, execute the ORIGINAL
    // address -- exactly JITGate.swift's own self-test pattern). Cheap (a handful of instructions)
    // and fails fast with a clear log line if this mechanism ever stops working, rather than
    // silently handing Unicorn a buffer whose RX side turns out not to work this time.
    void self_test_confirm_original_executable(void* alias, void* region, const size_t length)
    {
        auto* const write_addr = reinterpret_cast<uint8_t*>(alias) + (length / 2);
        auto* const exec_addr = reinterpret_cast<uint8_t*>(region) + (length / 2);

        *reinterpret_cast<volatile uint32_t*>(write_addr) = 0xD65F03C0u; // AArch64 RET
        sys_icache_invalidate(exec_addr, sizeof(uint32_t));

        using ret_fn = void (*)();
        reinterpret_cast<ret_fn>(static_cast<void*>(exec_addr))();

        sogen_jit26_device_log("[jit26-device] self-test: RX pointer confirmed executable");
    }
} // namespace

extern "C" void* sogen_ios_device_jit_mmap(void* addr, const size_t length, const int prot, const int flags,
                                            const int fd, const off_t offset)
{
    // Matches Unicorn's real, single call site (deps/unicorn/qemu/accel/tcg/translate-all.c's
    // alloc_code_gen_buffer(), the non-USE_MAP_JIT/non-USE_STATIC_CODE_GEN_BUFFER branch, the one
    // actually compiled for this target) exactly, not a bitwise subset test, to avoid catching
    // some unrelated mmap() call elsewhere in Unicorn's own sources (guest RAM, scratch buffers,
    // ...) that this same macro substitution also reaches.
    const bool is_unicorn_tcg_buffer = addr == nullptr && fd == -1 && offset == 0 &&
                                        prot == (PROT_READ | PROT_WRITE | PROT_EXEC) &&
                                        flags == (MAP_PRIVATE | MAP_ANONYMOUS);

    if (!is_unicorn_tcg_buffer)
    {
        return mmap(addr, length, prot, flags, fd, offset);
    }

    char line[192];
    std::snprintf(line, sizeof(line),
                  "[jit26-device] Unicorn's TCG buffer mmap() intercepted (size=%zu) -- asking JIT26 to "
                  "allocate+bless it directly instead of mmap'ing it ourselves",
                  length);
    sogen_jit26_device_log(line);

    // nil hint: the ONLY path live device testing has proven actually grants real execute
    // capability -- the debugger allocates a brand new region itself and blesses the whole thing
    // as part of creating it. This becomes the RX side of the splitwx pair; Unicorn never sees
    // this pointer directly (deps/unicorn's own patched code converts to it via
    // tcg_splitwx_to_rx() only at the specific points that need a real executable address).
    //
    // Timed explicitly: this call blesses the region one 16KB page at a time over a live
    // GDB-remote session (see universal.js's prepare_memory_region), so for a large request it can
    // legitimately take a while -- wall-clock seconds here is the only way to tell "slow but
    // finite" from "genuinely hung" from an exported log alone.
    const std::time_t started = std::time(nullptr);
    void* const region = jit26_prepare_region(nullptr, length);
    const double elapsed_seconds = std::difftime(std::time(nullptr), started);

    std::snprintf(line, sizeof(line), "[jit26-device] jit26_prepare_region(nil, %zu) -> %p (%.0fs)", length, region,
                  elapsed_seconds);
    sogen_jit26_device_log(line);

    if (!region)
    {
        // Unicorn's own code_gen_alloc() treats a NULL return from alloc_code_gen_buffer() as
        // fatal and calls exit(1) immediately -- there is no way to turn that into a clean
        // in-app error from here, short of patching deps/unicorn's own error handling too. This
        // log line is the only diagnostic that will exist for that failure.
        sogen_jit26_device_log(
            "[jit26-device] jit26_prepare_region returned null -- Unicorn is about to exit(1) as a result");
        return MAP_FAILED;
    }

    // The RW side of the splitwx pair: a writable remap of the same physical pages, exactly
    // JITGate.swift's own self-test mechanism. jit26_writable_alias() already leaves this
    // READ+WRITE on its own (it mprotects internally if needed) -- no separate execute permission
    // is ever needed on this pointer, since Unicorn now only ever writes through it, never
    // executes it (that's the entire point of the fix: execution always goes through the RX side,
    // converted via deps/unicorn's own tcg_splitwx_to_rx(), never this pointer directly).
    int kern_return = 0;
    unsigned int cur_prot = 0;
    unsigned int max_prot = 0;
    void* const alias = jit26_writable_alias(region, length, &kern_return, &cur_prot, &max_prot);

    std::snprintf(line, sizeof(line), "[jit26-device] jit26_writable_alias(%p) -> %p (kr=%d cur=0x%x max=0x%x)",
                  region, alias, kern_return, cur_prot, max_prot);
    sogen_jit26_device_log(line);

    if (!alias)
    {
        sogen_jit26_device_log(
            "[jit26-device] jit26_writable_alias failed -- returning the read+execute-only region as-is "
            "(Unicorn's first write into it will fault)");
        record_blessed_region(region, length);
        return region;
    }

    self_test_confirm_original_executable(alias, region, length);

    // deps/unicorn's own splitwx patch (see that fork's tcg.h/tcg.c/translate-all.c/
    // tcg-target.inc.c) reads this to convert specific addresses (the prologue entry point, each
    // translation block's dispatch address, direct-jump patch sites) from the RW pointer it
    // normally works with to the real RX pointer, only where a genuinely executable address is
    // actually needed.
    sogen_tcg_splitwx_diff = reinterpret_cast<intptr_t>(region) - reinterpret_cast<intptr_t>(alias);
    std::snprintf(line, sizeof(line), "[jit26-device] sogen_tcg_splitwx_diff = %ld (rx=%p, rw/alias=%p)",
                  static_cast<long>(sogen_tcg_splitwx_diff), region, alias);
    sogen_jit26_device_log(line);

    std::snprintf(line, sizeof(line), "[jit26-device] handing Unicorn %p (the writable alias) as its TCG buffer",
                  alias);
    sogen_jit26_device_log(line);

    record_blessed_region(alias, length);
    return alias;
}

// tcg_region_init() (qemu/tcg/tcg.c) calls qemu_mprotect_none() on a guard page carved off the
// end of the buffer sogen_ios_device_jit_mmap() above just returned -- an unmodified real
// mprotect() call running immediately after blessing (osdep.c's qemu_mprotect__osdep(), compiled
// as part of the unicorn-common target, so it goes through this exact force-included macro too).
// A plain mprotect() on part of an already-JIT26-blessed region hung indefinitely in earlier
// testing (before the splitwx fix existed) -- consistent with XNU/TXM treating a further
// mprotect() on already-blessed memory specially. This makes any mprotect() call whose full range
// falls inside an already-blessed region a no-op (pretends success without touching it), since it
// has already been through JIT26's real blessing process. This does forgo the guard page's actual
// protection (a safety net against Unicorn's own TCG codegen overrunning the buffer, not something
// guest correctness depends on) for the sake of avoiding that hang.
extern "C" int sogen_ios_device_jit_mprotect(void* addr, const size_t length, const int prot)
{
    if (range_within_a_blessed_region(addr, length))
    {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "[jit26-device] mprotect(%p, %zu, 0x%x) intercepted -- inside a JIT26-blessed region, "
                      "skipping (no-op, reporting success)",
                      addr, length, static_cast<unsigned int>(prot));
        sogen_jit26_device_log(line);
        return 0;
    }

    return mprotect(addr, length, prot);
}

#else

#include <sys/mman.h>

extern "C" void* sogen_ios_device_jit_mmap(void* addr, const size_t length, const int prot, const int flags,
                                            const int fd, const off_t offset)
{
    return mmap(addr, length, prot, flags, fd, offset);
}

extern "C" int sogen_ios_device_jit_mprotect(void* addr, const size_t length, const int prot)
{
    return mprotect(addr, length, prot);
}

#endif
