__int64 __fastcall NtUserGetAsyncKeyState(unsigned int a1)                                                 | 
{                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                            | 
                                                                                                           | 
  v2 = 1;                                                                                                  | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v32, 1LL);                                                   | 
  if ( *(_QWORD *)(((__int64 (*)(void))W32GetUserSessionState)() + 19040) )                                | 
  {                                                                                                        | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                                        | 
    v5 = CurrentProcessWin32Process;                                                                       | 
    if ( CurrentProcessWin32Process )                                                                      | 
    {                                                                                                      | 
      v4 = -*(_QWORD *)CurrentProcessWin32Process;                                                         | 
      v5 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process;          | 
    }                                                                                                      | 
    if ( v5 != *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState(v4) + 19040) + 464LL) )                      | 
    {                                                                                                      | 
      EtwTraceGetAsyncKeyState(v32);                                                                       | 
    }                                                                                                      | 
  }                                                                                                        | 
  v6 = 0;                                                                                                  | 
  v7 = *(__int64 (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 48) + 672LL);                     | 
  if ( v7 )                                                                                                | 
  {                                                                                                        | 
    v8 = v7();                                                                                             | 
  }                                                                                                        | 
  else                                                                                                     | 
  {                                                                                                        | 
    v8 = -1073741637;                                                                                      | 
  }                                                                                                        | 
  if ( v8 >= 0 )                                                                                           | 
  {                                                                                                        | 
    v9 = *(__int64 (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 48) + 680LL);                   | 
    if ( v9 )                                                                                              | 
    {                                                                                                      | 
      v6 = v9();                                                                                           | 
    }                                                                                                      | 
  }                                                                                                        | 
  if ( v6 )                                                                                                | 
  {                                                                                                        | 
    if ( !(unsigned int)Feature_AsyncKeyStateRecentDown_Removal2__private_IsEnabledDeviceUsageNoInline() ) | 
    {                                                                                                      | 
LABEL_14:                                                                                                  | 
      UserSetLastError(5LL);                                                                               | 
LABEL_15:                                                                                                  | 
      AsyncKeyState = 0;                                                                                   | 
      goto LABEL_41;                                                                                       | 
    }                                                                                                      | 
    if ( WPP_GLOBAL_Control == (CTouchProcessor *)&WPP_GLOBAL_Control                                      | 
      || (*((_DWORD *)WPP_GLOBAL_Control + 11) & 2) == 0                                                   | 
      || *((_BYTE *)WPP_GLOBAL_Control + 41) < 2u )                                                        | 
    {                                                                                                      | 
      v2 = 0;                                                                                              | 
    }                                                                                                      | 
    v11 = WPP_RECORDER_INITIALIZED != (_UNKNOWN *)&WPP_RECORDER_INITIALIZED;                               | 
    if ( v2 || WPP_RECORDER_INITIALIZED != (_UNKNOWN *)&WPP_RECORDER_INITIALIZED )                         | 
    {                                                                                                      | 
      UserSessionState = W32GetUserSessionState(&WPP_RECORDER_INITIALIZED);                                | 
      LOBYTE(v13) = v11;                                                                                   | 
      LOBYTE(v14) = v2;                                                                                    | 
      WPP_RECORDER_AND_TRACE_SF_(                                                                          | 
        *((_QWORD *)WPP_GLOBAL_Control + 3),                                                               | 
        v14,                                                                                               | 
        v13,                                                                                               | 
        *(_QWORD *)(UserSessionState + 69496),                                                             | 
        2,                                                                                                 | 
        2,                                                                                                 | 
        90,                                                                                                | 
        (__int64)&WPP_2d9ff25e81b038623ce9c364b7ad2133_Traceguids);                                        | 
    }                                                                                                      | 
    AsyncKeyState = 0;                                                                                     | 
LABEL_24:                                                                                                  | 
    UserSetLastError(5LL);                                                                                 | 
    goto LABEL_41;                                                                                         | 
  }                                                                                                        | 
  if ( !(unsigned int)ApiSetEditionIsGpqForegroundAccessibleCurrent(1LL) )                                 | 
  {                                                                                                        | 
    v16 = *(_QWORD *)(W32GetUserSessionState(v15) + 19024);                                                | 
    v33 = *(_QWORD *)(v16 + 544);                                                                          | 
    v17 = *(_DWORD *)(v16 + 552);                                                                          | 
    v18 = *(_QWORD *)(W32GetUserSessionState(v16) + 19024);                                                | 
    v30 = v33;                                                                                             | 
    v31 = v17;                                                                                             | 
    EtwTraceUIPIInputError((_DWORD)v32, 0, v18, (unsigned int)&v30, 3);                                    | 
    if ( !(unsigned int)Feature_AsyncKeyStateRecentDown_Removal2__private_IsEnabledDeviceUsageNoInline() ) | 
    {                                                                                                      | 
      goto LABEL_14;                                                                                       | 
    }                                                                                                      | 
    if ( WPP_GLOBAL_Control == (CTouchProcessor *)&WPP_GLOBAL_Control                                      | 
      || (*((_DWORD *)WPP_GLOBAL_Control + 11) & 2) == 0                                                   | 
      || *((_BYTE *)WPP_GLOBAL_Control + 41) < 2u )                                                        | 
    {                                                                                                      | 
      v2 = 0;                                                                                              | 
    }                                                                                                      | 
    v19 = WPP_RECORDER_INITIALIZED != (_UNKNOWN *)&WPP_RECORDER_INITIALIZED;                               | 
    if ( v2 || WPP_RECORDER_INITIALIZED != (_UNKNOWN *)&WPP_RECORDER_INITIALIZED )                         | 
    {                                                                                                      | 
      v20 = W32GetUserSessionState(&WPP_RECORDER_INITIALIZED);                                             | 
      LOBYTE(v21) = v19;                                                                                   | 
      LOBYTE(v22) = v2;                                                                                    | 
      WPP_RECORDER_AND_TRACE_SF_(                                                                          | 
        *((_QWORD *)WPP_GLOBAL_Control + 3),                                                               | 
        v22,                                                                                               | 
        v21,                                                                                               | 
        *(_QWORD *)(v20 + 69496),                                                                          | 
        2,                                                                                                 | 
        2,                                                                                                 | 
        91,                                                                                                | 
        (__int64)&WPP_2d9ff25e81b038623ce9c364b7ad2133_Traceguids);                                        | 
    }                                                                                                      | 
    AsyncKeyState = 0;                                                                                     | 
    goto LABEL_24;                                                                                         | 
  }                                                                                                        | 
  v23 = v32;                                                                                               | 
  if ( (unsigned __int8)IsKeyboardDelegationEnabledForThread(v32) )                                        | 
  {                                                                                                        | 
    *(_DWORD *)(*((_QWORD *)v23 + 65) + 124LL) = 0;                                                        | 
    *(_QWORD *)(*((_QWORD *)v32 + 65) + 128LL) = 0LL;                                                      | 
    if ( !(unsigned int)Feature_AsyncKeyStateRecentDown_Removal2__private_IsEnabledDeviceUsageNoInline() ) | 
    {                                                                                                      | 
      *(_QWORD *)(*((_QWORD *)v32 + 65) + 136LL) = 0LL;                                                    | 
    }                                                                                                      | 
    goto LABEL_15;                                                                                         | 
  }                                                                                                        | 
  AsyncKeyState = GetAsyncKeyState(a1);                                                                    | 
  v33 = *(_QWORD *)(W32GetUserSessionState(v24) + 20016);                                                  | 
  v25 = v32;                                                                                               | 
  *(_DWORD *)(*((_QWORD *)v32 + 65) + 124LL) = *(_DWORD *)(v33 + 6988);                                    | 
  v26 = W32GetUserSessionState(v25);                                                                       | 
  *(_QWORD *)(*((_QWORD *)v32 + 65) + 128LL) = *(_QWORD *)(v26 + 14328);                                   | 
  if ( !(unsigned int)Feature_AsyncKeyStateRecentDown_Removal2__private_IsEnabledDeviceUsageNoInline() )   | 
  {                                                                                                        | 
    v28 = *(_QWORD *)(W32GetUserSessionState(v27) + 14456);                                                | 
    *(_QWORD *)(*((_QWORD *)v32 + 65) + 136LL) = v28;                                                      | 
  }                                                                                                        | 
LABEL_41:                                                                                                  | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                                | 
  return AsyncKeyState;                                                                                    | 
}                                                                                                          | 
139 row(s)
