__int64 __fastcall NtUserRegisterWindowMessage(ULONG64 a1)                       | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  v1 = a1;                                                                       | 
  if ( a1 >= MmUserProbeAddress )                                                | 
  {                                                                              | 
    v1 = MmUserProbeAddress;                                                     | 
  }                                                                              | 
  v2 = *(_DWORD *)v1;                                                            | 
  v8 = *(_DWORD *)v1;                                                            | 
  v3 = *(const unsigned __int16 **)(v1 + 8);                                     | 
  if ( ((unsigned __int8)v3 & 1) != 0 )                                          | 
  {                                                                              | 
    ExRaiseDatatypeMisalignment();                                               | 
  }                                                                              | 
  v4 = (ULONG64)v3 + (unsigned __int16)v2 + 2;                                   | 
  if ( v4 <= (unsigned __int64)v3 || v4 >= MmUserProbeAddress )                  | 
  {                                                                              | 
    ExRaiseAccessViolation();                                                    | 
  }                                                                              | 
  if ( (unsigned __int16)v2 > HIWORD(v8) )                                       | 
  {                                                                              | 
    if ( (v2 & 1) == 0 )                                                         | 
    {                                                                            | 
      goto LABEL_12;                                                             | 
    }                                                                            | 
    goto LABEL_11;                                                               | 
  }                                                                              | 
  if ( (v2 & 1) != 0 )                                                           | 
  {                                                                              | 
LABEL_11:                                                                        | 
    MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 10194LL); | 
LABEL_12:                                                                        | 
    ExRaiseAccessViolation();                                                    | 
  }                                                                              | 
  v5 = RtlStringCchCopyW(v9, 0x100uLL, v3);                                      | 
  v6 = 0;                                                                        | 
  if ( v5 >= 0 )                                                                 | 
  {                                                                              | 
    return (unsigned __int16)UserAddAtomEx(v9, 0LL, 2LL);                        | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    UserSetLastStatus(v5);                                                       | 
  }                                                                              | 
  return v6;                                                                     | 
}                                                                                | 
48 row(s)
