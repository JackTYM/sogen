NTSTATUS __stdcall NtFreeVirtualMemory(HANDLE ProcessHandle, PVOID *BaseAddress, PSIZE_T RegionSize, ULONG FreeType) | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  if ( PreviousMode )                                                                                                | 
  {                                                                                                                  | 
    v8 = 0x7FFFFFFF0000LL;                                                                                           | 
    v9 = 0x7FFFFFFF0000LL;                                                                                           | 
    if ( (unsigned __int64)BaseAddress < 0x7FFFFFFF0000LL )                                                          | 
    {                                                                                                                | 
      v9 = (__int64)BaseAddress;                                                                                     | 
    }                                                                                                                | 
    *(_QWORD *)v9 = *(_QWORD *)v9;                                                                                   | 
    if ( (unsigned __int64)RegionSize < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                                | 
      v8 = (__int64)RegionSize;                                                                                      | 
    }                                                                                                                | 
    *(_QWORD *)v8 = *(_QWORD *)v8;                                                                                   | 
  }                                                                                                                  | 
  v12 = *BaseAddress;                                                                                                | 
  v11 = *RegionSize;                                                                                                 | 
  result = MmFreeVirtualMemory((ULONG_PTR)ProcessHandle, PreviousMode, 0);                                           | 
  if ( result >= 0 )                                                                                                 | 
  {                                                                                                                  | 
    *RegionSize = v11;                                                                                               | 
    *BaseAddress = v12;                                                                                              | 
  }                                                                                                                  | 
  return result;                                                                                                     | 
}                                                                                                                    | 
