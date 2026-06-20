__int64 __fastcall NtUserAllowSetForegroundWindow(unsigned int a1) | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  EnterCrit(0LL, 0LL);                                             | 
  v2 = PtiCurrent();                                               | 
  v3 = IAMThreadAccessGranted(v2);                                 | 
  v4 = _AllowSetForegroundWindow(a1, v3 != 0 ? 3 : 0) - 2;         | 
  if ( v4 )                                                        | 
  {                                                                | 
    if ( v4 != 1 )                                                 | 
    {                                                              | 
      v6 = 1LL;                                                    | 
      goto LABEL_4;                                                | 
    }                                                              | 
    v5 = 87LL;                                                     | 
  }                                                                | 
  else                                                             | 
  {                                                                | 
    v5 = 5LL;                                                      | 
  }                                                                | 
  UserSetLastError(v5);                                            | 
  v6 = 0LL;                                                        | 
LABEL_4:                                                           | 
  UserSessionSwitchLeaveCrit();                                    | 
  return v6;                                                       | 
}                                                                  | 
27 row(s)
