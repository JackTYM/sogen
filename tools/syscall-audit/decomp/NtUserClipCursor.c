__int64 __fastcall NtUserClipCursor(struct tagRECT *a1)                                          | 
{                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                  | 
                                                                                                 | 
  v1 = a1;                                                                                       | 
  *(_QWORD *)&v9.left = a1;                                                                      | 
  UserSessionState = W32GetUserSessionState();                                                   | 
  v3 = UserCritInternal::_anonymous_namespace_::EnterCritInternal(                               | 
         UserSessionState,                                                                       | 
         0LL,                                                                                    | 
         0LL,                                                                                    | 
         _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_);                     | 
  *(_QWORD *)(UserSessionState + 16) = v3;                                                       | 
  v4 = 0;                                                                                        | 
  if ( v3 )                                                                                      | 
  {                                                                                              | 
    DestroySharedUserCritDeferredUnlockList(UserSessionState + 19832);                           | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19888);                         | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19872);                         | 
  }                                                                                              | 
  v10 = 0LL;                                                                                     | 
  if ( (unsigned int)ApiSetCheckCursorClipAccess(0LL, v1 == 0LL) )                               | 
  {                                                                                              | 
    if ( v1 )                                                                                    | 
    {                                                                                            | 
      if ( (unsigned __int64)v1 >= MmUserProbeAddress )                                          | 
      {                                                                                          | 
        v1 = (struct tagRECT *)MmUserProbeAddress;                                               | 
      }                                                                                          | 
      v10 = *v1;                                                                                 | 
      v9.left = (v10.right + v10.left) / 2;                                                      | 
      v9.top = (v10.bottom + v10.top) / 2;                                                       | 
      CurrentThreadDpiAwarenessContext = (unsigned int)W32GetCurrentThreadDpiAwarenessContext(); | 
      *(_QWORD *)&v9.left = GuessMonitorOverrideForCoordinateConversions(                        | 
                              *(_QWORD *)&v9.left,                                               | 
                              CurrentThreadDpiAwarenessContext,                                  | 
                              0LL);                                                              | 
      LogicalToPhysicalDPIRect(&v10, &v10, (unsigned int)CurrentThreadDpiAwarenessContext, &v9); | 
      v6 = *(CCursorClip **)(W32GetUserSessionState() + 36384);                                  | 
      v9 = v10;                                                                                  | 
      CCursorClip::SetClip(v6, &v9);                                                             | 
    }                                                                                            | 
    else                                                                                         | 
    {                                                                                            | 
      v7 = W32GetUserSessionState();                                                             | 
      CCursorClip::ClearClip(*(CCursorClip **)(v7 + 36384));                                     | 
    }                                                                                            | 
    v4 = 1;                                                                                      | 
  }                                                                                              | 
  else                                                                                           | 
  {                                                                                              | 
    UserSetLastError(5LL);                                                                       | 
  }                                                                                              | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                      | 
  return v4;                                                                                     | 
}                                                                                                | 
56 row(s)
