_BOOL8 __fastcall NtUserDestroyCursor(__int64 a1, unsigned int a2)                                | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  EnterCrit(0LL, 0LL);                                                                            | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v11);                        | 
  LOBYTE(v4) = 3;                                                                                 | 
  v5 = 0LL;                                                                                       | 
  v6 = HMValidateHandleWithDescriptor(a1, v4);                                                    | 
  if ( v6 )                                                                                       | 
  {                                                                                               | 
    if ( a2 )                                                                                     | 
    {                                                                                             | 
      if ( a2 != 1 )                                                                              | 
      {                                                                                           | 
        v8 = 87LL;                                                                                | 
        goto LABEL_8;                                                                             | 
      }                                                                                           | 
    }                                                                                             | 
    else                                                                                          | 
    {                                                                                             | 
      v9 = *(_QWORD *)(W32GetUserGdiSessionState() + 40);                                         | 
      if ( PsGetCurrentProcess() != v9 )                                                          | 
      {                                                                                           | 
        CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                           | 
        if ( CurrentProcessWin32Process )                                                         | 
        {                                                                                         | 
          CurrentProcessWin32Process &= -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL); | 
        }                                                                                         | 
        if ( *(_QWORD *)(v6 + 24) != CurrentProcessWin32Process )                                 | 
        {                                                                                         | 
LABEL_7:                                                                                          | 
          v8 = 5LL;                                                                               | 
LABEL_8:                                                                                          | 
          UserSetLastError(v8);                                                                   | 
          goto LABEL_6;                                                                           | 
        }                                                                                         | 
      }                                                                                           | 
    }                                                                                             | 
    if ( *(_QWORD *)(v6 + 48) == v6 )                                                             | 
    {                                                                                             | 
      v5 = _DestroyCursor((struct tagCURSOR *)v6, a2);                                            | 
      goto LABEL_6;                                                                               | 
    }                                                                                             | 
    goto LABEL_7;                                                                                 | 
  }                                                                                               | 
LABEL_6:                                                                                          | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v11);                                      | 
  UserSessionSwitchLeaveCrit();                                                                   | 
  return v5;                                                                                      | 
}                                                                                                 | 
51 row(s)
