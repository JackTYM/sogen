__int64 __fastcall NtUserSetProcessDpiAwarenessContext(unsigned int a1, int a2)                                    | 
{                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                    | 
                                                                                                                   | 
  UserSessionState = W32GetUserSessionState();                                                                     | 
  v5 = UserCritInternal::_anonymous_namespace_::EnterCritInternal(                                                 | 
         UserSessionState,                                                                                         | 
         0LL,                                                                                                      | 
         0LL,                                                                                                      | 
         _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_);                                       | 
  *(_QWORD *)(UserSessionState + 16) = v5;                                                                         | 
  if ( v5 )                                                                                                        | 
  {                                                                                                                | 
    DestroySharedUserCritDeferredUnlockList(UserSessionState + 19832);                                             | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19888);                                           | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19872);                                           | 
  }                                                                                                                | 
  if ( !(unsigned int)IsValidKernelDpiAwarenessContext(a1) )                                                       | 
  {                                                                                                                | 
    goto LABEL_18;                                                                                                 | 
  }                                                                                                                | 
  if ( (a1 & 0xF) == 1 )                                                                                           | 
  {                                                                                                                | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                                                | 
    v7 = CurrentProcessWin32Process;                                                                               | 
    if ( CurrentProcessWin32Process )                                                                              | 
    {                                                                                                              | 
      v7 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process;                  | 
    }                                                                                                              | 
    if ( ((a1 >> 8) & 0x1FF) != *(_WORD *)(v7 + 272) )                                                             | 
    {                                                                                                              | 
LABEL_18:                                                                                                          | 
      UserSetLastError(87LL);                                                                                      | 
LABEL_19:                                                                                                          | 
      v10 = 0LL;                                                                                                   | 
      goto LABEL_20;                                                                                               | 
    }                                                                                                              | 
  }                                                                                                                | 
  v8 = *((_QWORD *)PtiCurrent() + 58);                                                                             | 
  v9 = *(_QWORD *)(v8 + 800);                                                                                      | 
  if ( (v9 & 0x400000) != 0 && (!a2 || (a1 & 0xF) == 0 || *(_DWORD *)(v8 + 268) != 18) )                           | 
  {                                                                                                                | 
    goto LABEL_19;                                                                                                 | 
  }                                                                                                                | 
  v10 = 1LL;                                                                                                       | 
  *(_QWORD *)(v8 + 800) = v9 | 0x400000;                                                                           | 
  if ( *(_DWORD *)(v8 + 268) != a1 )                                                                               | 
  {                                                                                                                | 
    *(_DWORD *)(v8 + 268) = a1;                                                                                    | 
    v11 = *(_QWORD *)(v8 + 656);                                                                                   | 
    if ( v11 )                                                                                                     | 
    {                                                                                                              | 
      for ( i = *(_QWORD *)(v11 + 16); i; i = *(_QWORD *)(i + 32) )                                                | 
      {                                                                                                            | 
        UpdateWindowTreeDpiAwareness((struct tagPROCESSINFO *)v8, *(struct tagWND **)(*(_QWORD *)(i + 8) + 24LL)); | 
        UpdateWindowTreeDpiAwareness((struct tagPROCESSINFO *)v8, *(struct tagWND **)(i + 112));                   | 
      }                                                                                                            | 
    }                                                                                                              | 
  }                                                                                                                | 
LABEL_20:                                                                                                          | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                                        | 
  return v10;                                                                                                      | 
}                                                                                                                  | 
63 row(s)
