__int64 __fastcall NtUserSetWindowFNID(__int64 a1, __int16 a2)                                    | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  EnterCrit(0LL, 0LL);                                                                            | 
  v4 = ValidateHwnd(a1);                                                                          | 
  v5 = 0LL;                                                                                       | 
  v6 = v4;                                                                                        | 
  if ( v4 )                                                                                       | 
  {                                                                                               | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                               | 
    v8 = CurrentProcessWin32Process;                                                              | 
    if ( CurrentProcessWin32Process )                                                             | 
    {                                                                                             | 
      v8 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process; | 
    }                                                                                             | 
    if ( *(_QWORD *)(*(_QWORD *)(v6 + 16) + 464LL) != v8 )                                        | 
    {                                                                                             | 
      v9 = 5LL;                                                                                   | 
LABEL_6:                                                                                          | 
      UserSetLastError(v9);                                                                       | 
      goto LABEL_19;                                                                              | 
    }                                                                                             | 
    if ( a2 != 0x4000 )                                                                           | 
    {                                                                                             | 
      if ( (unsigned int)IsWindowBeingDestroyed(v6) )                                             | 
      {                                                                                           | 
LABEL_9:                                                                                          | 
        v9 = 87LL;                                                                                | 
        goto LABEL_6;                                                                             | 
      }                                                                                           | 
      if ( (unsigned int)PsGetWin32KFilterSet() == 5 )                                            | 
      {                                                                                           | 
        if ( a2 != 681 )                                                                          | 
        {                                                                                         | 
          goto LABEL_9;                                                                           | 
        }                                                                                         | 
      }                                                                                           | 
      else if ( a2 != 666 && (unsigned __int16)(a2 - 673) > 9u )                                  | 
      {                                                                                           | 
        goto LABEL_9;                                                                             | 
      }                                                                                           | 
      v10 = *(_QWORD *)(v6 + 40);                                                                 | 
      if ( (*(_WORD *)(v10 + 42) & 0x2FFF) != 0 )                                                 | 
      {                                                                                           | 
        goto LABEL_9;                                                                             | 
      }                                                                                           | 
      if ( *(_DWORD *)(v10 + 248) )                                                               | 
      {                                                                                           | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTellMeIf", 0x20000LL, 8477LL);                | 
        goto LABEL_9;                                                                             | 
      }                                                                                           | 
    }                                                                                             | 
    v5 = 1LL;                                                                                     | 
    *(_WORD *)(*(_QWORD *)(v6 + 40) + 42LL) |= a2;                                                | 
  }                                                                                               | 
LABEL_19:                                                                                         | 
  UserSessionSwitchLeaveCrit();                                                                   | 
  return v5;                                                                                      | 
}                                                                                                 | 
60 row(s)
