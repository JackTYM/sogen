__int64 __fastcall NtUserSetWindowsHookEx(__int64 a1, ULONG64 a2, unsigned int a3, unsigned int a4, __int64 a5, int a6) | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v30 = a1;                                                                                                             | 
  v8 = a1;                                                                                                              | 
  if ( a4 + 1 > 0xF )                                                                                                   | 
  {                                                                                                                     | 
    if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                                   | 
      || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x10) == 0                                               | 
      || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 2u )                                                       | 
    {                                                                                                                   | 
      v10 = 0LL;                                                                                                        | 
      v23 = 0;                                                                                                          | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v23 = 1;                                                                                                          | 
      v10 = 0LL;                                                                                                        | 
    }                                                                                                                   | 
    v24 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                                    | 
    if ( v23 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                              | 
    {                                                                                                                   | 
      UserSessionState = W32GetUserSessionState();                                                                      | 
      LOBYTE(v26) = v24;                                                                                                | 
      LOBYTE(v27) = v23;                                                                                                | 
      WPP_RECORDER_AND_TRACE_SF_D(                                                                                      | 
        *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                             | 
        v27,                                                                                                            | 
        v26,                                                                                                            | 
        *(_QWORD *)(UserSessionState + 69512),                                                                          | 
        2,                                                                                                              | 
        5,                                                                                                              | 
        13,                                                                                                             | 
        (__int64)&WPP_ee910b509ed73fb465c6a84e13eeb380_Traceguids,                                                      | 
        a4);                                                                                                            | 
    }                                                                                                                   | 
    v22 = 1426LL;                                                                                                       | 
    goto LABEL_49;                                                                                                      | 
  }                                                                                                                     | 
  v9 = 1;                                                                                                               | 
  if ( a4 <= 1 )                                                                                                        | 
  {                                                                                                                     | 
    if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                                   | 
      || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x10) == 0                                               | 
      || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 3u )                                                       | 
    {                                                                                                                   | 
      v10 = 0LL;                                                                                                        | 
      v9 = 0;                                                                                                           | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v10 = 0LL;                                                                                                        | 
    }                                                                                                                   | 
    v17 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                                    | 
    if ( v9 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                               | 
    {                                                                                                                   | 
      v18 = W32GetUserSessionState();                                                                                   | 
      LOBYTE(v19) = v17;                                                                                                | 
      LOBYTE(v20) = v9;                                                                                                 | 
      WPP_RECORDER_AND_TRACE_SF_(                                                                                       | 
        *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                             | 
        v20,                                                                                                            | 
        v19,                                                                                                            | 
        *(_QWORD *)(v18 + 69512),                                                                                       | 
        3,                                                                                                              | 
        5,                                                                                                              | 
        14,                                                                                                             | 
        (__int64)&WPP_ee910b509ed73fb465c6a84e13eeb380_Traceguids);                                                     | 
    }                                                                                                                   | 
    v21 = PtiCurrent();                                                                                                 | 
    *(_QWORD *)(*((_QWORD *)v21 + 65) + 224LL) |= 0x1000uLL;                                                            | 
    v22 = 5LL;                                                                                                          | 
LABEL_49:                                                                                                               | 
    UserSetLastError(v22);                                                                                              | 
    return v10;                                                                                                         | 
  }                                                                                                                     | 
  EnterLeaveCritShared::EnterLeaveCritShared(Address, 1LL);                                                             | 
  v10 = 0LL;                                                                                                            | 
  if ( !a3 )                                                                                                            | 
  {                                                                                                                     | 
    v11 = 0LL;                                                                                                          | 
    goto LABEL_8;                                                                                                       | 
  }                                                                                                                     | 
  v11 = PtiFromThreadId(a3);                                                                                            | 
  if ( v11 )                                                                                                            | 
  {                                                                                                                     | 
LABEL_8:                                                                                                                | 
    if ( !v8 )                                                                                                          | 
    {                                                                                                                   | 
      goto LABEL_26;                                                                                                    | 
    }                                                                                                                   | 
    CurrentProcess = PsGetCurrentProcess();                                                                             | 
    Address[0] = (volatile void *)PsGetProcessPeb(CurrentProcess);                                                      | 
    ProbeForRead(Address[0], 0x7D0uLL, 1u);                                                                             | 
    v8 = v30;                                                                                                           | 
    if ( *((_QWORD *)Address[0] + 2) == v30 )                                                                           | 
    {                                                                                                                   | 
      v8 = 0LL;                                                                                                         | 
      v30 = 0LL;                                                                                                        | 
      a2 = 0LL;                                                                                                         | 
    }                                                                                                                   | 
    if ( !v8 || (v13 = a2) == 0 )                                                                                       | 
    {                                                                                                                   | 
LABEL_26:                                                                                                               | 
      v15 = 0LL;                                                                                                        | 
LABEL_27:                                                                                                               | 
      if ( a6 && (a6 & 0xFFFFFFBD) != 0 )                                                                               | 
      {                                                                                                                 | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 3408LL);                                     | 
      }                                                                                                                 | 
      v10 = zzzSetWindowsHookEx(v8, v15, v11, a4, a5, a6, 0LL);                                                         | 
      EtwTraceAuditApiSetWindowsHookEx(a4, v15, v8, a5, v10);                                                           | 
      goto LABEL_6;                                                                                                     | 
    }                                                                                                                   | 
    if ( a2 >= MmUserProbeAddress )                                                                                     | 
    {                                                                                                                   | 
      v13 = MmUserProbeAddress;                                                                                         | 
    }                                                                                                                   | 
    v14 = *(_DWORD *)v13;                                                                                               | 
    v32 = *(_DWORD *)v13;                                                                                               | 
    LODWORD(Address[0]) = *(_DWORD *)v13;                                                                               | 
    v15 = *(volatile void **)(v13 + 8);                                                                                 | 
    Address[1] = v15;                                                                                                   | 
    if ( ((unsigned __int8)v15 & 1) != 0 )                                                                              | 
    {                                                                                                                   | 
      ExRaiseDatatypeMisalignment();                                                                                    | 
    }                                                                                                                   | 
    v16 = (ULONG64)v15 + (unsigned __int16)v14 + 2;                                                                     | 
    if ( v16 <= (unsigned __int64)v15 || v16 >= MmUserProbeAddress )                                                    | 
    {                                                                                                                   | 
      ExRaiseAccessViolation();                                                                                         | 
    }                                                                                                                   | 
    if ( (unsigned __int16)v14 > HIWORD(v32) )                                                                          | 
    {                                                                                                                   | 
      if ( (v14 & 1) == 0 )                                                                                             | 
      {                                                                                                                 | 
        goto LABEL_24;                                                                                                  | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    else if ( (v14 & 1) == 0 )                                                                                          | 
    {                                                                                                                   | 
      Address[3] = v15;                                                                                                 | 
      v8 = v30;                                                                                                         | 
      goto LABEL_27;                                                                                                    | 
    }                                                                                                                   | 
    MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 3396LL);                                         | 
LABEL_24:                                                                                                               | 
    ExRaiseAccessViolation();                                                                                           | 
  }                                                                                                                     | 
  UserSetLastError(87LL);                                                                                               | 
LABEL_6:                                                                                                                | 
  UserSessionSwitchLeaveCrit();                                                                                         | 
  return v10;                                                                                                           | 
}                                                                                                                       | 
155 row(s)
