__int64 __fastcall NtSetSystemTime(LARGE_INTEGER *a1, LARGE_INTEGER *a2)  | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  SystemTime.QuadPart = 0LL;                                              | 
  v14.QuadPart = 0LL;                                                     | 
  Time.QuadPart = 0LL;                                                    | 
  TimeFields = 0LL;                                                       | 
  *(_QWORD *)&String2.Length = 2228256LL;                                 | 
  String2.Buffer = L"systemManagement";                                   | 
  CurrentServerSiloGlobals = PsGetCurrentServerSiloGlobals();             | 
  if ( a1 )                                                               | 
  {                                                                       | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                    | 
    if ( !SeSinglePrivilegeCheck(SeSystemtimePrivilege, PreviousMode) )   | 
    {                                                                     | 
      if ( !(unsigned __int8)RtlIsMultiSessionSku() )                     | 
      {                                                                   | 
        RtlCapabilityCheck(0LL, &String2);                                | 
      }                                                                   | 
      return 3221225569LL;                                                | 
    }                                                                     | 
    if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )               | 
    {                                                                     | 
      return 3221225569LL;                                                | 
    }                                                                     | 
    if ( !PreviousMode )                                                  | 
    {                                                                     | 
      v8 = *a1;                                                           | 
      v14 = *a1;                                                          | 
      goto LABEL_17;                                                      | 
    }                                                                     | 
    if ( ((unsigned __int8)a1 & 3) == 0 )                                 | 
    {                                                                     | 
      v7 = 0x7FFFFFFF0000LL;                                              | 
      if ( !a2 )                                                          | 
      {                                                                   | 
LABEL_14:                                                                 | 
        v8 = *a1;                                                         | 
        v14 = *a1;                                                        | 
LABEL_17:                                                                 | 
        if ( v8.HighPart > 0x20000000u )                                  | 
        {                                                                 | 
          return 3221225485LL;                                            | 
        }                                                                 | 
        ExAcquireTimeRefreshLockExclusive();                              | 
        LOBYTE(v9) = 1;                                                   | 
        ExpSetSystemTime(v9, 0, 1, v14.LowPart, (__int64)&SystemTime);    | 
        ((void (__fastcall *)(_QWORD, _QWORD))SeAuditSystemTimeChange)(   | 
          (LARGE_INTEGER)SystemTime.QuadPart,                             | 
          (LARGE_INTEGER)v14.QuadPart);                                   | 
        ExReleaseTimeRefreshLockExclusive();                              | 
        if ( a2 )                                                         | 
        {                                                                 | 
          *a2 = SystemTime;                                               | 
        }                                                                 | 
        return 0;                                                         | 
      }                                                                   | 
      if ( ((unsigned __int8)a2 & 3) == 0 )                               | 
      {                                                                   | 
        if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                    | 
        {                                                                 | 
          v7 = (__int64)a2;                                               | 
        }                                                                 | 
        *(_BYTE *)v7 = *(_BYTE *)v7;                                      | 
        *(_BYTE *)(v7 + 7) = *(_BYTE *)(v7 + 7);                          | 
        goto LABEL_14;                                                    | 
      }                                                                   | 
    }                                                                     | 
    ExRaiseDatatypeMisalignment();                                        | 
  }                                                                       | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                 | 
  {                                                                       | 
    ExAcquireTimeRefreshLockExclusive();                                  | 
    ExpRefreshTimeZoneInformation(0LL);                                   | 
    ExReleaseTimeRefreshLockExclusive();                                  | 
    return 0;                                                             | 
  }                                                                       | 
  v10 = -1073741811;                                                      | 
  ExAcquireTimeRefreshLockExclusive();                                    | 
  v11 = ExpSystemIsInCmosMode;                                            | 
  if ( (unsigned __int8)HalQueryRealTimeClock(&TimeFields)                | 
    && RtlTimeFieldsToTime(&TimeFields, &Time)                            | 
    && (unsigned __int8)ExpRefreshTimeZoneInformation(0LL) )              | 
  {                                                                       | 
    v12.QuadPart = MEMORY[0xFFFFF78000000014];                            | 
    SystemTime.QuadPart = MEMORY[0xFFFFF78000000014];                     | 
    if ( !ExpRealTimeIsUniversal )                                        | 
    {                                                                     | 
      if ( v11 )                                                          | 
      {                                                                   | 
        ExLocalTimeToSystemTime(&Time, &v14);                             | 
        KeSetSystemTime(&v14, &SystemTime, 0LL);                          | 
        goto LABEL_29;                                                    | 
      }                                                                   | 
      ExSystemTimeToLocalTime(&SystemTime, &Time);                        | 
      RtlTimeToTimeFields(&Time, &TimeFields);                            | 
      HalSetRealTimeClock(&TimeFields);                                   | 
      v12 = SystemTime;                                                   | 
    }                                                                     | 
    v14 = v12;                                                            | 
LABEL_29:                                                                 | 
    PoNotifySystemTimeSet(                                                | 
      (unsigned int)&v14,                                                 | 
      (unsigned int)&SystemTime,                                          | 
      3,                                                                  | 
      (unsigned int)&Time,                                                | 
      *(_DWORD *)(*(_QWORD *)(CurrentServerSiloGlobals + 1224) + 436LL)); | 
    v10 = 0;                                                              | 
  }                                                                       | 
  ExReleaseTimeRefreshLockExclusive();                                    | 
  return v10;                                                             | 
}                                                                         | 
113 row(s)
