__int64 __fastcall NtUserGetClassName(__int64 a1, int a2, ULONG64 a3)                   | 
{                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                         | 
                                                                                        | 
  AtomName = 0;                                                                         | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v16, 1LL);                                | 
  v7 = ValidateHwnd(a1);                                                                | 
  if ( v7 )                                                                             | 
  {                                                                                     | 
    if ( a3 >= MmUserProbeAddress )                                                     | 
    {                                                                                   | 
      a3 = MmUserProbeAddress;                                                          | 
    }                                                                                   | 
    v15 = *(_DWORD *)a3;                                                                | 
    v8 = *(volatile void **)(a3 + 8);                                                   | 
    ProbeForWrite(v8, HIWORD(v15), 2u);                                                 | 
    v14 = *(_WORD *)(*(_QWORD *)(*(_QWORD *)(v7 + 136) + 8LL) + 2LL);                   | 
    if ( a2 )                                                                           | 
    {                                                                                   | 
      v18 = *(_WORD *)(*(_QWORD *)(v7 + 40) + 42LL);                                    | 
      v9 = v18 & 0x2FFF;                                                                | 
      if ( (v18 & 0x2FFF) != 0 )                                                        | 
      {                                                                                 | 
        if ( v9 == 672 )                                                                | 
        {                                                                               | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 9842LL);   | 
        }                                                                               | 
        if ( v9 == 694 )                                                                | 
        {                                                                               | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 9843LL);   | 
        }                                                                               | 
        v10 = v9 - 666;                                                                 | 
        if ( v10 < 0x1F )                                                               | 
        {                                                                               | 
          _mm_lfence();                                                                 | 
          v11 = byte_140356038[v10];                                                    | 
          if ( (unsigned __int8)v11 > 0x19u || (v12 = 38797312, !_bittest(&v12, v11)) ) | 
          {                                                                             | 
            v17 = *(_QWORD *)(W32GetUserSessionState() + 20016);                        | 
            v14 = *(_WORD *)(v17 + 2 * v11 + 868);                                      | 
          }                                                                             | 
        }                                                                               | 
      }                                                                                 | 
    }                                                                                   | 
    AtomName = UserGetAtomName(v14, v8, HIWORD(v15) >> 1);                              | 
  }                                                                                     | 
  UserSessionSwitchLeaveCrit();                                                         | 
  return AtomName;                                                                      | 
}                                                                                       | 
49 row(s)
