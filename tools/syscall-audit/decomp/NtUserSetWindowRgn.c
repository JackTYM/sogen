__int64 __fastcall NtUserSetWindowRgn(__int64 a1, __int64 a2, unsigned int a3)                | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  v6 = EnterCrit(0LL, 0LL);                                                                   | 
  v7 = ValidateHwnd(a1);                                                                      | 
  v8 = 0;                                                                                     | 
  v9 = v7;                                                                                    | 
  if ( v7 && (((*(_WORD *)(*(_QWORD *)(v7 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 ) | 
  {                                                                                           | 
    Win32HM_LockIntoThread<0>(v6, v7, BugCheckParameter3);                                    | 
    v8 = xxxSetWindowRgn(v9, a2, a3);                                                         | 
    v10 = PtiCurrent();                                                                       | 
    Win32HM_UnlockFromThread<0>((ULONG_PTR)v10, (ULONG_PTR)BugCheckParameter3);               | 
  }                                                                                           | 
  UserSessionSwitchLeaveCrit();                                                               | 
  return v8;                                                                                  | 
}                                                                                             | 
18 row(s)
