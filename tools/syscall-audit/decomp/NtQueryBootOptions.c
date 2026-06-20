__int64 __fastcall NtQueryBootOptions(_DWORD *Address, _DWORD *a2)                | 
{                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                   | 
                                                                                  | 
  v13 = 0;                                                                        | 
  v14 = 0;                                                                        | 
  v15 = 0;                                                                        | 
  v12 = 0;                                                                        | 
  if ( dword_140EFED30 != 2 || (unsigned __int8)PsIsCurrentThreadInServerSilo() ) | 
  {                                                                               | 
    return 3221225474LL;                                                          | 
  }                                                                               | 
  CurrentThread = KeGetCurrentThread();                                           | 
  PreviousMode = CurrentThread->PreviousMode;                                     | 
  if ( PreviousMode )                                                             | 
  {                                                                               | 
    v4 = 0x7FFFFFFF0000LL;                                                        | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                | 
    {                                                                             | 
      v4 = (__int64)a2;                                                           | 
    }                                                                             | 
    *(_DWORD *)v4 = *(_DWORD *)v4;                                                | 
    v5 = Address != 0LL ? *a2 : 0;                                                | 
    v16 = v5;                                                                     | 
    if ( v5 )                                                                     | 
    {                                                                             | 
      ProbeForWrite(Address, v5, 4u);                                             | 
    }                                                                             | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )    | 
    {                                                                             | 
      return 3221225569LL;                                                        | 
    }                                                                             | 
  }                                                                               | 
  else                                                                            | 
  {                                                                               | 
    v5 = Address != 0LL ? *a2 : 0;                                                | 
    v16 = v5;                                                                     | 
  }                                                                               | 
  if ( v5 >= 0x16 )                                                               | 
  {                                                                               | 
    v8 = KeGetCurrentThread();                                                    | 
    --v8->KernelApcDisable;                                                       | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                | 
    v12 = 4;                                                                      | 
    EnvironmentVariable = IoGetEnvironmentVariableEx(                             | 
                            (unsigned int)L"Timeout",                             | 
                            (unsigned int)&EfiBootVariablesGuid,                  | 
                            (unsigned int)&v13,                                   | 
                            (unsigned int)&v12,                                   | 
                            0LL);                                                 | 
    v7 = EnvironmentVariable;                                                     | 
    if ( EnvironmentVariable == -1073741789 )                                     | 
    {                                                                             | 
      v13 = -2;                                                                   | 
LABEL_23:                                                                         | 
      v12 = 4;                                                                    | 
      v10 = IoGetEnvironmentVariableEx(                                           | 
              (unsigned int)L"BootCurrent",                                       | 
              (unsigned int)&EfiBootVariablesGuid,                                | 
              (unsigned int)&v14,                                                 | 
              (unsigned int)&v12,                                                 | 
              0LL);                                                               | 
      v7 = v10;                                                                   | 
      if ( v10 == -1073741789 || v10 == -1073741568 )                             | 
      {                                                                           | 
        v14 = -2;                                                                 | 
LABEL_29:                                                                         | 
        v12 = 2;                                                                  | 
        v11 = IoGetEnvironmentVariableEx(                                         | 
                (unsigned int)L"BootNext",                                        | 
                (unsigned int)&EfiBootVariablesGuid,                              | 
                (unsigned int)&v15,                                               | 
                (unsigned int)&v12,                                               | 
                0LL);                                                             | 
        v7 = v11;                                                                 | 
        if ( v11 == -1073741789 || v11 == -1073741568 )                           | 
        {                                                                         | 
          v15 = -2;                                                               | 
          v7 = 0;                                                                 | 
        }                                                                         | 
        else if ( !v11 && v12 > 2 )                                               | 
        {                                                                         | 
          v15 = (unsigned __int16)v15;                                            | 
        }                                                                         | 
        goto LABEL_35;                                                            | 
      }                                                                           | 
      if ( !v10 )                                                                 | 
      {                                                                           | 
        if ( v12 > 2 )                                                            | 
        {                                                                         | 
          v14 = (unsigned __int16)v14;                                            | 
        }                                                                         | 
        goto LABEL_29;                                                            | 
      }                                                                           | 
LABEL_35:                                                                         | 
      ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                              | 
      KeLeaveCriticalRegion();                                                    | 
      goto LABEL_36;                                                              | 
    }                                                                             | 
    if ( EnvironmentVariable != -1073741568 )                                     | 
    {                                                                             | 
      if ( EnvironmentVariable )                                                  | 
      {                                                                           | 
        goto LABEL_35;                                                            | 
      }                                                                           | 
      if ( v12 <= 2 )                                                             | 
      {                                                                           | 
LABEL_20:                                                                         | 
        if ( v13 != 0xFFFF )                                                      | 
        {                                                                         | 
          goto LABEL_23;                                                          | 
        }                                                                         | 
        goto LABEL_21;                                                            | 
      }                                                                           | 
      if ( v13 != -1 )                                                            | 
      {                                                                           | 
        if ( v13 > 0xFFFE )                                                       | 
        {                                                                         | 
          v13 = 65534;                                                            | 
        }                                                                         | 
        goto LABEL_20;                                                            | 
      }                                                                           | 
    }                                                                             | 
LABEL_21:                                                                         | 
    v13 = -1;                                                                     | 
    goto LABEL_23;                                                                | 
  }                                                                               | 
  v7 = -1073741789;                                                               | 
LABEL_36:                                                                         | 
  if ( !v7 )                                                                      | 
  {                                                                               | 
    if ( Address )                                                                | 
    {                                                                             | 
      *Address = 1;                                                               | 
      Address[1] = 22;                                                            | 
      Address[2] = v13;                                                           | 
      Address[3] = v14;                                                           | 
      Address[4] = v15;                                                           | 
      *((_WORD *)Address + 10) = 0;                                               | 
    }                                                                             | 
  }                                                                               | 
  *a2 = 22;                                                                       | 
  return v7;                                                                      | 
}                                                                                 | 
144 row(s)
