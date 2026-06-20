NTSTATUS __fastcall NtMakePermanentObject(HANDLE Handle)                                                        | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                            | 
  if ( !SeSinglePrivilegeCheck(SeCreatePermanentPrivilege, PreviousMode) )                                      | 
  {                                                                                                             | 
    return -1073741727;                                                                                         | 
  }                                                                                                             | 
  Object = 0LL;                                                                                                 | 
  result = ObReferenceObjectByHandle(Handle, 0, 0LL, PreviousMode, &Object, 0LL);                               | 
  v4 = result;                                                                                                  | 
  if ( result >= 0 )                                                                                            | 
  {                                                                                                             | 
    CurrentThread = KeGetCurrentThread();                                                                       | 
    v6 = (volatile signed __int32 *)((char *)Object - 32);                                                      | 
    v7 = (char *)Object - 32;                                                                                   | 
    --CurrentThread->KernelApcDisable;                                                                          | 
    v8 = KeAbPreAcquire(v7, 0LL, 0LL);                                                                          | 
    v9 = v8;                                                                                                    | 
    if ( _interlockedbittestandset64(v6, 0LL) )                                                                 | 
    {                                                                                                           | 
      ExfAcquirePushLockExclusiveEx(v6, v8, v6);                                                                | 
    }                                                                                                           | 
    if ( v9 )                                                                                                   | 
    {                                                                                                           | 
      *(_BYTE *)(v9 + 10) = 1;                                                                                  | 
    }                                                                                                           | 
    *((char *)Object - 21) |= 0x10u;                                                                            | 
    _m_prefetchw((const void *)v6);                                                                             | 
    v10 = *(_QWORD *)v6;                                                                                        | 
    v11 = *(_QWORD *)v6 - 16LL;                                                                                 | 
    if ( (*(_QWORD *)v6 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )                                                      | 
    {                                                                                                           | 
      v11 = 0LL;                                                                                                | 
    }                                                                                                           | 
    if ( (v10 & 2) != 0                                                                                         | 
      || (v12 = *(_QWORD *)v6, v12 != _InterlockedCompareExchange64((volatile signed __int64 *)v6, v11, v10)) ) | 
    {                                                                                                           | 
      ExfReleasePushLock(v6, v11);                                                                              | 
    }                                                                                                           | 
    KeAbPostRelease((ULONG_PTR)v6);                                                                             | 
    KeLeaveCriticalRegion();                                                                                    | 
    ObfDereferenceObject(Object);                                                                               | 
    return v4;                                                                                                  | 
  }                                                                                                             | 
  return result;                                                                                                | 
}                                                                                                               | 
48 row(s)
