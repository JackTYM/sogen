/*
 * Simulator-only replacement for deps/unicorn/qemu/include/tcg/tcg-apple-jit.h.
 *
 * This file lives OUTSIDE deps/unicorn (a shallow clone of the upstream, non-fork
 * momo5502/unicorn repo — see .gitmodules — so it cannot durably carry local source edits) and is
 * force-included (via the compiler's `-include` flag, see cmake/unicorn-ios-simulator-jit.cmake)
 * only when building deps/unicorn for the iOS Simulator target. Force-including it defines the
 * TCG_APPLE_JIT_H include guard before the real header is ever reached, so the real file's
 * `#ifndef TCG_APPLE_JIT_H` sees the guard already set and its own body never runs — this file's
 * declarations are used by the rest of Unicorn's Apple-JIT code (uc.c, translate-all.c,
 * misc_helper.c, ioport.c) instead, without ever touching or duplicating the submodule itself.
 *
 * Two real problems on the iOS Simulator target motivate this, neither of which the real header's
 * upstream logic anticipates (it only distinguishes "real macOS" from "no Apple JIT support"):
 *
 * 1. The iOS SDK marks pthread_jit_write_protect_np() `__API_UNAVAILABLE(ios, ...)` even though
 *    the symbol is present and functional on the Simulator (Simulator processes are ordinary
 *    macOS host processes) — so qemu/configure's compile-time probe for HAVE_PTHREAD_JIT_PROTECT
 *    always fails there, and even a real HAVE_PTHREAD_JIT_PROTECT define (see the CMake side of
 *    this fix) wouldn't let the real header's `pthread_jit_write_protect_np(enabled)` call compile
 *    directly. Resolved via dlsym(), which never names the symbol in a way the compiler's
 *    availability check can see.
 *
 * 2. accel/tcg/translate-all.c's tb_exec_change() (unmodified upstream) skips the very first
 *    unlock/lock toggle because its tracked uc->current_executable starts false
 *    (zero-initialized), which happens to already match a freshly mmap'd MAP_JIT buffer's true
 *    state on native macOS (writable by default there) — but not on the iOS Simulator, where the
 *    buffer does not start writable, so the skipped toggle leaves it execute-protected and the
 *    first TCG prologue write faults (confirmed via lldb: EXC_BAD_ACCESS writing into the
 *    MAP_JIT-allocated buffer during tcg_prologue_init). assert_executable() is the one hook
 *    tb_exec_change() calls unconditionally — even on the path that skips the toggle itself — so
 *    it is used here to force the real one-time unlock before that first write, without touching
 *    translate-all.c.
 */

#ifndef TCG_APPLE_JIT_H
#define TCG_APPLE_JIT_H

#include "assert.h"
#include "stdint.h"
#include "stdbool.h"
#include "qemu/compiler.h"

#include <dlfcn.h>

static inline void sogen_ios_sim_pthread_jit_write_protect_np(int enabled)
{
    typedef void (*jit_write_protect_fn)(int);
    static jit_write_protect_fn fn;
    static bool resolved;
    if (!resolved) {
        fn = (jit_write_protect_fn)dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_np");
        resolved = true;
    }
    if (fn) {
        fn(enabled);
    }
}

QEMU_UNUSED_FUNC static inline uint8_t thread_mask()
{
    return 0;
}

QEMU_UNUSED_FUNC static inline bool thread_writeable()
{
    return false;
}

QEMU_UNUSED_FUNC static inline bool thread_executable()
{
    return false;
}

static inline void assert_executable(bool executable)
{
    static bool unlocked_once;
    if (!unlocked_once) {
        unlocked_once = true;
        sogen_ios_sim_pthread_jit_write_protect_np(0);
    }
    (void)executable;
}

static inline void jit_write_protect(int enabled)
{
    sogen_ios_sim_pthread_jit_write_protect_np(enabled);
}

#define JIT_CALLBACK_GUARD(x)                       \
{                                                   \
    bool executable = uc->current_executable;       \
    assert_executable(executable);                  \
    x;                                              \
    jit_write_protect(executable);                  \
}                                                   \


#define JIT_CALLBACK_GUARD_VAR(var, x)                  \
{                                                       \
    bool executable = uc->current_executable;           \
    assert_executable(executable);                      \
    var = x;                                            \
    jit_write_protect(executable);                      \
}                                                       \

#endif /* define TCG_APPLE_JIT_H */
