__int64 __fastcall NtUserPostQuitMessage(unsigned int a1)       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v2 = (int)PostQuitMessage(a1);                                | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v2;                                                    | 
}                                                               | 
9 row(s)
