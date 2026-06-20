__int64 __fastcall NtUserCreateCaret(__int64 a1, __int64 a2, unsigned int a3, unsigned int a4) | 
{                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                | 
                                                                                               | 
  v8 = EnterCrit(0LL, 0LL);                                                                    | 
  v9 = ValidateReceivingHwnd(a1, 1LL);                                                         | 
  Caret = 0;                                                                                   | 
  v11 = v9;                                                                                    | 
  if ( v9 )                                                                                    | 
  {                                                                                            | 
    Win32HM_LockIntoThread<0>(v8, v9, BugCheckParameter3);                                     | 
    Caret = xxxCreateCaret(v11, a2, a3, a4);                                                   | 
    v12 = PtiCurrent();                                                                        | 
    Win32HM_UnlockFromThread<0>((ULONG_PTR)v12, (ULONG_PTR)BugCheckParameter3);                | 
  }                                                                                            | 
  UserSessionSwitchLeaveCrit();                                                                | 
  return Caret;                                                                                | 
}                                                                                              | 
18 row(s)
