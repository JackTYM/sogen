__int64 __fastcall NtInitiatePowerAction(int a1, int a2, int a3, char a4)                           | 
{                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     | 
                                                                                                    | 
  v20 = 0LL;                                                                                        | 
  Timeout.QuadPart = -1500000000LL;                                                                 | 
  *(_OWORD *)P = 0LL;                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                             | 
  v7 = a2;                                                                                          | 
  v9 = 0LL;                                                                                         | 
  PreviousMode = CurrentThread->PreviousMode;                                                       | 
  if ( PreviousMode )                                                                               | 
  {                                                                                                 | 
    if ( a1 == 7 )                                                                                  | 
    {                                                                                               | 
LABEL_41:                                                                                           | 
      v11 = -1073741811;                                                                            | 
      goto LABEL_42;                                                                                | 
    }                                                                                               | 
    if ( !SeSinglePrivilegeCheck(SeShutdownPrivilege, CurrentThread->PreviousMode) )                | 
    {                                                                                               | 
      v11 = -1073741727;                                                                            | 
      goto LABEL_42;                                                                                | 
    }                                                                                               | 
  }                                                                                                 | 
  if ( v7 > 7 || a1 > 7 || (a3 & 0x10000000) != 0 || a1 == 2 && v7 >= 5 || (a3 & 0xCFFFFC0) != 0 )  | 
  {                                                                                                 | 
    goto LABEL_41;                                                                                  | 
  }                                                                                                 | 
  if ( (unsigned int)(a1 - 4) > 2 && (unsigned __int8)PsIsCurrentThreadInServerSilo() )             | 
  {                                                                                                 | 
    v11 = -1073741637;                                                                              | 
    goto LABEL_42;                                                                                  | 
  }                                                                                                 | 
  v19[2] = 0;                                                                                       | 
  LOBYTE(a2) = PreviousMode;                                                                        | 
  v19[0] = a1;                                                                                      | 
  v19[1] = a3;                                                                                      | 
  DWORD1(v20) = 128;                                                                                | 
  v11 = PoCaptureReasonContext(0, a2, 0, 0, 0LL, (__int64)P);                                       | 
  v12 = P[0];                                                                                       | 
  if ( v11 < 0 )                                                                                    | 
  {                                                                                                 | 
    v12 = 0LL;                                                                                      | 
  }                                                                                                 | 
  P[0] = v12;                                                                                       | 
  if ( !PreviousMode && a1 == 6 && (a3 & 0x3000000) != 0 )                                          | 
  {                                                                                                 | 
    LODWORD(v20) = 15;                                                                              | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    LODWORD(v20) = 4;                                                                               | 
    PopDiagTracePolicyInitiatePowerActionApiCall((unsigned int)a1, (unsigned int)v7);               | 
  }                                                                                                 | 
  if ( !a4 )                                                                                        | 
  {                                                                                                 | 
    Pool2 = ExAllocatePool2(0x40uLL, 0x38uLL, 0x57634150u);                                         | 
    v9 = Pool2;                                                                                     | 
    if ( !Pool2 )                                                                                   | 
    {                                                                                               | 
      v11 = -1073741670;                                                                            | 
      goto LABEL_42;                                                                                | 
    }                                                                                               | 
    *(_DWORD *)(Pool2 + 24) = 0;                                                                    | 
    *(_QWORD *)(Pool2 + 48) = &v20;                                                                 | 
    KeInitializeEvent((PRKEVENT)Pool2, NotificationEvent, 0);                                       | 
    DWORD1(v20) |= 0x20u;                                                                           | 
    *((_QWORD *)&v20 + 1) = v9;                                                                     | 
  }                                                                                                 | 
  if ( (unsigned int)(a1 - 4) <= 2 && (BYTE8(PopBsdPowerTransition) & 8) == 0 )                     | 
  {                                                                                                 | 
    PopAcquireRwLockExclusive(&PopBsdUpdateLock);                                                   | 
    BYTE8(PopBsdPowerTransition) |= 8u;                                                             | 
    LOBYTE(PopBsdPowerTransitionExtension) = dword_140F0B054;                                       | 
    PopBsdHandleRequest(11LL);                                                                      | 
    PopReleaseRwLock((ULONG_PTR)&PopBsdUpdateLock);                                                 | 
  }                                                                                                 | 
  PopAcquirePolicyLock();                                                                           | 
  PopExecutePowerAction((unsigned int)&v20, 0, (unsigned int)v19, v7, 1);                           | 
  PopReleasePolicyLock();                                                                           | 
  PopThermalEventTransitionEnableDeepSleep((unsigned int)a1);                                       | 
  if ( v9 )                                                                                         | 
  {                                                                                                 | 
    v14 = v9 + 32;                                                                                  | 
    if ( *(_QWORD *)(v9 + 32) )                                                                     | 
    {                                                                                               | 
      v11 = KeWaitForSingleObject((PVOID)v9, Suspended, 0, 1u, &Timeout);                           | 
      if ( v11 == 258 && (byte_140F0B041 & 3) != 0 )                                                | 
      {                                                                                             | 
        v11 = KeWaitForSingleObject((PVOID)v9, Suspended, 0, 1u, 0LL);                              | 
      }                                                                                             | 
      PopAcquirePolicyLock();                                                                       | 
      v15 = *(_QWORD *)v14;                                                                         | 
      if ( *(_QWORD *)(*(_QWORD *)v14 + 8LL) != v14 || (v16 = *(_QWORD **)(v9 + 40), *v16 != v14) ) | 
      {                                                                                             | 
        __fastfail(3u);                                                                             | 
      }                                                                                             | 
      *v16 = v15;                                                                                   | 
      *(_QWORD *)(v15 + 8) = v16;                                                                   | 
      PopReleasePolicyLock();                                                                       | 
    }                                                                                               | 
    if ( v11 >= 0 )                                                                                 | 
    {                                                                                               | 
      v11 = *(_DWORD *)(v9 + 24);                                                                   | 
      if ( v11 >= 0 )                                                                               | 
      {                                                                                             | 
        v11 = 0;                                                                                    | 
      }                                                                                             | 
    }                                                                                               | 
    ExFreePoolWithTag((PVOID)v9, 0);                                                                | 
  }                                                                                                 | 
LABEL_42:                                                                                           | 
  if ( P[0] )                                                                                       | 
  {                                                                                                 | 
    PoDestroyReasonContext(P[0]);                                                                   | 
  }                                                                                                 | 
  return (unsigned int)v11;                                                                         | 
}                                                                                                   | 
119 row(s)
