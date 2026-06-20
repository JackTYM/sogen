__int64 __fastcall NtUserTransformRect(_OWORD *a1, unsigned int a2, unsigned int a3, __int64 a4)             | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v15 = a1;                                                                                                  | 
  v7 = 1;                                                                                                    | 
  EnterLeaveCritShared::EnterLeaveCritShared(v17, 1LL);                                                      | 
  v18 = 0LL;                                                                                                 | 
  v14[0] = 0LL;                                                                                              | 
  v8 = a2 & 0x7FFFFFFF;                                                                                      | 
  if ( (a2 & 0x7FFFFFFF) == 0x22 || v8 == 24592 || v8 == 1073766416 || v8 == 18 )                            | 
  {                                                                                                          | 
    valid = 1;                                                                                               | 
  }                                                                                                          | 
  else if ( (((a2 & 0x7FFE00FF) - 17) & 0xFFFFFFFD) != 0 || (a2 & 0x7FFE00FF) == 0x13 )                      | 
  {                                                                                                          | 
    valid = 0;                                                                                               | 
  }                                                                                                          | 
  else                                                                                                       | 
  {                                                                                                          | 
    valid = (unsigned __int8)IsDpiValidPlateau((v8 >> 8) & 0x1FF);                                           | 
  }                                                                                                          | 
  if ( valid                                                                                                 | 
    && ((v10 = a3 & 0x7FFFFFFF, (a3 & 0x7FFFFFFF) == 0x22) || v10 == 24592 || v10 == 1073766416 || v10 == 18 | 
      ? (v11 = 1)                                                                                            | 
      : (((a3 & 0x7FFE00FF) - 17) & 0xFFFFFFFD) != 0 || (a3 & 0x7FFE00FF) == 0x13                            | 
      ? (v11 = 0)                                                                                            | 
      : (v11 = (unsigned __int8)IsDpiValidPlateau((v10 >> 8) & 0x1FF)),                                      | 
        v11) )                                                                                               | 
  {                                                                                                          | 
    v12 = v15;                                                                                               | 
    if ( (unsigned __int64)v15 >= MmUserProbeAddress )                                                       | 
    {                                                                                                        | 
      v12 = (_BYTE *)MmUserProbeAddress;                                                                     | 
    }                                                                                                        | 
    *v12 = *v12;                                                                                             | 
    v12[15] = v12[15];                                                                                       | 
    v18 = *v15;                                                                                              | 
    if ( a4 && (a2 & 0xF) == 2 )                                                                             | 
    {                                                                                                        | 
      v14[0] = ValidateHmonitor(a4);                                                                         | 
    }                                                                                                        | 
    if ( !v14[0] )                                                                                           | 
    {                                                                                                        | 
      LODWORD(v16) = (DWORD2(v18) + (int)v18) / 2;                                                           | 
      HIDWORD(v16) = (HIDWORD(v18) + DWORD1(v18)) / 2;                                                       | 
      v14[0] = GuessMonitorOverrideForCoordinateConversions(v16, a3, 0LL);                                   | 
    }                                                                                                        | 
    LogicalToPhysicalDPIRect(&v18, &v18, a3, v14);                                                           | 
    PhysicalToLogicalDPIRect(&v18, &v18, a2, v14);                                                           | 
    *v15 = v18;                                                                                              | 
  }                                                                                                          | 
  else                                                                                                       | 
  {                                                                                                          | 
    v7 = 0;                                                                                                  | 
    UserSetLastError(87LL);                                                                                  | 
  }                                                                                                          | 
  UserSessionSwitchLeaveCrit();                                                                              | 
  return v7;                                                                                                 | 
}                                                                                                            | 
60 row(s)
