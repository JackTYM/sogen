__int64 __fastcall NtSetThreadExecutionState(int a1, _DWORD *a2)                                                  | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  v2 = a2;                                                                                                        | 
  v18 = 0;                                                                                                        | 
  P = 0LL;                                                                                                        | 
  LOBYTE(a2) = KeGetCurrentThread()->PreviousMode;                                                                | 
  v4 = 0;                                                                                                         | 
  v5 = 0;                                                                                                         | 
  if ( !(_BYTE)a2 )                                                                                               | 
  {                                                                                                               | 
    return (unsigned int)-1073741637;                                                                             | 
  }                                                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                                           | 
  v17 = CurrentThread;                                                                                            | 
  if ( (a1 & 0x7FFFFFBC) != 0 || (a1 & 0x80000040) == 0x40 )                                                      | 
  {                                                                                                               | 
    return (unsigned int)-1073741811;                                                                             | 
  }                                                                                                               | 
  v8 = 0x7FFFFFFF0000LL;                                                                                          | 
  if ( (unsigned __int64)v2 < 0x7FFFFFFF0000LL )                                                                  | 
  {                                                                                                               | 
    v8 = (__int64)v2;                                                                                             | 
  }                                                                                                               | 
  *(_DWORD *)v8 = *(_DWORD *)v8;                                                                                  | 
  Blink = CurrentThread[1].ApcState.ApcListHead[1].Blink;                                                         | 
  v16 = Blink;                                                                                                    | 
  if ( Blink || a1 >= 0 )                                                                                         | 
  {                                                                                                               | 
LABEL_15:                                                                                                         | 
    PopGetLegacyPowerRequestFlags(Blink, (unsigned int)a1, &v18);                                                 | 
    *v2 = v18;                                                                                                    | 
    if ( a1 < 0 )                                                                                                 | 
    {                                                                                                             | 
      PopApplyLegacyPowerRequestFlags(Blink);                                                                     | 
      return 0;                                                                                                   | 
    }                                                                                                             | 
    if ( !(unsigned int)Feature_ExpandSTEIgnoreReasons__private_IsEnabledDeviceUsageNoInline() )                  | 
    {                                                                                                             | 
LABEL_29:                                                                                                         | 
      PopDiagTraceSetThreadExecutionState(CurrentThread, (unsigned int)a1, v4, v5);                               | 
      if ( (unsigned int)Feature_ExpandSTEIgnoreReasons__private_IsEnabledDeviceUsageNoInline() )                 | 
      {                                                                                                           | 
        if ( (v4 & 1) != 0 )                                                                                      | 
        {                                                                                                         | 
          PopAcquirePolicyLock();                                                                                 | 
          tlgDefineProvider_annotation__TlgMiTraceHandleProv();                                                   | 
          PopReleasePolicyLock();                                                                                 | 
        }                                                                                                         | 
        if ( (v4 & 2) == 0 )                                                                                      | 
        {                                                                                                         | 
          return 0;                                                                                               | 
        }                                                                                                         | 
        PoEnergyContextUpdateComponentPower(KeGetCurrentThread()->ApcState.Process, 12LL, 3LL);                   | 
        SessionId = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);                                     | 
        if ( !(unsigned __int8)SessionIsInteractive(SessionId) )                                                  | 
        {                                                                                                         | 
          return 0;                                                                                               | 
        }                                                                                                         | 
      }                                                                                                           | 
      else                                                                                                        | 
      {                                                                                                           | 
        if ( (a1 & 1) != 0 )                                                                                      | 
        {                                                                                                         | 
          PopAcquirePolicyLock();                                                                                 | 
          tlgDefineProvider_annotation__TlgMiTraceHandleProv();                                                   | 
          PopReleasePolicyLock();                                                                                 | 
        }                                                                                                         | 
        if ( (a1 & 2) == 0 )                                                                                      | 
        {                                                                                                         | 
          return 0;                                                                                               | 
        }                                                                                                         | 
        PoEnergyContextUpdateComponentPower(KeGetCurrentThread()->ApcState.Process, 12LL, 3LL);                   | 
        PopAcquirePolicyLock();                                                                                   | 
        v13 = (unsigned __int8)SSHSupportIsPlatformAoAc() && !PopLidOpened && !PopConsoleExternalDisplayConnected | 
           || (unsigned __int8)PopAdaptiveGetBootIsSystemInitiated() != 0;                                        | 
        PopReleasePolicyLock();                                                                                   | 
        v14 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);                                           | 
        if ( !(unsigned __int8)SessionIsInteractive(v14) || v13 )                                                 | 
        {                                                                                                         | 
          return 0;                                                                                               | 
        }                                                                                                         | 
      }                                                                                                           | 
      LOBYTE(v12) = 1;                                                                                            | 
      PopNotifyConsoleUserPresent(v12, 8LL);                                                                      | 
      return 0;                                                                                                   | 
    }                                                                                                             | 
    v4 = a1;                                                                                                      | 
    if ( (int)PopGetCurrentPdcPhase() >= 2 )                                                                      | 
    {                                                                                                             | 
      v5 = 1;                                                                                                     | 
      v4 = a1 & 0xFFFFFFFC;                                                                                       | 
      goto LABEL_29;                                                                                              | 
    }                                                                                                             | 
    if ( (a1 & 2) == 0 )                                                                                          | 
    {                                                                                                             | 
      goto LABEL_29;                                                                                              | 
    }                                                                                                             | 
    PopAcquirePolicyLock();                                                                                       | 
    if ( !(unsigned __int8)SSHSupportIsPlatformAoAc() || PopLidOpened || PopConsoleExternalDisplayConnected )     | 
    {                                                                                                             | 
      if ( !(unsigned __int8)PopAdaptiveGetBootIsSystemInitiated() )                                              | 
      {                                                                                                           | 
LABEL_28:                                                                                                         | 
        PopReleasePolicyLock();                                                                                   | 
        goto LABEL_29;                                                                                            | 
      }                                                                                                           | 
      v5 = 3;                                                                                                     | 
    }                                                                                                             | 
    else                                                                                                          | 
    {                                                                                                             | 
      v5 = 2;                                                                                                     | 
    }                                                                                                             | 
    v4 = a1 & 0xFFFFFFFD;                                                                                         | 
    goto LABEL_28;                                                                                                | 
  }                                                                                                               | 
  v6 = PoCaptureReasonContext(0, (_DWORD)a2, 0, 1, 0LL, (__int64)&P);                                             | 
  if ( v6 < 0 )                                                                                                   | 
  {                                                                                                               | 
    return (unsigned int)v6;                                                                                      | 
  }                                                                                                               | 
  v10 = P;                                                                                                        | 
  v6 = PopPowerRequestCreateCommon(P, 0LL, &v16);                                                                 | 
  if ( v6 >= 0 )                                                                                                  | 
  {                                                                                                               | 
    Blink = v16;                                                                                                  | 
    CurrentThread = v17;                                                                                          | 
    v17[1].ApcState.ApcListHead[1].Blink = v16;                                                                   | 
    goto LABEL_15;                                                                                                | 
  }                                                                                                               | 
  if ( v10 )                                                                                                      | 
  {                                                                                                               | 
    PoDestroyReasonContext(v10);                                                                                  | 
  }                                                                                                               | 
  return (unsigned int)v6;                                                                                        | 
}                                                                                                                 | 
