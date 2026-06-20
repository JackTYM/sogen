__int64 __fastcall NtUserModifyUserStartupInfoFlags(int a1, int a2)                         | 
{                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                             | 
                                                                                            | 
  EnterCrit(0LL, 0LL);                                                                      | 
  v4 = ((__int64 (*)(void))PsGetCurrentProcessWin32Process)();                              | 
  v8 = v4;                                                                                  | 
  if ( v4 )                                                                                 | 
  {                                                                                         | 
    v7 = -*(_QWORD *)v4;                                                                    | 
    v8 = -(__int64)(*(_QWORD *)v4 != 0LL) & v4;                                             | 
  }                                                                                         | 
  v9 = a2 & a1 | ~a1 & *(_DWORD *)(v8 + 788);                                               | 
  CurrentProcessWin32Process = PsGetCurrentProcessWin32Process(v6, v5, v7);                 | 
  if ( CurrentProcessWin32Process )                                                         | 
  {                                                                                         | 
    v11 = -*(_QWORD *)CurrentProcessWin32Process;                                           | 
    CurrentProcessWin32Process &= -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL); | 
  }                                                                                         | 
  *(_DWORD *)(CurrentProcessWin32Process + 788) = v9;                                       | 
  UserSessionSwitchLeaveCrit(v11);                                                          | 
  return 1LL;                                                                               | 
}                                                                                           | 
23 row(s)
