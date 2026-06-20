__int64 NtUserGetForegroundWindow()                                                                                                      | 
{                                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                          | 
                                                                                                                                         | 
  EnterSharedCrit(0LL, 1LL);                                                                                                             | 
  v0 = 0LL;                                                                                                                              | 
  if ( *(_QWORD *)(W32GetUserSessionState() + 19024) )                                                                                   | 
  {                                                                                                                                      | 
    if ( *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 19024) + 128LL) )                                                            | 
    {                                                                                                                                    | 
      v1 = PtiCurrent();                                                                                                                 | 
      if ( *((_QWORD *)v1 + 62) == *(_QWORD *)(*(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 19024) + 128LL) + 24LL) )              | 
      {                                                                                                                                  | 
        UserSessionState = W32GetUserSessionState();                                                                                     | 
        v0 = *(_QWORD *)CoreWindowProp::GetCompositeAppFrameWindowOrSelf(*(const struct tagWND **)(*(_QWORD *)(UserSessionState + 19024) | 
                                                                                                 + 128LL));                              | 
      }                                                                                                                                  | 
    }                                                                                                                                    | 
  }                                                                                                                                      | 
  UserSessionSwitchLeaveCrit();                                                                                                          | 
  return v0;                                                                                                                             | 
}                                                                                                                                        | 
22 row(s)
