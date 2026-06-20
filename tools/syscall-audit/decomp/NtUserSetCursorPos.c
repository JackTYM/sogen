_BOOL8 __fastcall NtUserSetCursorPos(int a1, int a2)                                                                     | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v4 = 1;                                                                                                                | 
  v5 = *(unsigned int **)&WPP_GLOBAL_Control != &WPP_GLOBAL_Control                                                      | 
    && (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x80000) != 0                                               | 
    && *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) >= 4u;                                                          | 
  v6 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                                        | 
  if ( v5 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                                  | 
  {                                                                                                                      | 
    UserSessionState = W32GetUserSessionState();                                                                         | 
    LOBYTE(v8) = v6;                                                                                                     | 
    LOBYTE(v9) = v5;                                                                                                     | 
    WPP_RECORDER_AND_TRACE_SF_DD(                                                                                        | 
      *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                                | 
      v9,                                                                                                                | 
      v8,                                                                                                                | 
      *(_QWORD *)(UserSessionState + 69512),                                                                             | 
      4,                                                                                                                 | 
      20,                                                                                                                | 
      47,                                                                                                                | 
      (__int64)&WPP_ee910b509ed73fb465c6a84e13eeb380_Traceguids,                                                         | 
      a1,                                                                                                                | 
      a2);                                                                                                               | 
  }                                                                                                                      | 
  InputTraceLogging::ThreadLockedPerfRegion::ThreadLockedPerfRegion(                                                     | 
    (InputTraceLogging::ThreadLockedPerfRegion *)&v18,                                                                   | 
    "SetCursorPos",                                                                                                      | 
    0LL);                                                                                                                | 
  EnterCrit(0LL, 0LL);                                                                                                   | 
  v10 = zzzSetCursorPos(a1, a2);                                                                                         | 
  v11 = v10;                                                                                                             | 
  if ( !v10 )                                                                                                            | 
  {                                                                                                                      | 
    if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                                    | 
      || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x80000) == 0                                             | 
      || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 2u )                                                        | 
    {                                                                                                                    | 
      v4 = 0;                                                                                                            | 
    }                                                                                                                    | 
    v13 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                                     | 
    if ( v4 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                                | 
    {                                                                                                                    | 
      LastError = UserGetLastError();                                                                                    | 
      v15 = W32GetUserSessionState();                                                                                    | 
      LOBYTE(v16) = v13;                                                                                                 | 
      LOBYTE(v17) = v4;                                                                                                  | 
      WPP_RECORDER_AND_TRACE_SF_D(                                                                                       | 
        *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                              | 
        v17,                                                                                                             | 
        v16,                                                                                                             | 
        *(_QWORD *)(v15 + 69512),                                                                                        | 
        2,                                                                                                               | 
        20,                                                                                                              | 
        48,                                                                                                              | 
        (__int64)&WPP_ee910b509ed73fb465c6a84e13eeb380_Traceguids,                                                       | 
        LastError);                                                                                                      | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  UserSessionSwitchLeaveCrit();                                                                                          | 
  InputTraceLogging::ThreadLockedPerfRegion::~ThreadLockedPerfRegion((InputTraceLogging::ThreadLockedPerfRegion *)&v18); | 
  return v11;                                                                                                            | 
}                                                                                                                        | 
64 row(s)
