__int64 __fastcall NtUserInitializeClientPfnArrays(__int64 a1, __int64 a2, __int64 a3, __int64 a4) | 
{                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                    | 
                                                                                                   | 
  EnterCrit(0LL, 0LL);                                                                             | 
  if ( a1 && (a1 & 3) != 0 || a2 && (a2 & 3) != 0 || a3 && (a3 & 3) != 0 )                         | 
  {                                                                                                | 
    ExRaiseDatatypeMisalignment();                                                                 | 
  }                                                                                                | 
  v8 = InitializeClientPfnArrays(a1, a2, a3, a4);                                                  | 
  UserSessionSwitchLeaveCrit(v9);                                                                  | 
  return v8;                                                                                       | 
}                                                                                                  | 
13 row(s)
