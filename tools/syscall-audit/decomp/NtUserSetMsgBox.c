__int64 __fastcall NtUserSetMsgBox(__int64 a1)                                                        | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v2 = EnterCrit(0LL, 0LL);                                                                           | 
  v3 = ValidateReceivingHwnd(a1, 1LL);                                                                | 
  v4 = 0LL;                                                                                           | 
  v5 = v3;                                                                                            | 
  if ( v3 )                                                                                           | 
  {                                                                                                   | 
    Win32HM_LockIntoThread<0>(v2, v3, BugCheckParameter3);                                            | 
    v7 = **(_QWORD **)(*(_QWORD *)(v5 + 24) + 8LL);                                                   | 
    ++*(_DWORD *)(v7 + 48);                                                                           | 
    SetOrClrWF(1LL, v5);                                                                              | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
    v4 = 1LL;                                                                                         | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v4;                                                                                          | 
}                                                                                                     | 
20 row(s)
