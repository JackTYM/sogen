__int64 __fastcall NtGetEnvironmentVariableEx(int a1, int a2, int a3, int a4, __int64 a5) | 
{                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                           | 
                                                                                          | 
  if ( dword_140EFED30 != 2 )                                                             | 
  {                                                                                       | 
    return 3221225474LL;                                                                  | 
  }                                                                                       | 
  if ( KeGetCurrentThread()->PreviousMode )                                               | 
  {                                                                                       | 
    return 3221225569LL;                                                                  | 
  }                                                                                       | 
  CurrentThread = KeGetCurrentThread();                                                   | 
  --CurrentThread->KernelApcDisable;                                                      | 
  ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                          | 
  EnvironmentVariable = IoGetEnvironmentVariableEx(a1, a2, a3, a4, a5);                   | 
  ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                          | 
  KeLeaveCriticalRegion();                                                                | 
  return EnvironmentVariable;                                                             | 
}                                                                                         | 
20 row(s)
