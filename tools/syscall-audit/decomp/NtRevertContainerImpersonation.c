__int64 NtRevertContainerImpersonation()                                   | 
{                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]            | 
                                                                           | 
  return PspRevertContainerImpersonation((ULONG_PTR)KeGetCurrentThread()); | 
}                                                                          | 
6 row(s)
