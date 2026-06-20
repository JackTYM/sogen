__int64 __fastcall NtUserKillTimer(__int64 a1, int a2)                    | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  EnterCrit(0LL, 0LL);                                                    | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v7); | 
  Timer = 0;                                                              | 
  if ( !a1 )                                                              | 
  {                                                                       | 
    LODWORD(v5) = 0;                                                      | 
    goto LABEL_3;                                                         | 
  }                                                                       | 
  v5 = ValidateHwnd(a1);                                                  | 
  if ( v5 )                                                               | 
  {                                                                       | 
LABEL_3:                                                                  | 
    Timer = FindTimer(v5, a2, 0, 1, 0LL);                                 | 
  }                                                                       | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v7);               | 
  UserSessionSwitchLeaveCrit();                                           | 
  return Timer;                                                           | 
}                                                                         | 
22 row(s)
