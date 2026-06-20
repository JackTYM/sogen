__int64 __fastcall NtUserGetWindowDC(__int64 a1)                | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v5, 1LL);         | 
  DCEx = 0LL;                                                   | 
  if ( !a1 )                                                    | 
  {                                                             | 
    v3 = 0LL;                                                   | 
    goto LABEL_3;                                               | 
  }                                                             | 
  v3 = ValidateHwnd(a1);                                        | 
  if ( v3 )                                                     | 
  {                                                             | 
LABEL_3:                                                        | 
    DCEx = _GetDCEx(v3, 0LL, 65537LL);                          | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return DCEx;                                                  | 
}                                                               | 
20 row(s)
