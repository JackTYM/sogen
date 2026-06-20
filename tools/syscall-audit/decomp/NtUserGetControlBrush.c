__int64 __fastcall NtUserGetControlBrush(__int64 a1, __int64 a2, unsigned int a3) | 
{                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                   | 
                                                                                  | 
  v6 = EnterCrit(0LL, 0LL);                                                       | 
  v7 = ValidateReceivingHwnd(a1, 1LL);                                            | 
  ControlBrush = 0LL;                                                             | 
  v9 = v7;                                                                        | 
  if ( v7 )                                                                       | 
  {                                                                               | 
    Win32HM_LockIntoThread<0>(v6, v7, BugCheckParameter3);                        | 
    if ( a2 && a3 - 306 <= 6 )                                                    | 
    {                                                                             | 
      ControlBrush = xxxGetControlBrush(v9, a2, a3);                              | 
    }                                                                             | 
    v10 = PtiCurrent();                                                           | 
    Win32HM_UnlockFromThread<0>((ULONG_PTR)v10, (ULONG_PTR)BugCheckParameter3);   | 
  }                                                                               | 
  UserSessionSwitchLeaveCrit();                                                   | 
  return ControlBrush;                                                            | 
}                                                                                 | 
21 row(s)
