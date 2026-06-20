__int64 __fastcall NtUserSetWindowState(__int64 a1, unsigned int a2)                                  | 
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
    v6 = SetWindowState(v7, a2);                                                                      | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v6;                                                                                          | 
}                                                                                                     | 
17 row(s)
