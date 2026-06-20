__int64 __fastcall NtUserPostThreadMessage(unsigned int a1, unsigned int a2, unsigned __int64 a3, __int64 a4) | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  v23 = a4;                                                                                                   | 
  EnterCrit(0LL, 0LL);                                                                                        | 
  if ( (a2 & 0xFFFE0000) == 0 )                                                                               | 
  {                                                                                                           | 
    v8 = 0;                                                                                                   | 
    v9 = PtiFromThreadId(a1);                                                                                 | 
    if ( !v9 )                                                                                                | 
    {                                                                                                         | 
LABEL_4:                                                                                                      | 
      v10 = 1444LL;                                                                                           | 
LABEL_5:                                                                                                      | 
      UserSetLastError(v10);                                                                                  | 
      goto LABEL_35;                                                                                          | 
    }                                                                                                         | 
    v11 = PtiCurrent();                                                                                       | 
    if ( *((_QWORD *)v11 + 62) != *(_QWORD *)(v9 + 496)                                                       | 
      && (_InterlockedCompareExchange((volatile signed __int32 *)v11 + 132, 0, 0) & 8) == 0                   | 
      && !GetDesktopView(*((_QWORD *)v11 + 58), *(_QWORD *)(v9 + 496)) )                                      | 
    {                                                                                                         | 
      if ( !(unsigned __int8)HasTcbPrivilege() )                                                              | 
      {                                                                                                       | 
        goto LABEL_4;                                                                                         | 
      }                                                                                                       | 
      if ( !UIPrivilegeIsolation::CheckAccess(                                                                | 
              (UIPrivilegeIsolation *)(*((_QWORD *)v11 + 58) + 856LL),                                        | 
              (const struct tagUIPI_INFO *)(*(_QWORD *)(v9 + 464) + 856LL),                                   | 
              v12) )                                                                                          | 
      {                                                                                                       | 
        if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                     | 
          || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x8000) == 0                               | 
          || (v13 = 1, *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 3u) )                              | 
        {                                                                                                     | 
          v13 = 0;                                                                                            | 
        }                                                                                                     | 
        v22 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                      | 
        if ( v13 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                | 
        {                                                                                                     | 
          tagTHREADINFO::GetTID((tagTHREADINFO *)v9);                                                         | 
          tagTHREADINFO::GetTID(v11);                                                                         | 
          UserSessionState = W32GetUserSessionState();                                                        | 
          LOBYTE(v15) = v13;                                                                                  | 
          LOBYTE(v16) = v22;                                                                                  | 
          WPP_RECORDER_AND_TRACE_SF_LDD(                                                                      | 
            *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                               | 
            v15,                                                                                              | 
            v16,                                                                                              | 
            *(_QWORD *)(UserSessionState + 69512));                                                           | 
        }                                                                                                     | 
        EtwTraceUIPIMsgError(*((_QWORD *)v11 + 58), *(_QWORD *)(v9 + 464), a2, a3, v23);                      | 
        goto LABEL_4;                                                                                         | 
      }                                                                                                       | 
    }                                                                                                         | 
    v17 = *(_QWORD *)(v9 + 464);                                                                              | 
    v18 = *((_QWORD *)v11 + 58);                                                                              | 
    if ( v17 == v18 )                                                                                         | 
    {                                                                                                         | 
      goto LABEL_34;                                                                                          | 
    }                                                                                                         | 
    if ( a2 == 717 )                                                                                          | 
    {                                                                                                         | 
      goto LABEL_35;                                                                                          | 
    }                                                                                                         | 
    if ( IsMessageAllowedAcrossILByReceiver(*(struct tagPROCESSINFO **)(v9 + 464), 0LL, a2)                   | 
      || (unsigned int)IsMessageAlwaysAllowedAcrossIL(a2)                                                     | 
      || a2 == 274 && ((a3 - 61472) & 0xFFFFFFFFFFFFFEBFuLL) == 0 && a3 != 61792                              | 
      || (!(unsigned int)Feature_MsgUseCSRSSTrueIL__private_IsEnabledDeviceUsageNoInline()                    | 
        ? (*(_QWORD *)v17 != *(_QWORD *)(W32GetUserGdiSessionState() + 40)                                    | 
         ? (v23 = *(_QWORD *)(v17 + 856), v24 = *(_DWORD *)(v17 + 864))                                       | 
         : (v23 = 0x2000LL, v24 = 0),                                                                         | 
           v20 = (const struct tagUIPI_INFO *)&v23)                                                           | 
        : (v20 = (const struct tagUIPI_INFO *)(v17 + 856)),                                                   | 
          UIPrivilegeIsolation::CheckAccess((UIPrivilegeIsolation *)(v18 + 856), v20, v19)) )                 | 
    {                                                                                                         | 
LABEL_34:                                                                                                     | 
      v8 = PostThreadMessage((struct tagTHREADINFO *)v9, a2, a3, a4);                                         | 
      goto LABEL_35;                                                                                          | 
    }                                                                                                         | 
    EtwTraceUIPIMsgError(v18, v17, a2, a3, a4);                                                               | 
    v10 = 5LL;                                                                                                | 
    goto LABEL_5;                                                                                             | 
  }                                                                                                           | 
  UserSetLastError(87LL);                                                                                     | 
  v8 = 0;                                                                                                     | 
LABEL_35:                                                                                                     | 
  UserSessionSwitchLeaveCrit();                                                                               | 
  return v8;                                                                                                  | 
}                                                                                                             | 
91 row(s)
