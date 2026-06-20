__int64 __fastcall NtUserSetProp(__int64 a1, int a2, __int64 a3)                 | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  v6 = 0;                                                                        | 
  EnterSharedCrit(0LL, 0LL);                                                     | 
  v9[0] = 0;                                                                     | 
  v10 = 0LL;                                                                     | 
  AtomicExecutionCheck::Arm((AtomicExecutionCheck *)v9);                         | 
  v7 = ValidatePropWindow(a1, 3LL);                                              | 
  if ( v7 )                                                                      | 
  {                                                                              | 
    v6 = InternalSetProp(v7, (unsigned __int16)a2, a3, HIWORD(a2) != 0 ? 2 : 0); | 
  }                                                                              | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v9);                      | 
  UserSessionSwitchLeaveCrit();                                                  | 
  return v6;                                                                     | 
}                                                                                | 
18 row(s)
