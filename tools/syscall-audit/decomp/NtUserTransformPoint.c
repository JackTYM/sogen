__int64 __fastcall NtUserTransformPoint(_QWORD *a1, unsigned int a2, unsigned int a3, __int64 a4)                 | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  v7 = 1;                                                                                                         | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v13, 1LL);                                                          | 
  v11 = 0LL;                                                                                                      | 
  v12[0] = 0LL;                                                                                                   | 
  if ( (unsigned int)IsValidKernelDpiAwarenessContext(a2) && (unsigned int)IsValidKernelDpiAwarenessContext(a3) ) | 
  {                                                                                                               | 
    v8 = a1;                                                                                                      | 
    if ( (unsigned __int64)a1 >= MmUserProbeAddress )                                                             | 
    {                                                                                                             | 
      v8 = (_BYTE *)MmUserProbeAddress;                                                                           | 
    }                                                                                                             | 
    *v8 = *v8;                                                                                                    | 
    v8[7] = v8[7];                                                                                                | 
    v9 = *a1;                                                                                                     | 
    v11 = *a1;                                                                                                    | 
    if ( a4 && (a2 & 0xF) == 2 )                                                                                  | 
    {                                                                                                             | 
      v12[0] = ValidateHmonitor(a4);                                                                              | 
      v9 = v11;                                                                                                   | 
    }                                                                                                             | 
    if ( !v12[0] )                                                                                                | 
    {                                                                                                             | 
      v12[0] = GuessMonitorOverrideForCoordinateConversions(v9, a3, 0LL);                                         | 
    }                                                                                                             | 
    LogicalToPhysicalDPIPoint(&v11, &v11, a3, v12);                                                               | 
    PhysicalToLogicalDPIPoint(&v11, &v11, a2, v12);                                                               | 
    *a1 = v11;                                                                                                    | 
  }                                                                                                               | 
  else                                                                                                            | 
  {                                                                                                               | 
    v7 = 0;                                                                                                       | 
    UserSetLastError(87LL);                                                                                       | 
  }                                                                                                               | 
  UserSessionSwitchLeaveCrit();                                                                                   | 
  return v7;                                                                                                      | 
}                                                                                                                 | 
40 row(s)
