__int64 __fastcall NtUserScheduleDispatchNotification(__int64 a1)                                 | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  v2 = EnterCrit(0LL, 0LL);                                                                       | 
  v3 = ValidateReceivingHwnd(a1, 0LL);                                                            | 
  v4 = 0LL;                                                                                       | 
  v5 = (struct tagWND *)v3;                                                                       | 
  if ( v3 )                                                                                       | 
  {                                                                                               | 
    BugCheckParameter3[0] = *(_QWORD *)(v2 + 456);                                                | 
    *(_QWORD *)(v2 + 456) = BugCheckParameter3;                                                   | 
    BugCheckParameter3[1] = v3;                                                                   | 
    HMLockObject(v3);                                                                             | 
    v6 = *((_QWORD *)v5 + 2);                                                                     | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                               | 
    v9 = CurrentProcessWin32Process;                                                              | 
    if ( CurrentProcessWin32Process )                                                             | 
    {                                                                                             | 
      v8 = -*(_QWORD *)CurrentProcessWin32Process;                                                | 
      v9 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process; | 
    }                                                                                             | 
    if ( *(_QWORD *)(v6 + 464) == v9 )                                                            | 
    {                                                                                             | 
      CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged(v8);                         | 
      if ( CurrentThreadNonPaged )                                                                | 
      {                                                                                           | 
        v12 = *CurrentThreadNonPaged;                                                             | 
      }                                                                                           | 
      else                                                                                        | 
      {                                                                                           | 
        v12 = 0LL;                                                                                | 
      }                                                                                           | 
      if ( v6 == v12                                                                              | 
        || UIPrivilegeIsolation::CheckAccess(                                                     | 
             (UIPrivilegeIsolation *)(v9 + 856),                                                  | 
             (const struct tagUIPI_INFO *)(*(_QWORD *)(v6 + 472) + 544LL),                        | 
             v11) )                                                                               | 
      {                                                                                           | 
        v14 = (unsigned int)PostEventMessageEx(                                                   | 
                              (struct tagTHREADINFO *)v6,                                         | 
                              *(struct tagQ **)(v6 + 472),                                        | 
                              9u,                                                                 | 
                              v5,                                                                 | 
                              0x60u,                                                              | 
                              0LL,                                                                | 
                              0LL,                                                                | 
                              0LL) != 0                                                           | 
            ? 2                                                                                   | 
            : 0;                                                                                  | 
      }                                                                                           | 
      else                                                                                        | 
      {                                                                                           | 
        v14 = 1;                                                                                  | 
      }                                                                                           | 
    }                                                                                             | 
    else                                                                                          | 
    {                                                                                             | 
      UserSetLastError(5LL);                                                                      | 
      v14 = 0;                                                                                    | 
    }                                                                                             | 
    v15 = (ULONG_PTR *)W32GetCurrentThreadNonPaged(v13);                                          | 
    if ( v15 )                                                                                    | 
    {                                                                                             | 
      v4 = *v15;                                                                                  | 
    }                                                                                             | 
    Win32HM_UnlockFromThread<0>(v4, (ULONG_PTR)BugCheckParameter3);                               | 
  }                                                                                               | 
  else                                                                                            | 
  {                                                                                               | 
    v14 = 0;                                                                                      | 
  }                                                                                               | 
  UserSessionSwitchLeaveCrit();                                                                   | 
  return v14;                                                                                     | 
}                                                                                                 | 
75 row(s)
