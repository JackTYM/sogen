__int64 __fastcall NtUserSetActiveWindow(__int64 a1)                                            | 
{                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                 | 
                                                                                                | 
  EnterCrit(0LL, 0LL);                                                                          | 
  v2 = 0LL;                                                                                     | 
  if ( a1 )                                                                                     | 
  {                                                                                             | 
    v3 = (struct tagWND *)ValidateHwnd(a1);                                                     | 
    if ( !v3 )                                                                                  | 
    {                                                                                           | 
      goto LABEL_6;                                                                             | 
    }                                                                                           | 
  }                                                                                             | 
  else                                                                                          | 
  {                                                                                             | 
    v3 = 0LL;                                                                                   | 
  }                                                                                             | 
  Win32HMThreadLockBase<tagWND,1,0>::Win32HMThreadLockBase<tagWND,1,0>(BugCheckParameter3, v3); | 
  v4 = xxxSetActiveWindow(v3);                                                                  | 
  if ( v4 )                                                                                     | 
  {                                                                                             | 
    v2 = *(_QWORD *)v4;                                                                         | 
  }                                                                                             | 
  v5 = PtiCurrent();                                                                            | 
  Win32HM_UnlockFromThread<1>((ULONG_PTR)v5, (ULONG_PTR)BugCheckParameter3);                    | 
LABEL_6:                                                                                        | 
  UserSessionSwitchLeaveCrit();                                                                 | 
  return v2;                                                                                    | 
}                                                                                               | 
30 row(s)
