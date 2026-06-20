__int64 __fastcall NtQuerySystemEnvironmentValueEx(                                                               | 
        __m128i *a1,                                                                                              | 
        __int128 *a2,                                                                                             | 
        volatile void *a3,                                                                                        | 
        unsigned int *a4,                                                                                         | 
        _DWORD *a5)                                                                                               | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  Src = 0LL;                                                                                                      | 
  v22 = 0LL;                                                                                                      | 
  v27 = 0LL;                                                                                                      | 
  v20 = 0;                                                                                                        | 
  v23 = 0;                                                                                                        | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                         | 
  {                                                                                                               | 
    return 3221225474LL;                                                                                          | 
  }                                                                                                               | 
  if ( !KeGetCurrentThread()->PreviousMode )                                                                      | 
  {                                                                                                               | 
    return ExGetFirmwareEnvironmentVariable((_DWORD)a1, (_DWORD)a2, (_DWORD)a3, (_DWORD)a4, (__int64)a5);         | 
  }                                                                                                               | 
  v9 = SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, 1);                                                   | 
  if ( !v9 )                                                                                                      | 
  {                                                                                                               | 
    if ( (unsigned __int8)PsIsProcessAppContainer(KeGetCurrentThread()->ApcState.Process) )                       | 
    {                                                                                                             | 
      v9 = ExpFirmwareAccessAppContainerCheck(1LL);                                                               | 
    }                                                                                                             | 
    if ( !v9 )                                                                                                    | 
    {                                                                                                             | 
      RtlCheckTokenMembershipEx(0LL, SeExports->SeUserModeDriversSid);                                            | 
      EnvironmentVariable = -1073741727;                                                                          | 
      goto LABEL_40;                                                                                              | 
    }                                                                                                             | 
  }                                                                                                               | 
  if ( dword_140EFED30 != 2 )                                                                                     | 
  {                                                                                                               | 
    EnvironmentVariable = -1073741822;                                                                            | 
LABEL_40:                                                                                                         | 
    v14 = v22;                                                                                                    | 
    goto LABEL_36;                                                                                                | 
  }                                                                                                               | 
  if ( ((unsigned __int8)a1 & 3) != 0 )                                                                           | 
  {                                                                                                               | 
LABEL_24:                                                                                                         | 
    ExRaiseDatatypeMisalignment();                                                                                | 
  }                                                                                                               | 
  v10 = 0x7FFFFFFF0000LL;                                                                                         | 
  Src = *a1;                                                                                                      | 
  if ( (unsigned __int16)_mm_cvtsi128_si32(Src) )                                                                 | 
  {                                                                                                               | 
    if ( (Src.m128i_i8[8] & 1) != 0 )                                                                             | 
    {                                                                                                             | 
      ExRaiseDatatypeMisalignment();                                                                              | 
    }                                                                                                             | 
    if ( ((unsigned __int8)a2 & 3) == 0 )                                                                         | 
    {                                                                                                             | 
      v11 = 0x7FFFFFFF0000LL;                                                                                     | 
      if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                              | 
      {                                                                                                           | 
        v11 = (__int64)a4;                                                                                        | 
      }                                                                                                           | 
      *(_DWORD *)v11 = *(_DWORD *)v11;                                                                            | 
      v12 = *a4;                                                                                                  | 
      v20 = *a4;                                                                                                  | 
      if ( !a3 )                                                                                                  | 
      {                                                                                                           | 
        v20 = 0;                                                                                                  | 
        v12 = 0;                                                                                                  | 
      }                                                                                                           | 
      if ( v12 )                                                                                                  | 
      {                                                                                                           | 
        ProbeForWrite(a3, v12, 1u);                                                                               | 
      }                                                                                                           | 
      if ( a5 )                                                                                                   | 
      {                                                                                                           | 
        if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                            | 
        {                                                                                                         | 
          v10 = (__int64)a5;                                                                                      | 
        }                                                                                                         | 
        *(_DWORD *)v10 = *(_DWORD *)v10;                                                                          | 
      }                                                                                                           | 
      v27 = *a2;                                                                                                  | 
      Pool2 = (_WORD *)ExAllocatePool2(0x40uLL, Src.m128i_u16[0] + 2LL, 0x72766E45u);                             | 
      v14 = Pool2;                                                                                                | 
      v22 = Pool2;                                                                                                | 
      if ( Pool2 )                                                                                                | 
      {                                                                                                           | 
        memmove(Pool2, (const void *)Src.m128i_i64[1], Src.m128i_u16[0]);                                         | 
        v14[(unsigned __int64)Src.m128i_u16[0] >> 1] = 0;                                                         | 
        P = 0LL;                                                                                                  | 
        v24 = 0LL;                                                                                                | 
        if ( v20 && (LOBYTE(v15) = 1, v17 = ExLockUserBuffer(a3, v20, v15), EnvironmentVariable = v17, v17 < 0) ) | 
        {                                                                                                         | 
          v21 = v17;                                                                                              | 
        }                                                                                                         | 
        else                                                                                                      | 
        {                                                                                                         | 
          CurrentThread = KeGetCurrentThread();                                                                   | 
          --CurrentThread->KernelApcDisable;                                                                      | 
          ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                          | 
          v14 = v22;                                                                                              | 
          EnvironmentVariable = IoGetEnvironmentVariableEx(                                                       | 
                                  (_DWORD)v22,                                                                    | 
                                  (unsigned int)&v27,                                                             | 
                                  v24,                                                                            | 
                                  (unsigned int)&v20,                                                             | 
                                  (__int64)&v23);                                                                 | 
          ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                          | 
          KeLeaveCriticalRegion();                                                                                | 
          if ( P )                                                                                                | 
          {                                                                                                       | 
            ExUnlockUserBuffer(P);                                                                                | 
          }                                                                                                       | 
          v21 = EnvironmentVariable;                                                                              | 
        }                                                                                                         | 
        if ( (int)(EnvironmentVariable + 0x80000000) < 0 || EnvironmentVariable == -1073741789 )                  | 
        {                                                                                                         | 
          *a4 = v20;                                                                                              | 
        }                                                                                                         | 
        if ( a5 && EnvironmentVariable >= 0 )                                                                     | 
        {                                                                                                         | 
          *a5 = v23;                                                                                              | 
        }                                                                                                         | 
      }                                                                                                           | 
      else                                                                                                        | 
      {                                                                                                           | 
        EnvironmentVariable = -1073741670;                                                                        | 
        v21 = -1073741670;                                                                                        | 
      }                                                                                                           | 
      goto LABEL_36;                                                                                              | 
    }                                                                                                             | 
    goto LABEL_24;                                                                                                | 
  }                                                                                                               | 
  EnvironmentVariable = -1073741819;                                                                              | 
  v21 = -1073741819;                                                                                              | 
  v14 = v22;                                                                                                      | 
LABEL_36:                                                                                                         | 
  if ( v14 )                                                                                                      | 
  {                                                                                                               | 
    ExFreePoolWithTag(v14, 0);                                                                                    | 
  }                                                                                                               | 
  return (unsigned int)EnvironmentVariable;                                                                       | 
}                                                                                                                 | 
145 row(s)
