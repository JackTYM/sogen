HDC __fastcall NtUserGetDC(__int64 a1)                                                       | 
{                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                              | 
                                                                                             | 
  v2 = 1;                                                                                    | 
  v3 = EnterSharedCrit(0LL, 1LL);                                                            | 
  if ( a1 )                                                                                  | 
  {                                                                                          | 
    v4 = ValidateHwndEx(a1);                                                                 | 
    if ( !v4 )                                                                               | 
    {                                                                                        | 
      DC = 0LL;                                                                              | 
      goto LABEL_21;                                                                         | 
    }                                                                                        | 
  }                                                                                          | 
  else                                                                                       | 
  {                                                                                          | 
    v4 = 0LL;                                                                                | 
  }                                                                                          | 
  if ( (unsigned int)Feature_LearningModeExpansion__private_IsEnabledDeviceUsageNoInline() ) | 
  {                                                                                          | 
    if ( !v4 && tagPROCESSINFO::HasUILimit(*(tagPROCESSINFO **)(v3 + 464), 1u) )             | 
    {                                                                                        | 
      if ( *(_QWORD *)(*(_QWORD *)(*((_QWORD *)PtiCurrent() + 62) + 8LL) + 24LL) )           | 
      {                                                                                      | 
        v6 = **(_QWORD **)(*(_QWORD *)(*((_QWORD *)PtiCurrent() + 62) + 8LL) + 24LL);        | 
      }                                                                                      | 
      else                                                                                   | 
      {                                                                                      | 
        LODWORD(v6) = 0;                                                                     | 
      }                                                                                      | 
      if ( !ValidateHwndEx(v6) )                                                             | 
      {                                                                                      | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 1066LL);          | 
      }                                                                                      | 
    }                                                                                        | 
  }                                                                                          | 
  else if ( tagPROCESSINFO::HasUILimit(*(tagPROCESSINFO **)(v3 + 464), 1u) && !v4 )          | 
  {                                                                                          | 
    v7 = *(__int64 **)(*(_QWORD *)(*((_QWORD *)PtiCurrent() + 62) + 8LL) + 24LL);            | 
    if ( v7 )                                                                                | 
    {                                                                                        | 
      v8 = *v7;                                                                              | 
    }                                                                                        | 
    else                                                                                     | 
    {                                                                                        | 
      LODWORD(v8) = 0;                                                                       | 
    }                                                                                        | 
    v2 = ValidateHwndEx(v8) != 0;                                                            | 
  }                                                                                          | 
  DC = (HDC)GetDC(v4);                                                                       | 
  if ( !v2 )                                                                                 | 
  {                                                                                          | 
    v12 = 0LL;                                                                               | 
    GreCreateRectRgnIndirect((struct REGION_CORE *)&v12);                                    | 
    UserSessionState = W32GetUserSessionState();                                             | 
    GreLockVisRgn(*(_QWORD *)(*(_QWORD *)(UserSessionState + 57104) + 48LL));                | 
    GreSelectVisRgnShared(DC);                                                               | 
    v10 = W32GetUserSessionState();                                                          | 
    GreUnlockVisRgn(*(_QWORD *)(*(_QWORD *)(v10 + 57104) + 48LL));                           | 
  }                                                                                          | 
LABEL_21:                                                                                    | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                  | 
  return DC;                                                                                 | 
}                                                                                            | 
65 row(s)
