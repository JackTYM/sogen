__int64 __fastcall NtAllocateVirtualMemoryEx(int a1, int a2, int a3, int a4, int a5, volatile void *Address, int a7)
{                                                                                                                   
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     
                                                                                                                    
  return MmAllocateVirtualMemory(a1, a2, a3, a4, a5, Address, a7, KeGetCurrentThread()->PreviousMode, 0, 0, 0LL);   
}                                                                                                                   
