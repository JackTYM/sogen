__int64 __fastcall NtEnumerateSystemEnvironmentValuesEx(unsigned int a1, volatile void *a2, unsigned int *a3)        | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  v14 = 0LL;                                                                                                         | 
  v13 = 0;                                                                                                           | 
  P[0] = 0LL;                                                                                                        | 
  if ( dword_140EFED30 != 2 || (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                    | 
  {                                                                                                                  | 
    return 3221225474LL;                                                                                             | 
  }                                                                                                                  | 
  CurrentThread = KeGetCurrentThread();                                                                              | 
  PreviousMode = CurrentThread->PreviousMode;                                                                        | 
  if ( PreviousMode )                                                                                                | 
  {                                                                                                                  | 
    v8 = 0x7FFFFFFF0000LL;                                                                                           | 
    if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )                                                                   | 
    {                                                                                                                | 
      v8 = (__int64)a3;                                                                                              | 
    }                                                                                                                | 
    *(_DWORD *)v8 = *(_DWORD *)v8;                                                                                   | 
    v9 = *a3;                                                                                                        | 
    v13 = *a3;                                                                                                       | 
    if ( !a2 )                                                                                                       | 
    {                                                                                                                | 
      v13 = 0;                                                                                                       | 
      v9 = 0;                                                                                                        | 
    }                                                                                                                | 
    if ( v9 )                                                                                                        | 
    {                                                                                                                | 
      ProbeForWrite(a2, v9, 4u);                                                                                     | 
    }                                                                                                                | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                                       | 
    {                                                                                                                | 
      return 3221225569LL;                                                                                           | 
    }                                                                                                                | 
  }                                                                                                                  | 
  else                                                                                                               | 
  {                                                                                                                  | 
    v13 = *a3;                                                                                                       | 
    if ( !a2 )                                                                                                       | 
    {                                                                                                                | 
      v13 = 0;                                                                                                       | 
    }                                                                                                                | 
  }                                                                                                                  | 
  if ( !v13 || (LOBYTE(v6) = PreviousMode, result = ExLockUserBuffer(a2, v13, v6, 1LL, &v14, P), (int)result >= 0) ) | 
  {                                                                                                                  | 
    v11 = KeGetCurrentThread();                                                                                      | 
    --v11->KernelApcDisable;                                                                                         | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                                   | 
    v12 = IoEnumerateEnvironmentVariablesEx(a1, 0LL, v14, &v13);                                                     | 
    ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                                   | 
    KeLeaveCriticalRegion();                                                                                         | 
    if ( P[0] )                                                                                                      | 
    {                                                                                                                | 
      ExUnlockUserBuffer(P[0]);                                                                                      | 
    }                                                                                                                | 
    *a3 = v13;                                                                                                       | 
    return v12;                                                                                                      | 
  }                                                                                                                  | 
  return result;                                                                                                     | 
}                                                                                                                    | 
62 row(s)
