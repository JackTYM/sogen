__int64 NtUserReleaseCapture()                                  | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v0 = (int)xxxReleaseCapture();                                | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v0;                                                    | 
}                                                               | 
9 row(s)
