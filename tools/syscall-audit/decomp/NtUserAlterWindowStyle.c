__int64 __fastcall NtUserAlterWindowStyle(__int64 a1, int a2, int a3)                                                              | 
{                                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                    | 
                                                                                                                                   | 
  EnterCrit(0LL, 0LL);                                                                                                             | 
  v6 = ValidateReceivingHwnd(a1, 1LL);                                                                                             | 
  v7 = 0LL;                                                                                                                        | 
  v8 = v6;                                                                                                                         | 
  if ( v6 )                                                                                                                        | 
  {                                                                                                                                | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                                                                | 
    v10 = CurrentProcessWin32Process;                                                                                              | 
    if ( CurrentProcessWin32Process )                                                                                              | 
    {                                                                                                                              | 
      v10 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process;                                 | 
    }                                                                                                                              | 
    if ( *(_QWORD *)(*(_QWORD *)(v8 + 16) + 464LL) == v10 )                                                                        | 
    {                                                                                                                              | 
      *(_DWORD *)(*(_QWORD *)(v8 + 40) + 28LL) = a3 & a2 & 0x30023F | *(_DWORD *)(*(_QWORD *)(v8 + 40) + 28LL) & ~(a2 & 0x30023F); | 
    }                                                                                                                              | 
    v7 = 1LL;                                                                                                                      | 
  }                                                                                                                                | 
  UserSessionSwitchLeaveCrit();                                                                                                    | 
  return v7;                                                                                                                       | 
}                                                                                                                                  | 
25 row(s)
