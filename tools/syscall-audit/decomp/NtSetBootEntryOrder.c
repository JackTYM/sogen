__int64 __fastcall NtSetBootEntryOrder(__int64 a1, unsigned int a2)                                         | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v2 = a2;                                                                                                  | 
  Pool2 = 0LL;                                                                                              | 
  if ( dword_140EFED30 != 2 )                                                                               | 
  {                                                                                                         | 
    return 3221225474LL;                                                                                    | 
  }                                                                                                         | 
  if ( a2 > 0x3FFFFFFF )                                                                                    | 
  {                                                                                                         | 
    return 3221225485LL;                                                                                    | 
  }                                                                                                         | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                   | 
  {                                                                                                         | 
    return 3221225474LL;                                                                                    | 
  }                                                                                                         | 
  CurrentThread = KeGetCurrentThread();                                                                     | 
  PreviousMode = CurrentThread->PreviousMode;                                                               | 
  if ( PreviousMode && !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, CurrentThread->PreviousMode) ) | 
  {                                                                                                         | 
    return 3221225569LL;                                                                                    | 
  }                                                                                                         | 
  if ( (_DWORD)v2 )                                                                                         | 
  {                                                                                                         | 
    Pool2 = (void *)ExAllocatePool2(0x40uLL, 2 * v2, 0x72766E45u);                                          | 
    if ( !Pool2 )                                                                                           | 
    {                                                                                                       | 
      return 3221225626LL;                                                                                  | 
    }                                                                                                       | 
    if ( PreviousMode && 4 * (_DWORD)v2 && (a1 & 3) != 0 )                                                  | 
    {                                                                                                       | 
      ExRaiseDatatypeMisalignment();                                                                        | 
    }                                                                                                       | 
    for ( i = 0LL; (unsigned int)i < (unsigned int)v2; i = (unsigned int)(i + 1) )                          | 
    {                                                                                                       | 
      if ( *(_DWORD *)(a1 + 4 * i) > 0xFFFFu )                                                              | 
      {                                                                                                     | 
        ExFreePoolWithTag(Pool2, 0);                                                                        | 
        return 3221225485LL;                                                                                | 
      }                                                                                                     | 
      *((_WORD *)Pool2 + i) = *(_WORD *)(a1 + 4 * i);                                                       | 
    }                                                                                                       | 
  }                                                                                                         | 
  v7 = KeGetCurrentThread();                                                                                | 
  --v7->KernelApcDisable;                                                                                   | 
  ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                            | 
  v8 = IoSetEnvironmentVariableEx(                                                                          | 
         (unsigned int)L"BootOrder",                                                                        | 
         (unsigned int)&EfiBootVariablesGuid,                                                               | 
         (_DWORD)Pool2,                                                                                     | 
         2 * (int)v2,                                                                                       | 
         1);                                                                                                | 
  ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                            | 
  KeLeaveCriticalRegion();                                                                                  | 
  v9 = 0;                                                                                                   | 
  if ( v8 != -1073741568 )                                                                                  | 
  {                                                                                                         | 
    v9 = v8;                                                                                                | 
  }                                                                                                         | 
  if ( Pool2 )                                                                                              | 
  {                                                                                                         | 
    ExFreePoolWithTag(Pool2, 0);                                                                            | 
  }                                                                                                         | 
  return v9;                                                                                                | 
}                                                                                                           | 
67 row(s)
