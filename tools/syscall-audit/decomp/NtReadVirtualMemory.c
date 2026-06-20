__int64 __fastcall NtReadVirtualMemory(ULONG_PTR a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5)
{                                                                                                   
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     
                                                                                                    
  return MiReadWriteVirtualMemory(a1, a5, 16, 0);                                                   
}                                                                                                   
