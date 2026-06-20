__int64 __fastcall NtUserShowWindow(__int64 a1, unsigned int a2)                              | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  v4 = EnterCrit(0LL, 0LL);                                                                   | 
  v5 = ValidateHwnd(a1);                                                                      | 
  v6 = 0LL;                                                                                   | 
  v7 = v5;                                                                                    | 
  if ( v5 && (((*(_WORD *)(*(_QWORD *)(v5 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 ) | 
  {                                                                                           | 
    BugCheckParameter3[0] = *(_QWORD *)(v4 + 456);                                            | 
    *(_QWORD *)(v4 + 456) = BugCheckParameter3;                                               | 
    BugCheckParameter3[1] = v5;                                                               | 
    HMLockObject(v5);                                                                         | 
    if ( a2 > 0xB || *(char *)(*(_QWORD *)(v7 + 40) + 20LL) < 0 )                             | 
    {                                                                                         | 
      UserSetLastError(87LL);                                                                 | 
      v9 = 0;                                                                                 | 
      v11 = PtiCurrent();                                                                     | 
    }                                                                                         | 
    else                                                                                      | 
    {                                                                                         | 
      UserSessionState = W32GetUserSessionState();                                            | 
      v9 = xxxShowWindowEx(                                                                   | 
             v7,                                                                              | 
             a2 | *(_DWORD *)(UserSessionState + 67152) & 0x10000,                            | 
             a2 & 0x10000 | *(_DWORD *)(UserSessionState + 67152) & 0x10000);                 | 
      CurrentThreadNonPaged = (struct tagTHREADINFO **)W32GetCurrentThreadNonPaged();         | 
      if ( CurrentThreadNonPaged )                                                            | 
      {                                                                                       | 
        v6 = *CurrentThreadNonPaged;                                                          | 
      }                                                                                       | 
      v11 = v6;                                                                               | 
    }                                                                                         | 
    Win32HM_UnlockFromThread<0>((ULONG_PTR)v11, (ULONG_PTR)BugCheckParameter3);               | 
  }                                                                                           | 
  else                                                                                        | 
  {                                                                                           | 
    v9 = 0;                                                                                   | 
  }                                                                                           | 
  UserSessionSwitchLeaveCrit();                                                               | 
  return v9;                                                                                  | 
}                                                                                             | 
43 row(s)
