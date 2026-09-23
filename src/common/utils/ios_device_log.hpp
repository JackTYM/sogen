#pragma once

#include <string_view>

namespace sogen
{
    namespace utils
    {
        // Mirrors a diagnostic line to the same exportable Documents/sogen_log.txt file the
        // SogenIOS app's own [jit]/[sogen] log lines land in (see
        // tools/sogen-ios/Sources/JIT/DeviceJITLog.swift for the @_cdecl bridge this calls into,
        // and ios_device_jit_bless.cpp in the unicorn-emulator backend for the original use of
        // that bridge). A no-op on every platform except real iOS device (TARGET_OS_IPHONE, not
        // Simulator) -- there is no other way to see what happened between a hang and the last
        // thing that made it onto the screen, since a hung process never produces a crash report.
        //
        // Written directly and synchronously rather than through the app's own ObjC
        // appendLog:/dispatch_async(main queue) path, since these calls happen from deep,
        // cross-platform emulator code with no reference to that Objective-C object, and because
        // a milestone log meant to survive a hang on some other thread should not itself depend
        // on the main thread's queue ever getting a chance to run.
        void log_ios_device_milestone(std::string_view line);
    } // namespace utils
} // namespace sogen
