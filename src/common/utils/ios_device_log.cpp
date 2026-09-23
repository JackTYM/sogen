#include "ios_device_log.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR

#include <string>

// Defined in tools/sogen-ios/Sources/JIT/DeviceJITLog.swift, part of the SogenIOS app target
// rather than this CMake target; tied together only at final link time inside the app's own
// Mach-O image.
extern "C" void sogen_jit26_device_log(const char* line);

namespace sogen
{
    namespace utils
    {
        void log_ios_device_milestone(const std::string_view line)
        {
            const std::string owned(line);
            sogen_jit26_device_log(owned.c_str());
        }
    } // namespace utils
} // namespace sogen

#else

namespace sogen
{
    namespace utils
    {
        void log_ios_device_milestone(std::string_view)
        {
        }
    } // namespace utils
} // namespace sogen

#endif
