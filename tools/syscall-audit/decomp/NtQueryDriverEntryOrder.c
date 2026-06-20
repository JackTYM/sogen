__int64 __fastcall NtQueryDriverEntryOrder(volatile void *Address, unsigned int *a2)                                    | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v17 = 0LL;                                                                                                            | 
  v20 = 0;                                                                                                              | 
  P[0] = 0LL;                                                                                                           | 
  if ( dword_140EFED30 != 2 || (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                       | 
  {                                                                                                                     | 
    return 3221225474LL;                                                                                                | 
  }                                                                                                                     | 
  CurrentThread = KeGetCurrentThread();                                                                                 | 
  PreviousMode = CurrentThread->PreviousMode;                                                                           | 
  if ( PreviousMode )                                                                                                   | 
  {                                                                                                                     | 
    v6 = 0x7FFFFFFF0000LL;                                                                                              | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                                   | 
      v6 = (__int64)a2;                                                                                                 | 
    }                                                                                                                   | 
    *(_DWORD *)v6 = *(_DWORD *)v6;                                                                                      | 
    v7 = 4 * *a2;                                                                                                       | 
    v20 = v7;                                                                                                           | 
    v8 = v7;                                                                                                            | 
    if ( !Address )                                                                                                     | 
    {                                                                                                                   | 
      v7 = 0;                                                                                                           | 
      v20 = 0;                                                                                                          | 
      v8 = 0;                                                                                                           | 
    }                                                                                                                   | 
    if ( v8 )                                                                                                           | 
    {                                                                                                                   | 
      ProbeForWrite(Address, v8, 4u);                                                                                   | 
    }                                                                                                                   | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                                          | 
    {                                                                                                                   | 
      return 3221225569LL;                                                                                              | 
    }                                                                                                                   | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    v7 = Address != 0LL ? 4 * *a2 : 0;                                                                                  | 
    v20 = v7;                                                                                                           | 
  }                                                                                                                     | 
  if ( !v7 || (LOBYTE(v4) = PreviousMode, result = ExLockUserBuffer(Address, v7, v4, 1LL, &v17, P), (int)result >= 0) ) | 
  {                                                                                                                     | 
    v20 = v7 >> 1;                                                                                                      | 
    v10 = KeGetCurrentThread();                                                                                         | 
    --v10->KernelApcDisable;                                                                                            | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                                      | 
    v11 = v17;                                                                                                          | 
    EnvironmentVariable = HalGetEnvironmentVariableEx(                                                                  | 
                            (unsigned int)L"DriverOrder",                                                               | 
                            (unsigned int)&EfiDriverVariablesGuid,                                                      | 
                            v17,                                                                                        | 
                            (unsigned int)&v20,                                                                         | 
                            0LL);                                                                                       | 
    ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                                      | 
    KeLeaveCriticalRegion();                                                                                            | 
    if ( EnvironmentVariable < 0 )                                                                                      | 
    {                                                                                                                   | 
      if ( EnvironmentVariable == -1073741568 )                                                                         | 
      {                                                                                                                 | 
        v20 = 0;                                                                                                        | 
        EnvironmentVariable = 0;                                                                                        | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v13 = v20 >> 1;                                                                                                   | 
      v14 = (unsigned __int16 *)(v11 + 2LL * ((v20 >> 1) - 1));                                                         | 
      for ( i = (_DWORD *)(v11 + 4LL * ((v20 >> 1) - 1)); v13; --v13 )                                                  | 
      {                                                                                                                 | 
        *i-- = *v14--;                                                                                                  | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    v16 = 2 * v20;                                                                                                      | 
    v20 *= 2;                                                                                                           | 
    if ( P[0] )                                                                                                         | 
    {                                                                                                                   | 
      ExUnlockUserBuffer(P[0]);                                                                                         | 
    }                                                                                                                   | 
    *a2 = v16 >> 2;                                                                                                     | 
    return (unsigned int)EnvironmentVariable;                                                                           | 
  }                                                                                                                     | 
  return result;                                                                                                        | 
}                                                                                                                       | 
87 row(s)
