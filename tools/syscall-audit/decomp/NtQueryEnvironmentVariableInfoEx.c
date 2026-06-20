__int64 __fastcall NtQueryEnvironmentVariableInfoEx(unsigned int a1, __int64 a2, __int64 a3, __int64 a4) | 
{                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                          | 
                                                                                                         | 
  if ( dword_140EFED30 != 2 )                                                                            | 
  {                                                                                                      | 
    return 3221225474LL;                                                                                 | 
  }                                                                                                      | 
  if ( KeGetCurrentThread()->PreviousMode )                                                              | 
  {                                                                                                      | 
    return 3221225569LL;                                                                                 | 
  }                                                                                                      | 
  CurrentThread = KeGetCurrentThread();                                                                  | 
  --CurrentThread->KernelApcDisable;                                                                     | 
  ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                         | 
  EnvironmentVariableInfo = IoQueryEnvironmentVariableInfoEx(a1, a2, a3, a4);                            | 
  ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                         | 
  KeLeaveCriticalRegion();                                                                               | 
  return EnvironmentVariableInfo;                                                                        | 
}                                                                                                        | 
20 row(s)
