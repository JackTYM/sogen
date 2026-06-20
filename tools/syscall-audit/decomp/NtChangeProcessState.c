__int64 __fastcall NtChangeProcessState(ULONG_PTR a1, ULONG_PTR a2, unsigned int a3, __int64 a4, int a5, int a6) | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  Object = 0LL;                                                                                                  | 
  v20 = 0LL;                                                                                                     | 
  if ( a3 >= 2 )                                                                                                 | 
  {                                                                                                              | 
    return (unsigned int)-1073741821;                                                                            | 
  }                                                                                                              | 
  if ( a5 )                                                                                                      | 
  {                                                                                                              | 
    return (unsigned int)-1073741820;                                                                            | 
  }                                                                                                              | 
  if ( a4 || a6 )                                                                                                | 
  {                                                                                                              | 
    return (unsigned int)-1073741811;                                                                            | 
  }                                                                                                              | 
  v8 = ObpReferenceObjectByHandleWithTag(a1, 0x63507350u, (__int64)&v20, 0LL, 0LL);                              | 
  if ( v8 >= 0 )                                                                                                 | 
  {                                                                                                              | 
    v9 = ObpReferenceObjectByHandleWithTag(a2, 0x63507350u, (__int64)&Object, 0LL, 0LL);                         | 
    v10 = Object;                                                                                                | 
    v8 = v9;                                                                                                     | 
    if ( v9 < 0 )                                                                                                | 
    {                                                                                                            | 
      goto LABEL_30;                                                                                             | 
    }                                                                                                            | 
    if ( *(PVOID *)v20 != Object )                                                                               | 
    {                                                                                                            | 
      v8 = -1073741811;                                                                                          | 
LABEL_30:                                                                                                        | 
      if ( v10 )                                                                                                 | 
      {                                                                                                          | 
        ObfDereferenceObjectWithTag(v10, 0x63507350u);                                                           | 
      }                                                                                                          | 
      goto LABEL_32;                                                                                             | 
    }                                                                                                            | 
    CurrentThread = KeGetCurrentThread();                                                                        | 
    v12 = (volatile signed __int32 *)((char *)v20 + 8);                                                          | 
    v13 = (char *)v20 + 8;                                                                                       | 
    --CurrentThread->KernelApcDisable;                                                                           | 
    v14 = KeAbPreAcquire(v13, 0LL, 0LL);                                                                         | 
    v15 = v14;                                                                                                   | 
    if ( _interlockedbittestandset64(v12, 0LL) )                                                                 | 
    {                                                                                                            | 
      ExfAcquirePushLockExclusiveEx(v12, v14, v12);                                                              | 
    }                                                                                                            | 
    if ( v15 )                                                                                                   | 
    {                                                                                                            | 
      *(_BYTE *)(v15 + 10) = 1;                                                                                  | 
    }                                                                                                            | 
    if ( a3 )                                                                                                    | 
    {                                                                                                            | 
      if ( a3 == 1 )                                                                                             | 
      {                                                                                                          | 
        v16 = v20;                                                                                               | 
        if ( !*((_DWORD *)v20 + 4) )                                                                             | 
        {                                                                                                        | 
          v8 = -1073741811;                                                                                      | 
LABEL_27:                                                                                                        | 
          if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v12, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )     | 
          {                                                                                                      | 
            ExfTryToWakePushLock(v12);                                                                           | 
          }                                                                                                      | 
          KeAbPostRelease((ULONG_PTR)v12);                                                                       | 
          KiLeaveCriticalRegionUnsafe(CurrentThread);                                                            | 
          v10 = Object;                                                                                          | 
          goto LABEL_30;                                                                                         | 
        }                                                                                                        | 
        PsThawMultiProcess(Object, 0LL, 1LL);                                                                    | 
        --v16[4];                                                                                                | 
      }                                                                                                          | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v17 = v20;                                                                                                 | 
      if ( *((_DWORD *)v20 + 4) == -1 )                                                                          | 
      {                                                                                                          | 
        v8 = -1073741750;                                                                                        | 
        goto LABEL_27;                                                                                           | 
      }                                                                                                          | 
      if ( !(unsigned __int8)PsFreezeProcess(Object, 0LL) )                                                      | 
      {                                                                                                          | 
        v8 = -1073741558;                                                                                        | 
        goto LABEL_27;                                                                                           | 
      }                                                                                                          | 
      ++v17[4];                                                                                                  | 
    }                                                                                                            | 
    v8 = 0;                                                                                                      | 
    goto LABEL_27;                                                                                               | 
  }                                                                                                              | 
LABEL_32:                                                                                                        | 
  if ( v20 )                                                                                                     | 
  {                                                                                                              | 
    ObfDereferenceObjectWithTag(v20, 0x63507350u);                                                               | 
  }                                                                                                              | 
  return (unsigned int)v8;                                                                                       | 
}                                                                                                                | 
99 row(s)
