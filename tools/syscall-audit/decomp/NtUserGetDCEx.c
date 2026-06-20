__int64 __fastcall NtUserGetDCEx(__int64 a1, __int64 a2, unsigned int a3)                                 | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v6 = EnterSharedCrit(0LL, 1LL);                                                                         | 
  if ( a2 && !(unsigned int)GreIsValidRegion(a2)                                                          | 
    || a2 == *(_QWORD *)(W32GetUserSessionState() + 43200)                                                | 
    || (a3 & 0xFFDAF800) != 0 )                                                                           | 
  {                                                                                                       | 
    v13 = 87LL;                                                                                           | 
    goto LABEL_24;                                                                                        | 
  }                                                                                                       | 
  if ( !a1 )                                                                                              | 
  {                                                                                                       | 
    v7 = *(__int64 **)(*(_QWORD *)(*((_QWORD *)PtiCurrent() + 62) + 8LL) + 24LL);                         | 
    IsEnabledDeviceUsageNoInline = Feature_LearningModeExpansion__private_IsEnabledDeviceUsageNoInline(); | 
    v10 = *(tagPROCESSINFO **)(v6 + 464);                                                                 | 
    if ( IsEnabledDeviceUsageNoInline )                                                                   | 
    {                                                                                                     | 
      if ( tagPROCESSINFO::HasUILimit(v10, 1u) )                                                          | 
      {                                                                                                   | 
        v11 = 0LL;                                                                                        | 
        if ( v7 )                                                                                         | 
        {                                                                                                 | 
          v11 = *v7;                                                                                      | 
        }                                                                                                 | 
        if ( !ValidateHwnd(v11) )                                                                         | 
        {                                                                                                 | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 7042LL);                     | 
        }                                                                                                 | 
      }                                                                                                   | 
      goto LABEL_14;                                                                                      | 
    }                                                                                                     | 
    if ( !tagPROCESSINFO::HasUILimit(v10, 1u) )                                                           | 
    {                                                                                                     | 
      goto LABEL_14;                                                                                      | 
    }                                                                                                     | 
    v12 = 0LL;                                                                                            | 
    if ( v7 )                                                                                             | 
    {                                                                                                     | 
      v12 = *v7;                                                                                          | 
    }                                                                                                     | 
    if ( ValidateHwnd(v12) )                                                                              | 
    {                                                                                                     | 
      goto LABEL_14;                                                                                      | 
    }                                                                                                     | 
    v13 = 5LL;                                                                                            | 
LABEL_24:                                                                                                 | 
    DCEx = 0LL;                                                                                           | 
    UserSetLastError(v13);                                                                                | 
    goto LABEL_25;                                                                                        | 
  }                                                                                                       | 
  v7 = (__int64 *)ValidateHwnd(a1);                                                                       | 
  if ( !v7 )                                                                                              | 
  {                                                                                                       | 
    DCEx = 0LL;                                                                                           | 
    goto LABEL_25;                                                                                        | 
  }                                                                                                       | 
LABEL_14:                                                                                                 | 
  DCEx = 0LL;                                                                                             | 
  if ( !a2 || (unsigned int)GreLockRegion(a2) )                                                           | 
  {                                                                                                       | 
    DCEx = _GetDCEx(v7, a2, a3);                                                                          | 
    if ( a2 )                                                                                             | 
    {                                                                                                     | 
      GreUnlockRegion(a2);                                                                                | 
    }                                                                                                     | 
  }                                                                                                       | 
LABEL_25:                                                                                                 | 
  UserSessionSwitchLeaveCrit();                                                                           | 
  return DCEx;                                                                                            | 
}                                                                                                         | 
72 row(s)
