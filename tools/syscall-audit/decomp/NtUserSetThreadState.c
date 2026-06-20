__int64 __fastcall NtUserSetThreadState(int a1, int a2)         | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  if ( (a1 & 0xFFFFBFFF) != 0 )                                 | 
  {                                                             | 
    UserSetLastError(3221225485LL);                             | 
    v6 = 0LL;                                                   | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    v4 = *((_QWORD *)PtiCurrent() + 59);                        | 
    v5 = a1 ^ *(_DWORD *)(v4 + 508);                            | 
    v6 = 1LL;                                                   | 
    *(_DWORD *)(v4 + 508) ^= a2 & v5;                           | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v6;                                                    | 
}                                                               | 
20 row(s)
