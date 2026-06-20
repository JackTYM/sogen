__int64 __fastcall NtUserDrawMenuBar(__int64 a1)                                                      | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v2 = EnterCrit(0LL, 0LL);                                                                           | 
  v3 = ValidateHwnd(a1);                                                                              | 
  v4 = 0LL;                                                                                           | 
  v5 = (struct tagWND *)v3;                                                                           | 
  if ( v3 && (((*(_WORD *)(*(_QWORD *)(v3 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 )         | 
  {                                                                                                   | 
    Win32HM_LockIntoThread<0>(v2, v3, BugCheckParameter3);                                            | 
    if ( (*(_BYTE *)(*((_QWORD *)v5 + 5) + 31LL) & 0xC0) != 0x40 )                                    | 
    {                                                                                                 | 
      xxxSetWindowPos(v5, 0, 0, 55);                                                                  | 
    }                                                                                                 | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
    v4 = 1LL;                                                                                         | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v4;                                                                                          | 
}                                                                                                     | 
21 row(s)
