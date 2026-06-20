__int64 __fastcall NtUserGetSystemMenu(__int64 a1)                                                    | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v2 = EnterCrit(0LL, 1LL);                                                                           | 
  v3 = ValidateHwnd(a1);                                                                              | 
  v4 = 0LL;                                                                                           | 
  v5 = (struct tagWND *)v3;                                                                           | 
  if ( v3 )                                                                                           | 
  {                                                                                                   | 
    Win32HM_LockIntoThread<0>(v2, v3, BugCheckParameter3);                                            | 
    SystemMenu = (__int64 *)xxxGetSystemMenu(v5);                                                     | 
    if ( SystemMenu )                                                                                 | 
    {                                                                                                 | 
      v4 = *SystemMenu;                                                                               | 
    }                                                                                                 | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v4;                                                                                          | 
}                                                                                                     | 
21 row(s)
