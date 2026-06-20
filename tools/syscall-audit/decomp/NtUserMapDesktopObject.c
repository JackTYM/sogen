__int64 __fastcall NtUserMapDesktopObject(__int64 a1)           | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v2 = MapDesktopObject(a1);                                    | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v2;                                                    | 
}                                                               | 
9 row(s)
