__int64 __fastcall NtUserSetTimer(__int64 a1, int a2, unsigned int a3, int a4, unsigned int a5) | 
{                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                 | 
                                                                                                | 
  EnterCrit(0LL, 0LL);                                                                          | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v15);                      | 
  v9 = 0LL;                                                                                     | 
  if ( a1 )                                                                                     | 
  {                                                                                             | 
    v10 = ValidateHwnd(a1);                                                                     | 
    if ( !v10 )                                                                                 | 
    {                                                                                           | 
      goto LABEL_17;                                                                            | 
    }                                                                                           | 
  }                                                                                             | 
  else                                                                                          | 
  {                                                                                             | 
    v10 = 0LL;                                                                                  | 
  }                                                                                             | 
  v11 = 10;                                                                                     | 
  if ( a3 >= 0xA )                                                                              | 
  {                                                                                             | 
    v11 = a3;                                                                                   | 
  }                                                                                             | 
  v12 = a5;                                                                                     | 
  if ( v11 > 0x7FFFFFFF )                                                                       | 
  {                                                                                             | 
    v11 = 0x7FFFFFFF;                                                                           | 
  }                                                                                             | 
  if ( a5 == 2147483637 )                                                                       | 
  {                                                                                             | 
    v12 = 0x7FFFFFFF - v11;                                                                     | 
  }                                                                                             | 
  else if ( a5 != -1 && (v11 + a5 < v11 || v11 + a5 > 0x7FFFFFFF) )                             | 
  {                                                                                             | 
    UserSetLastError(87LL);                                                                     | 
    goto LABEL_17;                                                                              | 
  }                                                                                             | 
  if ( !v10 )                                                                                   | 
  {                                                                                             | 
    goto LABEL_16;                                                                              | 
  }                                                                                             | 
  CurrentProcessWin32Process = (_QWORD *)PsGetCurrentProcessWin32Process(0x7FFFFFFFLL);         | 
  if ( CurrentProcessWin32Process && !*CurrentProcessWin32Process )                             | 
  {                                                                                             | 
    CurrentProcessWin32Process = 0LL;                                                           | 
  }                                                                                             | 
  if ( CurrentProcessWin32Process != *(_QWORD **)(*(_QWORD *)(v10 + 16) + 464LL) )              | 
  {                                                                                             | 
    UserSetLastError(5LL);                                                                      | 
  }                                                                                             | 
  else                                                                                          | 
  {                                                                                             | 
LABEL_16:                                                                                       | 
    v9 = InternalSetTimer(v10, a2, v11, a4, v12, 0);                                            | 
  }                                                                                             | 
LABEL_17:                                                                                       | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v15);                                    | 
  UserSessionSwitchLeaveCrit();                                                                 | 
  return v9;                                                                                    | 
}                                                                                               | 
61 row(s)
