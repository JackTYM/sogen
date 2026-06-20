__int64 __fastcall NtDeleteBootEntry(unsigned int a1)                                                                    | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  if ( dword_140EFED30 == 2 && !(unsigned __int8)PsIsCurrentThreadInServerSilo() )                                       | 
  {                                                                                                                      | 
    if ( a1 > 0xFFFF )                                                                                                   | 
    {                                                                                                                    | 
      return 3221225485LL;                                                                                               | 
    }                                                                                                                    | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                   | 
    if ( PreviousMode && !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                           | 
    {                                                                                                                    | 
      return 3221225569LL;                                                                                               | 
    }                                                                                                                    | 
    CurrentThread = KeGetCurrentThread();                                                                                | 
    --CurrentThread->KernelApcDisable;                                                                                   | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                                       | 
    swprintf_s(Dst, 9uLL, L"Boot%04X", a1);                                                                              | 
    v6 = 0;                                                                                                              | 
    EnvironmentVariable = IoGetEnvironmentVariableEx(                                                                    | 
                            (unsigned int)Dst,                                                                           | 
                            (unsigned int)&EfiBootVariablesGuid,                                                         | 
                            0,                                                                                           | 
                            (unsigned int)&v6,                                                                           | 
                            0LL);                                                                                        | 
    if ( EnvironmentVariable == -1073741568 )                                                                            | 
    {                                                                                                                    | 
      if ( ((2 * ((a1 | (2 * a1)) & 0xC4444444)) & a1) == 0 )                                                            | 
      {                                                                                                                  | 
LABEL_14:                                                                                                                | 
        ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                                   | 
        KeLeaveCriticalRegion();                                                                                         | 
        return EnvironmentVariable;                                                                                      | 
      }                                                                                                                  | 
      swprintf_s(Dst, 9uLL, L"Boot%04x", a1);                                                                            | 
      v6 = 0;                                                                                                            | 
      EnvironmentVariable = IoGetEnvironmentVariableEx(                                                                  | 
                              (unsigned int)Dst,                                                                         | 
                              (unsigned int)&EfiBootVariablesGuid,                                                       | 
                              0,                                                                                         | 
                              (unsigned int)&v6,                                                                         | 
                              0LL);                                                                                      | 
    }                                                                                                                    | 
    if ( !EnvironmentVariable || EnvironmentVariable == -1073741789 )                                                    | 
    {                                                                                                                    | 
      EnvironmentVariable = IoSetEnvironmentVariableEx((unsigned int)Dst, (unsigned int)&EfiBootVariablesGuid, 0, 0, 1); | 
    }                                                                                                                    | 
    goto LABEL_14;                                                                                                       | 
  }                                                                                                                      | 
  return 3221225474LL;                                                                                                   | 
}                                                                                                                        | 
52 row(s)
