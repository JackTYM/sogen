__int64 NtUserRemoteConnectState()                              | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v0 = RemoteConnectState();                                    | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v0;                                                    | 
}                                                               | 
9 row(s)
