__int64 NtUserDisableProcessWindowsGhosting()                   | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v0 = (int)_DisableProcessWindowsGhosting();                   | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v0;                                                    | 
}                                                               | 
9 row(s)
