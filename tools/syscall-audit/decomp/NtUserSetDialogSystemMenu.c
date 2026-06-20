__int64 __fastcall NtUserSetDialogSystemMenu(__int64 a1)                                              | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v2 = EnterCrit(0LL, 0LL);                                                                           | 
  v3 = ValidateHwnd(a1);                                                                              | 
  v4 = 0;                                                                                             | 
  v5 = v3;                                                                                            | 
  if ( v3 )                                                                                           | 
  {                                                                                                   | 
    Win32HM_LockIntoThread<0>(v2, v3, BugCheckParameter3);                                            | 
    v4 = xxxSetDialogSystemMenu(v5);                                                                  | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v4;                                                                                          | 
}                                                                                                     | 
17 row(s)
