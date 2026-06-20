NTSTATUS __stdcall NtQueryVirtualMemory(                                                                    | 
        HANDLE ProcessHandle,                                                                               | 
        PVOID BaseAddress,                                                                                  | 
        MEMORY_INFORMATION_CLASS MemoryInformationClass,                                                    | 
        PVOID MemoryInformation,                                                                            | 
        SIZE_T MemoryInformationLength,                                                                     | 
        PSIZE_T ReturnLength)                                                                               | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  return MmQueryVirtualMemory((ULONG_PTR)ProcessHandle, MemoryInformationLength, (__int64)ReturnLength, 2); | 
}                                                                                                           | 
