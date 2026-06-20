__int64 __fastcall NtUserShowCursor(int a1)                     | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v2 = zzzShowCursor(a1 != 0);                                  | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v2;                                                    | 
}                                                               | 
9 row(s)
