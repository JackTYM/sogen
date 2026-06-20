__int64 __fastcall NtUserUnregisterClass(ULONG64 a1, __int64 a2, void *a3)                 | 
{                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                            | 
                                                                                           | 
  Src = 0LL;                                                                               | 
  v12 = 0LL;                                                                               | 
  EnterCrit(0LL, 0LL);                                                                     | 
  if ( a1 >= MmUserProbeAddress )                                                          | 
  {                                                                                        | 
    a1 = MmUserProbeAddress;                                                               | 
  }                                                                                        | 
  v6 = *(_DWORD *)a1;                                                                      | 
  v7 = *(_QWORD *)(a1 + 8);                                                                | 
  if ( (v7 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                 | 
  {                                                                                        | 
    if ( (v7 & 1) != 0 )                                                                   | 
    {                                                                                      | 
      ExRaiseDatatypeMisalignment();                                                       | 
    }                                                                                      | 
    v8 = v7 + (unsigned __int16)v6 + 2LL;                                                  | 
    if ( v8 <= v7 || v8 >= MmUserProbeAddress )                                            | 
    {                                                                                      | 
      ExRaiseAccessViolation();                                                            | 
    }                                                                                      | 
    if ( (unsigned __int16)v6 > (unsigned __int16)HIWORD(*(_DWORD *)a1) || (v6 & 1) != 0 ) | 
    {                                                                                      | 
      if ( (v6 & 1) != 0 )                                                                 | 
      {                                                                                    | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 10436LL);       | 
      }                                                                                    | 
      ExRaiseAccessViolation();                                                            | 
    }                                                                                      | 
  }                                                                                        | 
  v9 = UnregisterClass(v7, a2, &Src);                                                      | 
  if ( v9 )                                                                                | 
  {                                                                                        | 
    if ( (unsigned __int64)a3 >= MmUserProbeAddress )                                      | 
    {                                                                                      | 
      a3 = (void *)MmUserProbeAddress;                                                     | 
    }                                                                                      | 
    RtlCopyVolatileMemory(a3, &Src, 0x18uLL);                                              | 
  }                                                                                        | 
  UserSessionSwitchLeaveCrit();                                                            | 
  return v9;                                                                               | 
}                                                                                          | 
45 row(s)
