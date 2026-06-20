__int64 __fastcall NtUserSetDialogPointer(__int64 a1, __int64 a2)                                     | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v4 = EnterCrit(0LL, 0LL);                                                                           | 
  v5 = ValidateHwnd(a1);                                                                              | 
  v6 = 0;                                                                                             | 
  v7 = v5;                                                                                            | 
  if ( v5 )                                                                                           | 
  {                                                                                                   | 
    Win32HM_LockIntoThread<0>(v4, v5, BugCheckParameter3);                                            | 
    v6 = SetDialogPointer(v7, a2);                                                                    | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v6;                                                                                          | 
}                                                                                                     | 
17 row(s)
