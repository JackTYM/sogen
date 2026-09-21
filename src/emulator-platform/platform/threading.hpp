#pragma once

#include "kernel_mapped.hpp"

// NOLINTBEGIN(modernize-use-using,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-use-enum-class)

namespace sogen
{

    typedef enum _THREADINFOCLASS
    {
        ThreadBasicInformation,          // q: THREAD_BASIC_INFORMATION
        ThreadTimes,                     // q: KERNEL_USER_TIMES
        ThreadPriority,                  // s: KPRIORITY (requires SeIncreaseBasePriorityPrivilege)
        ThreadBasePriority,              // s: KPRIORITY
        ThreadAffinityMask,              // s: KAFFINITY
        ThreadImpersonationToken,        // s: HANDLE
        ThreadDescriptorTableEntry,      // q: DESCRIPTOR_TABLE_ENTRY (or WOW64_DESCRIPTOR_TABLE_ENTRY)
        ThreadEnableAlignmentFaultFixup, // s: BOOLEAN
        ThreadEventPair,
        ThreadQuerySetWin32StartAddress, // q: ULONG_PTR
        ThreadZeroTlsCell,               // s: ULONG // TlsIndex // 10
        ThreadPerformanceCount,          // q: LARGE_INTEGER
        ThreadAmILastThread,             // q: ULONG
        ThreadIdealProcessor,            // s: ULONG
        ThreadPriorityBoost,             // qs: ULONG
        ThreadSetTlsArrayAddress,        // s: ULONG_PTR // Obsolete
        ThreadIsIoPending,               // q: ULONG
        ThreadHideFromDebugger,          // q: BOOLEAN; s: void
        ThreadBreakOnTermination,        // qs: ULONG
        ThreadSwitchLegacyState,         // s: void // NtCurrentThread // NPX/FPU
        ThreadIsTerminated,              // q: ULONG // 20
        ThreadLastSystemCall,            // q: THREAD_LAST_SYSCALL_INFORMATION
        ThreadIoPriority,                // qs: IO_PRIORITY_HINT (requires SeIncreaseBasePriorityPrivilege)
        ThreadCycleTime,                 // q: THREAD_CYCLE_TIME_INFORMATION
        ThreadPagePriority,              // qs: PAGE_PRIORITY_INFORMATION
        ThreadActualBasePriority,        // s: LONG (requires SeIncreaseBasePriorityPrivilege)
        ThreadTebInformation,            // q: THREAD_TEB_INFORMATION (requires THREAD_GET_CONTEXT + THREAD_SET_CONTEXT)
        ThreadCSwitchMon,                // Obsolete
        ThreadCSwitchPmu,
        ThreadWow64Context,             // qs: WOW64_CONTEXT, ARM_NT_CONTEXT since 20H1
        ThreadGroupInformation,         // qs: GROUP_AFFINITY // 30
        ThreadUmsInformation,           // q: THREAD_UMS_INFORMATION // Obsolete
        ThreadCounterProfiling,         // q: BOOLEAN; s: THREAD_PROFILING_INFORMATION?
        ThreadIdealProcessorEx,         // qs: PROCESSOR_NUMBER; s: previous PROCESSOR_NUMBER on return
        ThreadCpuAccountingInformation, // q: BOOLEAN; s: HANDLE (NtOpenSession) // NtCurrentThread // since WIN8
        ThreadSuspendCount,             // q: ULONG // since WINBLUE
        ThreadHeterogeneousCpuPolicy,   // q: KHETERO_CPU_POLICY // since THRESHOLD
        ThreadContainerId,              // q: GUID
        ThreadNameInformation,          // qs: THREAD_NAME_INFORMATION
        ThreadSelectedCpuSets,
        ThreadSystemThreadInformation,        // q: SYSTEM_THREAD_INFORMATION // 40
        ThreadActualGroupAffinity,            // q: GROUP_AFFINITY // since THRESHOLD2
        ThreadDynamicCodePolicyInfo,          // q: ULONG; s: ULONG (NtCurrentThread)
        ThreadExplicitCaseSensitivity,        // qs: ULONG; s: 0 disables, otherwise enables
        ThreadWorkOnBehalfTicket,             // RTL_WORK_ON_BEHALF_TICKET_EX
        ThreadSubsystemInformation,           // q: SUBSYSTEM_INFORMATION_TYPE // since REDSTONE2
        ThreadDbgkWerReportActive,            // s: ULONG; s: 0 disables, otherwise enables
        ThreadAttachContainer,                // s: HANDLE (job object) // NtCurrentThread
        ThreadManageWritesToExecutableMemory, // MANAGE_WRITES_TO_EXECUTABLE_MEMORY // since REDSTONE3
        ThreadPowerThrottlingState,           // POWER_THROTTLING_THREAD_STATE // since REDSTONE3 (set), WIN11 22H2 (query)
        ThreadWorkloadClass,                  // THREAD_WORKLOAD_CLASS // since REDSTONE5 // 50
        ThreadCreateStateChange,              // since WIN11
        ThreadApplyStateChange,
        ThreadStrongerBadHandleChecks, // since 22H1
        ThreadEffectiveIoPriority,     // q: IO_PRIORITY_HINT
        ThreadEffectivePagePriority,   // q: ULONG
        ThreadUpdateLockOwnership,     // since 24H2
        ThreadSchedulerSharedDataSlot, // SCHEDULER_SHARED_DATA_SLOT_INFORMATION
        ThreadTebInformationAtomic,    // THREAD_TEB_INFORMATION
        ThreadIndexInformation,        // THREAD_INDEX_INFORMATION
        MaxThreadInfoClass
    } THREADINFOCLASS;

    template <typename Traits>
    struct THREAD_NAME_INFORMATION
    {
        UNICODE_STRING<Traits> ThreadName;
    };

    typedef struct _THREAD_BASIC_INFORMATION64
    {
        NTSTATUS ExitStatus;
        EMULATOR_CAST(uint64_t, PTEB64) TebBaseAddress;
        CLIENT_ID64 ClientId;
        EMULATOR_CAST(std::uint64_t, KAFFINITY) AffinityMask;
        EMULATOR_CAST(std::uint32_t, KPRIORITY) Priority;
        EMULATOR_CAST(std::uint32_t, KPRIORITY) BasePriority;
    } THREAD_BASIC_INFORMATION64, *PTHREAD_BASIC_INFORMATION64;

    // A 32-bit WOW64 caller passes this smaller layout (28 bytes: 32-bit TebBaseAddress/AffinityMask,
    // CLIENT_ID32 instead of CLIENT_ID64), not THREAD_BASIC_INFORMATION64's 44 bytes.
    typedef struct _THREAD_BASIC_INFORMATION32
    {
        NTSTATUS ExitStatus;
        EMULATOR_CAST(uint32_t, PTEB32) TebBaseAddress;
        CLIENT_ID32 ClientId;
        EMULATOR_CAST(std::uint32_t, KAFFINITY) AffinityMask;
        EMULATOR_CAST(std::uint32_t, KPRIORITY) Priority;
        EMULATOR_CAST(std::uint32_t, KPRIORITY) BasePriority;
    } THREAD_BASIC_INFORMATION32, *PTHREAD_BASIC_INFORMATION32;

    typedef struct _THREAD_TEB_INFORMATION
    {
        EmulatorTraits<Emu64>::PVOID TebInformation; // Buffer to write data into.
        ULONG TebOffset;                             // Offset in TEB to begin reading from.
        ULONG BytesToRead;                           // Number of bytes to read.
    } THREAD_TEB_INFORMATION, *PTHREAD_TEB_INFORMATION;

    struct THREAD_CYCLE_TIME_INFORMATION
    {
        ULONGLONG AccumulatedCycles;
        ULONGLONG CurrentCycleCount;
    };

    // A 32-bit caller (e.g. wow64.dll's own exception-preparation code, which queries
    // ThreadTebInformation as 32-bit guest code) passes this smaller layout - TebInformation is a
    // 32-bit pointer here, making the struct 12 bytes instead of THREAD_TEB_INFORMATION's 16.
    typedef struct _THREAD_TEB_INFORMATION32
    {
        EmulatorTraits<Emu32>::PVOID TebInformation;
        ULONG TebOffset;
        ULONG BytesToRead;
    } THREAD_TEB_INFORMATION32, *PTHREAD_TEB_INFORMATION32;

    typedef enum _KCONTINUE_TYPE
    {
        KCONTINUE_UNWIND,
        KCONTINUE_RESUME,
        KCONTINUE_LONGJUMP,
        KCONTINUE_SET,
        KCONTINUE_LAST,
    } KCONTINUE_TYPE;

    typedef struct _KCONTINUE_ARGUMENT
    {
        KCONTINUE_TYPE ContinueType;
        ULONG ContinueFlags;
        ULONGLONG Reserved[2];
    } KCONTINUE_ARGUMENT, *PKCONTINUE_ARGUMENT;

#define KCONTINUE_FLAG_TEST_ALERT  0x00000001
#define KCONTINUE_FLAG_DELIVER_APC 0x00000002

#ifndef OS_WINDOWS
    typedef enum _QUEUE_USER_APC_FLAGS
    {
        QUEUE_USER_APC_FLAGS_NONE,
        QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC,
        QUEUE_USER_APC_CALLBACK_DATA_CONTEXT
    } QUEUE_USER_APC_FLAGS;
#endif

    // NOLINTEND(modernize-use-using,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-use-enum-class)
} // namespace sogen
