__int64 NtWriteRequestData()                                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  CurrentThread = KeGetCurrentThread();                         | 
  --CurrentThread->KernelApcDisable;                            | 
  KeLeaveCriticalRegion();                                      | 
  return 3221225659LL;                                          | 
}                                                               | 
9 row(s)
