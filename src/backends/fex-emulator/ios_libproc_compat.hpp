#pragma once

// <libproc.h> does not exist at all in the iOS/iOS-Simulator SDKs (unlike <mach/mach_vm.h>, which
// is present but #errors out - this one is simply absent). proc_regionfilename is documented
// (macOS's libproc.h) as available since __IPHONE_2_0 and is a real, linkable symbol in the
// iOS/iOS-Simulator SDKs' libSystem.tbd export list; only the header is withheld. Declare just
// that one routine, and the PROC_PIDPATHINFO_MAXSIZE constant it's used with (macOS's
// sys/proc_info.h), ourselves.
#include <cstdint>
#include <sys/param.h>

#define PROC_PIDPATHINFO_MAXSIZE (4 * MAXPATHLEN)

extern "C"
{
    int proc_regionfilename(int pid, uint64_t address, void* buffer, uint32_t buffersize);
}
