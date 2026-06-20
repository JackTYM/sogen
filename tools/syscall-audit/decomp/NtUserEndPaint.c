__int64 __fastcall NtUserEndPaint(__int64 a1, ULONG64 a2)                                             | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  memset_0(v10, 0, 0x48uLL);                                                                          | 
  v4 = EnterCrit(0LL, 0LL);                                                                           | 
  v5 = ValidateReceivingHwnd(a1, 1LL);                                                                | 
  v6 = (struct tagWND *)v5;                                                                           | 
  v7 = 0;                                                                                             | 
  if ( v5 )                                                                                           | 
  {                                                                                                   | 
    Win32HMThreadLockAlways<tagHOOK>::Win32HMThreadLockAlways<tagHOOK>(BugCheckParameter3, v4, v5);   | 
    if ( a2 >= MmUserProbeAddress )                                                                   | 
    {                                                                                                 | 
      a2 = MmUserProbeAddress;                                                                        | 
    }                                                                                                 | 
    v10[0] = *(_OWORD *)a2;                                                                           | 
    v10[1] = *(_OWORD *)(a2 + 16);                                                                    | 
    v10[2] = *(_OWORD *)(a2 + 32);                                                                    | 
    v10[3] = *(_OWORD *)(a2 + 48);                                                                    | 
    v11 = *(_QWORD *)(a2 + 64);                                                                       | 
    v7 = xxxEndPaint(v6);                                                                             | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v7;                                                                                          | 
}                                                                                                     | 
27 row(s)
