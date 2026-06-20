__int64 __fastcall NtChangeThreadState(ULONG_PTR a1, ULONG_PTR a2, unsigned int a3, __int64 a4, int a5, int a6) | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  Object = 0LL;                                                                                                 | 
  v20 = 0LL;                                                                                                    | 
  if ( a3 <= 1 )                                                                                                | 
  {                                                                                                             | 
    if ( a5 )                                                                                                   | 
    {                                                                                                           | 
      return (unsigned int)-1073741820;                                                                         | 
    }                                                                                                           | 
    if ( a4 )                                                                                                   | 
    {                                                                                                           | 
      return (unsigned int)-1073741811;                                                                         | 
    }                                                                                                           | 
  }                                                                                                             | 
  if ( a6 )                                                                                                     | 
  {                                                                                                             | 
    return (unsigned int)-1073741811;                                                                           | 
  }                                                                                                             | 
  v8 = ObpReferenceObjectByHandleWithTag(a1, 0x63547350u, (__int64)&v20, 0LL, 0LL);                             | 
  if ( v8 >= 0 )                                                                                                | 
  {                                                                                                             | 
    v9 = ObpReferenceObjectByHandleWithTag(a2, 0x63547350u, (__int64)&Object, 0LL, 0LL);                        | 
    v10 = Object;                                                                                               | 
    v8 = v9;                                                                                                    | 
    if ( v9 >= 0 )                                                                                              | 
    {                                                                                                           | 
      if ( *(PVOID *)v20 == Object )                                                                            | 
      {                                                                                                         | 
        CurrentThread = KeGetCurrentThread();                                                                   | 
        v13 = (volatile signed __int32 *)((char *)v20 + 8);                                                     | 
        v14 = (char *)v20 + 8;                                                                                  | 
        --CurrentThread->KernelApcDisable;                                                                      | 
        v15 = KeAbPreAcquire(v14, 0LL, 0LL);                                                                    | 
        v16 = v15;                                                                                              | 
        if ( _interlockedbittestandset64(v13, 0LL) )                                                            | 
        {                                                                                                       | 
          ExfAcquirePushLockExclusiveEx(v13, v15, v13);                                                         | 
        }                                                                                                       | 
        if ( v16 )                                                                                              | 
        {                                                                                                       | 
          *(_BYTE *)(v16 + 10) = 1;                                                                             | 
        }                                                                                                       | 
        if ( a3 )                                                                                               | 
        {                                                                                                       | 
          if ( a3 == 1 )                                                                                        | 
          {                                                                                                     | 
            v18 = v20;                                                                                          | 
            if ( *((_DWORD *)v20 + 4) )                                                                         | 
            {                                                                                                   | 
              v8 = PsMultiResumeThread(Object, 0LL, 1LL);                                                       | 
              if ( v8 >= 0 )                                                                                    | 
              {                                                                                                 | 
                --v18[4];                                                                                       | 
              }                                                                                                 | 
            }                                                                                                   | 
            else                                                                                                | 
            {                                                                                                   | 
              v8 = -1073741811;                                                                                 | 
            }                                                                                                   | 
          }                                                                                                     | 
        }                                                                                                       | 
        else                                                                                                    | 
        {                                                                                                       | 
          v17 = v20;                                                                                            | 
          if ( *((_DWORD *)v20 + 4) == 0x7FFFFFFF )                                                             | 
          {                                                                                                     | 
            v8 = -1073741750;                                                                                   | 
          }                                                                                                     | 
          else                                                                                                  | 
          {                                                                                                     | 
            v8 = PsSuspendThread(Object, 0LL);                                                                  | 
            if ( v8 >= 0 )                                                                                      | 
            {                                                                                                   | 
              ++v17[4];                                                                                         | 
            }                                                                                                   | 
          }                                                                                                     | 
        }                                                                                                       | 
        if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v13, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )      | 
        {                                                                                                       | 
          ExfTryToWakePushLock(v13);                                                                            | 
        }                                                                                                       | 
        KeAbPostRelease((ULONG_PTR)v13);                                                                        | 
        KiLeaveCriticalRegionUnsafe(CurrentThread);                                                             | 
        v10 = Object;                                                                                           | 
      }                                                                                                         | 
      else                                                                                                      | 
      {                                                                                                         | 
        v8 = -1073741811;                                                                                       | 
      }                                                                                                         | 
    }                                                                                                           | 
    if ( v10 )                                                                                                  | 
    {                                                                                                           | 
      ObfDereferenceObjectWithTag(v10, 0x63547350u);                                                            | 
    }                                                                                                           | 
  }                                                                                                             | 
  if ( v20 )                                                                                                    | 
  {                                                                                                             | 
    ObfDereferenceObjectWithTag(v20, 0x63547350u);                                                              | 
  }                                                                                                             | 
  return (unsigned int)v8;                                                                                      | 
}                                                                                                               | 
104 row(s)
