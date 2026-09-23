/*
 * Real-device-only replacement for mmap() as seen by Unicorn's own C sources, force-included
 * (via -include, see cmake/unicorn-ios-device-jit.cmake) only when building deps/unicorn for the
 * iOS DEVICE target -- mirrors cmake/unicorn-ios-simulator-jit-shim.h's approach for the
 * Simulator (force-included header, compile-time macro substitution), which solves a different
 * problem; see that file's own comment.
 *
 * On real iOS hardware, TXM/SPTM blocks any writable->executable memory transition unless the
 * specific pages have been blessed by an attached debugger first (see
 * tools/sogen-ios/Sources/JIT/JIT26.c / JITGate.swift). An earlier version of this fix blessed
 * Unicorn's own, already-mmap'd TCG buffer AFTER THE FACT
 * (jit26_prepare_region(existing_address, length)). Live device testing conclusively disproved
 * that: jit26_prepare_region() and a follow-up mprotect() both reported success, and even
 * directly jumping into the "blessed" memory from this app's own code (completely independent of
 * Unicorn) hung forever -- proving blessing an already-allocated region does not actually grant
 * real execute capability, regardless of what those calls report. The only path proven to
 * actually work is this app's own startup self-test's path: jit26_prepare_region(nil, size),
 * letting the debugger allocate AND bless a brand new region itself, in one step.
 *
 * This header makes Unicorn's own TCG-buffer-allocating mmap() call go through that exact proven
 * path instead, by substituting the call itself at the preprocessor level -- deliberately NOT
 * runtime symbol interposition (DYLD_INTERPOSE was tried first and confirmed to never engage on
 * this binary at all), since text substitution at compile time is immune to whatever dyld policy
 * blocked that.
 *
 * mprotect() is intercepted too, for the same reason: tcg_region_init() (qemu/tcg/tcg.c) calls
 * qemu_mprotect_none() on a guard page carved off the end of the buffer this shim just handed
 * back, an unmodified real mprotect() syscall running immediately after blessing, completely
 * outside the mmap shim's awareness on its own. Live device testing with a 64MB buffer (blessed
 * in 0 seconds, ruling out "too slow") still hung in the exact same shape as the original 2GB
 * case, pointing squarely at whatever Unicorn does to that memory next -- this guard-page
 * mprotect() being the very next thing in its own init sequence. The real implementation
 * (sogen_ios_device_jit_mprotect(), in the same .cpp as the mmap one) treats any mprotect() call
 * whose range falls inside a region this shim already blessed as a no-op, since real device
 * testing is the only way to know whether XNU/TXM treats a further mprotect() on already-blessed
 * memory specially.
 *
 * Both sogen_ios_device_jit_mmap() and sogen_ios_device_jit_mprotect() (the real implementations)
 * live in src/common/utils/ios_device_jit_mmap_shim.cpp -- deliberately NOT anywhere this header's
 * force-include reaches, since their own fallback calls to the real mmap()/mprotect() would
 * otherwise recurse into themselves through these exact macros.
 */
#ifndef SOGEN_IOS_DEVICE_JIT_SHIM_H
#define SOGEN_IOS_DEVICE_JIT_SHIM_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void* sogen_ios_device_jit_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int sogen_ios_device_jit_mprotect(void* addr, size_t length, int prot);

#ifdef __cplusplus
}
#endif

#define mmap sogen_ios_device_jit_mmap
#define mprotect sogen_ios_device_jit_mprotect

#endif /* SOGEN_IOS_DEVICE_JIT_SHIM_H */
