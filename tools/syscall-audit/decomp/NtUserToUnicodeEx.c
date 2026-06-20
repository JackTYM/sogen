__int64 __fastcall NtUserToUnicodeEx(                                                                                 | 
        unsigned int a1,                                                                                              | 
        __int64 a2,                                                                                                   | 
        const void *a3,                                                                                               | 
        volatile void *a4,                                                                                            | 
        int a5,                                                                                                       | 
        char a6,                                                                                                      | 
        __int64 a7)                                                                                                   | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  Address = a4;                                                                                                       | 
  v29 = a7;                                                                                                           | 
  v9 = 0LL;                                                                                                           | 
  v30 = 0LL;                                                                                                          | 
  Src = 0LL;                                                                                                          | 
  v23 = 0;                                                                                                            | 
  Win32RawLockedNtObject<tagDESKTOP>::Win32RawLockedNtObject<tagDESKTOP>(&BugCheckParameter2);                        | 
  UserSessionState = W32GetUserSessionState();                                                                        | 
  CurrentThreadWin32Thread = (struct _W32THREADNONPAGED *)PsGetCurrentThreadWin32Thread();                            | 
  KeEnterCriticalRegion();                                                                                            | 
  _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_(                                                  | 
    *(struct _FAST_ERESOURCE **)UserSessionState,                                                                     | 
    CurrentThreadWin32Thread);                                                                                        | 
  v12 = *(_QWORD *)CurrentThreadWin32Thread;                                                                          | 
  if ( *(_QWORD *)CurrentThreadWin32Thread )                                                                          | 
  {                                                                                                                   | 
    *(_BYTE *)(v12 + 1708) = 1;                                                                                       | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v12 = 0LL;                                                                                                        | 
  }                                                                                                                   | 
  *(_QWORD *)(UserSessionState + 16) = v12;                                                                           | 
  if ( v12 )                                                                                                          | 
  {                                                                                                                   | 
    DestroySharedUserCritDeferredUnlockList(UserSessionState + 19832);                                                | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19888);                                              | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19872);                                              | 
  }                                                                                                                   | 
  PsGetCurrentThreadWin32Thread();                                                                                    | 
  if ( a5 > 0 )                                                                                                       | 
  {                                                                                                                   | 
    RtlCopyVolatileMemory(v31, a3, 0x100uLL);                                                                         | 
    v14 = 2LL * a5;                                                                                                   | 
    ProbeForWrite(Address, v14, 2u);                                                                                  | 
    if ( a5 >= 4 )                                                                                                    | 
    {                                                                                                                 | 
      v16 = Win32AllocPoolWithQuotaZInitImpl(v15, v14, 0x62757355u);                                                  | 
      Src = v16;                                                                                                      | 
      if ( !v16 )                                                                                                     | 
      {                                                                                                               | 
        ExRaiseStatus(-1073741801);                                                                                   | 
      }                                                                                                               | 
      v23 = 1;                                                                                                        | 
      if ( v26 != -1 )                                                                                                | 
      {                                                                                                               | 
        BugCheckParameter4 = PtiCurrent();                                                                            | 
        KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)&BugCheckParameter2, (ULONG_PTR)v16, (ULONG_PTR)BugCheckParameter4); | 
      }                                                                                                               | 
      v18 = (__int64 *)PsGetCurrentThreadWin32Thread();                                                               | 
      if ( v18 )                                                                                                      | 
      {                                                                                                               | 
        v19 = *v18;                                                                                                   | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        v19 = 0LL;                                                                                                    | 
      }                                                                                                               | 
      BugCheckParameter2 = *(_QWORD *)(v19 + 384);                                                                    | 
      *(_QWORD *)(v19 + 384) = &BugCheckParameter2;                                                                   | 
      v25 = v16;                                                                                                      | 
      v26 = (__int64)GreDeleteFastMutex;                                                                              | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      Src = &v30;                                                                                                     | 
    }                                                                                                                 | 
    v13 = xxxToUnicodeEx(a1, a5, a6, v29);                                                                            | 
    memmove((void *)Address, Src, v14);                                                                               | 
    if ( v23 )                                                                                                        | 
    {                                                                                                                 | 
      if ( v26 == -1 )                                                                                                | 
      {                                                                                                               | 
        v21 = PtiCurrent();                                                                                           | 
        KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)&BugCheckParameter2, 0LL, (ULONG_PTR)v21);                           | 
      }                                                                                                               | 
      v20 = (__int64 *)PsGetCurrentThreadWin32Thread();                                                               | 
      if ( v20 )                                                                                                      | 
      {                                                                                                               | 
        v9 = *v20;                                                                                                    | 
      }                                                                                                               | 
      *(_QWORD *)(v9 + 384) = BugCheckParameter2;                                                                     | 
      if ( v25 )                                                                                                      | 
      {                                                                                                               | 
        ((void (*)(void))v26)();                                                                                      | 
      }                                                                                                               | 
      v26 = -1LL;                                                                                                     | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v13 = 0;                                                                                                          | 
    UserSetLastError(87LL);                                                                                           | 
  }                                                                                                                   | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                                           | 
  Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(&BugCheckParameter2);                       | 
  return v13;                                                                                                         | 
}                                                                                                                     | 
109 row(s)
