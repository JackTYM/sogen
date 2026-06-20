__int64 __fastcall NtDxgkGetProperties(ULONG64 a1)                                                                      | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v1 = (const void *)a1;                                                                                                | 
  v21 = 0LL;                                                                                                            | 
  *(_OWORD *)Src = 0LL;                                                                                                 | 
  v23 = 0LL;                                                                                                            | 
  P = 0LL;                                                                                                              | 
  v26 = 0;                                                                                                              | 
  if ( a1 >= MmUserProbeAddress )                                                                                       | 
  {                                                                                                                     | 
    v1 = (const void *)MmUserProbeAddress;                                                                              | 
  }                                                                                                                     | 
  RtlCopyVolatileMemory(&v21, v1, 0x28uLL);                                                                             | 
  if ( !Src[0] )                                                                                                        | 
  {                                                                                                                     | 
    WdLogSingleEntry1(3LL, -1073741811LL, v2, v3);                                                                      | 
    WdLogGlobalForLineNumber = 6493;                                                                                    | 
    if ( P != v25 )                                                                                                     | 
    {                                                                                                                   | 
      if ( P )                                                                                                          | 
      {                                                                                                                 | 
        ExFreePoolWithTag(P, 0);                                                                                        | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    P = 0LL;                                                                                                            | 
    v26 = 0;                                                                                                            | 
    return 3221225485LL;                                                                                                | 
  }                                                                                                                     | 
  if ( !DWORD1(v21) )                                                                                                   | 
  {                                                                                                                     | 
    WdLogSingleEntry1(3LL, -1073741811LL, 0LL, v3);                                                                     | 
    WdLogGlobalForLineNumber = 6500;                                                                                    | 
    if ( P != v25 && P )                                                                                                | 
    {                                                                                                                   | 
      ExFreePoolWithTag(P, 0);                                                                                          | 
    }                                                                                                                   | 
    P = 0LL;                                                                                                            | 
    v26 = 0;                                                                                                            | 
    return 3221225485LL;                                                                                                | 
  }                                                                                                                     | 
  if ( DWORD2(v21) )                                                                                                    | 
  {                                                                                                                     | 
    WdLogSingleEntry1(3LL, -1073741811LL, DWORD1(v21), v3);                                                             | 
    WdLogGlobalForLineNumber = 6507;                                                                                    | 
    if ( P != v25 && P )                                                                                                | 
    {                                                                                                                   | 
      ExFreePoolWithTag(P, 0);                                                                                          | 
    }                                                                                                                   | 
    P = 0LL;                                                                                                            | 
    v26 = 0;                                                                                                            | 
    return 3221225485LL;                                                                                                | 
  }                                                                                                                     | 
  if ( (_DWORD)v21 == 4 )                                                                                               | 
  {                                                                                                                     | 
    v5 = 88;                                                                                                            | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    if ( (unsigned int)v21 <= 2 )                                                                                       | 
    {                                                                                                                   | 
LABEL_26:                                                                                                               | 
      v5 = 16;                                                                                                          | 
      goto LABEL_29;                                                                                                    | 
    }                                                                                                                   | 
    if ( (_DWORD)v21 != 3 )                                                                                             | 
    {                                                                                                                   | 
      if ( (_DWORD)v21 != 5 )                                                                                           | 
      {                                                                                                                 | 
        WdLogSingleEntry1(2LL, -1073741811LL, DWORD1(v21), v3);                                                         | 
        WdLogGlobalForLineNumber = 6554;                                                                                | 
        DxgkLogInternalTriageEvent(                                                                                     | 
          0,                                                                                                            | 
          0x40000,                                                                                                      | 
          -1,                                                                                                           | 
          (unsigned int)L"Invalid D3DKMT_PROPERTIES_TYPE, returning 0x%I64x",                                           | 
          -1073741811LL,                                                                                                | 
          0LL,                                                                                                          | 
          0LL,                                                                                                          | 
          0LL,                                                                                                          | 
          0LL);                                                                                                         | 
        if ( P != v25 && P )                                                                                            | 
        {                                                                                                               | 
          ExFreePoolWithTag(P, 0);                                                                                      | 
        }                                                                                                               | 
        P = 0LL;                                                                                                        | 
        v26 = 0;                                                                                                        | 
        return 3221225485LL;                                                                                            | 
      }                                                                                                                 | 
      goto LABEL_26;                                                                                                    | 
    }                                                                                                                   | 
    v5 = 32;                                                                                                            | 
  }                                                                                                                     | 
LABEL_29:                                                                                                               | 
  if ( DWORD1(v21) < v5 )                                                                                               | 
  {                                                                                                                     | 
    WdLogSingleEntry2(2LL, (int)v21, -1073741811LL);                                                                    | 
    WdLogGlobalForLineNumber = 6563;                                                                                    | 
    DxgkLogInternalTriageEvent(                                                                                         | 
      0,                                                                                                                | 
      0x40000,                                                                                                          | 
      -1,                                                                                                               | 
      (unsigned int)L"Invalid propertydata size specified for D3DKMT_PROPERTIES_TYPE %d, returning 0x%I64x",            | 
      (int)v21,                                                                                                         | 
      -1073741811LL,                                                                                                    | 
      0LL,                                                                                                              | 
      0LL,                                                                                                              | 
      0LL);                                                                                                             | 
    if ( P != v25 && P )                                                                                                | 
    {                                                                                                                   | 
      ExFreePoolWithTag(P, 0);                                                                                          | 
    }                                                                                                                   | 
    P = 0LL;                                                                                                            | 
    v26 = 0;                                                                                                            | 
    return 3221225485LL;                                                                                                | 
  }                                                                                                                     | 
  v6 = v5;                                                                                                              | 
  if ( 0xFFFFFFFFFFFFFFFFuLL / v5 )                                                                                     | 
  {                                                                                                                     | 
    Pool2 = (_DWORD *)ExAllocatePool2(256LL, v5, 1265072196LL);                                                         | 
    P = Pool2;                                                                                                          | 
    v26 = v5;                                                                                                           | 
    v20 = Pool2;                                                                                                        | 
    v6 = v5;                                                                                                            | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    Pool2 = 0LL;                                                                                                        | 
    v20 = 0LL;                                                                                                          | 
  }                                                                                                                     | 
  if ( !Pool2 )                                                                                                         | 
  {                                                                                                                     | 
    WdLogSingleEntry2(2LL, v5, -1073741801LL);                                                                          | 
    WdLogGlobalForLineNumber = 6572;                                                                                    | 
    DxgkLogInternalTriageEvent(                                                                                         | 
      0,                                                                                                                | 
      0x40000,                                                                                                          | 
      -1,                                                                                                               | 
      (unsigned int)L"Out of memory allocating pPropertyData of size 0x%I64x, returning 0x%I64x",                       | 
      v6,                                                                                                               | 
      -1073741801LL,                                                                                                    | 
      0LL,                                                                                                              | 
      0LL,                                                                                                              | 
      0LL);                                                                                                             | 
    if ( P != v25 && P )                                                                                                | 
    {                                                                                                                   | 
      ExFreePoolWithTag(P, 0);                                                                                          | 
    }                                                                                                                   | 
    P = 0LL;                                                                                                            | 
    v26 = 0;                                                                                                            | 
    return 3221225495LL;                                                                                                | 
  }                                                                                                                     | 
  v8 = Src[0];                                                                                                          | 
  if ( (char *)Src[0] + v5 < Src[0] || (char *)Src[0] + v5 > (void *)MmUserProbeAddress )                               | 
  {                                                                                                                     | 
    *(_BYTE *)MmUserProbeAddress = 0;                                                                                   | 
  }                                                                                                                     | 
  memmove(Pool2, v8, v5);                                                                                               | 
  CurrentProcess = PsGetCurrentProcess();                                                                               | 
  ProcessDxgProcess = PsGetProcessDxgProcess(CurrentProcess);                                                           | 
  v12 = (struct DXGPROCESS *)ProcessDxgProcess;                                                                         | 
  if ( ProcessDxgProcess && (*(_DWORD *)(ProcessDxgProcess + 408) & 0x80) == 0 )                                        | 
  {                                                                                                                     | 
    goto LABEL_52;                                                                                                      | 
  }                                                                                                                     | 
  Current = DXGTHREAD::GetCurrent();                                                                                    | 
  if ( !Current )                                                                                                       | 
  {                                                                                                                     | 
    CurrentThread = KeGetCurrentThread();                                                                               | 
    Global = DXGGLOBAL::GetGlobal();                                                                                    | 
    v14 = DXGGLOBAL::SearchDxgThreadList(Global, CurrentThread);                                                        | 
    if ( v14 )                                                                                                          | 
    {                                                                                                                   | 
      Pool2 = v20;                                                                                                      | 
      goto LABEL_54;                                                                                                    | 
    }                                                                                                                   | 
    WdLogSingleEntry0(2LL);                                                                                             | 
    WdLogGlobalForLineNumber = 2935;                                                                                    | 
    DxgkLogInternalTriageEvent(0, 0x40000, -1, (unsigned int)L"Failed to find DXGPROCESS", 2935LL, 0LL, 0LL, 0LL, 0LL); | 
    Pool2 = v20;                                                                                                        | 
LABEL_52:                                                                                                               | 
    v14 = v12;                                                                                                          | 
    goto LABEL_54;                                                                                                      | 
  }                                                                                                                     | 
  v14 = (struct DXGPROCESS *)*((_QWORD *)Current + 3);                                                                  | 
  if ( !v14 )                                                                                                           | 
  {                                                                                                                     | 
    v14 = v12;                                                                                                          | 
  }                                                                                                                     | 
LABEL_54:                                                                                                               | 
  if ( (_DWORD)v21 == 4 )                                                                                               | 
  {                                                                                                                     | 
    if ( Pool2[2] < 0x50u )                                                                                             | 
    {                                                                                                                   | 
      WdLogSingleEntry1(2LL, -1073741811LL, v11, v14);                                                                  | 
      WdLogGlobalForLineNumber = 6629;                                                                                  | 
      v17 = L"Invalid cbSize specified of PresentStatsDwm2 , returning 0x%I64x";                                        | 
LABEL_68:                                                                                                               | 
      DxgkLogInternalTriageEvent(0, 0x40000, -1, (_DWORD)v17, -1073741811LL, 0LL, 0LL, 0LL, 0LL);                       | 
      if ( P != v25 && P )                                                                                              | 
      {                                                                                                                 | 
        ExFreePoolWithTag(P, 0);                                                                                        | 
      }                                                                                                                 | 
      return 3221225485LL;                                                                                              | 
    }                                                                                                                   | 
    v18 = PresentStatsDwm2Property(v14, (struct _D3DKMT_PROPERTIES_PRESENT_STATS_DWM2 *)Pool2);                         | 
  }                                                                                                                     | 
  else if ( (_DWORD)v21 )                                                                                               | 
  {                                                                                                                     | 
    switch ( (_DWORD)v21 )                                                                                              | 
    {                                                                                                                   | 
      case 1:                                                                                                           | 
        v18 = ProcessUserPreferredAdapterProperty(                                                                      | 
                v14,                                                                                                    | 
                (struct _D3DKMT_PROPERTIES_PROCESS_PREFERRED_ADAPTER_ID *)Pool2,                                        | 
                0);                                                                                                     | 
        break;                                                                                                          | 
      case 2:                                                                                                           | 
        v18 = ProcessDefaultHighPerformanceAdapterProperty(                                                             | 
                v14,                                                                                                    | 
                (struct _D3DKMT_PROPERTIES_PROCESS_PREFERRED_ADAPTER_ID *)Pool2,                                        | 
                0);                                                                                                     | 
        break;                                                                                                          | 
      case 3:                                                                                                           | 
        v18 = PresentDurationPlaneProperty(v14, (struct _D3DKMT_PROPERTIES_PRESENT_DURATION_PLANE *)Pool2, 0);          | 
        break;                                                                                                          | 
      case 5:                                                                                                           | 
        v18 = ProcessBoostedVSyncEligibleProperty(                                                                      | 
                v14,                                                                                                    | 
                (struct _D3DKMT_PROPERTIES_PROCESS_BOOST_VSYNC_ELIGIBLE *)Pool2,                                        | 
                0);                                                                                                     | 
        break;                                                                                                          | 
      default:                                                                                                          | 
        WdLogSingleEntry1(2LL, -1073741811LL, v11, v14);                                                                | 
        WdLogGlobalForLineNumber = 6649;                                                                                | 
        v17 = L"Invalid D3DKMT_PROPERTIES_TYPE, returning 0x%I64x";                                                     | 
        goto LABEL_68;                                                                                                  | 
    }                                                                                                                   | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    v18 = ProcessVrrEligibleProperty(v14, (struct _D3DKMT_PROPERTIES_PROCESS_VRR_ELIGIBLE *)Pool2, 0);                  | 
  }                                                                                                                     | 
  v19 = v18;                                                                                                            | 
  RtlCopyToUser(Src[0], Pool2, v6);                                                                                     | 
  if ( P != v25 && P )                                                                                                  | 
  {                                                                                                                     | 
    ExFreePoolWithTag(P, 0);                                                                                            | 
  }                                                                                                                     | 
  return v19;                                                                                                           | 
}                                                                                                                       | 
252 row(s)
