__int64 __fastcall NtSaveMergedKeys(__int64 a1, __int64 a2, void *a3)                      | 
{                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                            | 
                                                                                           | 
  v24 = a1;                                                                                | 
  v25 = a2;                                                                                | 
  v3 = 0LL;                                                                                | 
  v27 = 0LL;                                                                               | 
  v4 = 0LL;                                                                                | 
  v31 = 0LL;                                                                               | 
  v5 = 0LL;                                                                                | 
  v21 = 0LL;                                                                               | 
  v26 = 0LL;                                                                               | 
  v22 = 0LL;                                                                               | 
  memset(&ApcState, 0, sizeof(ApcState));                                                  | 
  v20 = 0LL;                                                                               | 
  v7 = 0;                                                                                  | 
  v28 = 0LL;                                                                               | 
  v29 = 0LL;                                                                               | 
  v30 = 0LL;                                                                               | 
  CmpInitializeThreadInfo(&v26);                                                           | 
  v23[0] = v23;                                                                            | 
  v23[1] = v23;                                                                            | 
  if ( !(unsigned __int8)CmpAcquireShutdownRundown() )                                     | 
  {                                                                                        | 
    v8 = -1073741431;                                                                      | 
    goto LABEL_14;                                                                         | 
  }                                                                                        | 
  v8 = CmCheckNoTxContext();                                                               | 
  if ( v8 >= 0 )                                                                           | 
  {                                                                                        | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                     | 
    if ( !SeSinglePrivilegeCheck(SeBackupPrivilege, PreviousMode) )                        | 
    {                                                                                      | 
      v8 = -1073741727;                                                                    | 
LABEL_6:                                                                                   | 
      v5 = 0LL;                                                                            | 
      v4 = v22;                                                                            | 
      v3 = v21;                                                                            | 
      goto LABEL_7;                                                                        | 
    }                                                                                      | 
    LOBYTE(v11) = PreviousMode;                                                            | 
    v8 = CmObReferenceObjectByHandle(v24, 0, v10, v11, (__int64)&v21, 0LL);                | 
    if ( v8 < 0 )                                                                          | 
    {                                                                                      | 
      goto LABEL_6;                                                                        | 
    }                                                                                      | 
    LOBYTE(v14) = PreviousMode;                                                            | 
    v8 = CmObReferenceObjectByHandle(v25, 0, v13, v14, (__int64)&v22, 0LL);                | 
    if ( v8 < 0 )                                                                          | 
    {                                                                                      | 
      goto LABEL_6;                                                                        | 
    }                                                                                      | 
    if ( PreviousMode == 1 )                                                               | 
    {                                                                                      | 
      LOBYTE(v15) = 1;                                                                     | 
      v8 = IoConvertFileHandleToKernelHandle(a3, v15, 2LL);                                | 
      if ( v8 < 0 )                                                                        | 
      {                                                                                    | 
        goto LABEL_6;                                                                      | 
      }                                                                                    | 
    }                                                                                      | 
    else                                                                                   | 
    {                                                                                      | 
      v20 = a3;                                                                            | 
    }                                                                                      | 
    CurrentThread = KeGetCurrentThread();                                                  | 
    v3 = v21;                                                                              | 
    v4 = v22;                                                                              | 
    v5 = v20;                                                                              | 
    --CurrentThread->KernelApcDisable;                                                     | 
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )                  | 
    {                                                                                      | 
      *(_QWORD *)&v29 = v3;                                                                | 
      *((_QWORD *)&v29 + 1) = v4;                                                          | 
      LOBYTE(v17) = 1;                                                                     | 
      *((_QWORD *)&v28 + 1) = v20;                                                         | 
      v18 = CmpCallCallBacksEx(49, (unsigned int)&v28, 0, v17, 50, 0LL, (__int64)v23);     | 
      if ( v18 < 0 )                                                                       | 
      {                                                                                    | 
        v8 = 0;                                                                            | 
        if ( v18 != -1073740541 )                                                          | 
        {                                                                                  | 
          v8 = v18;                                                                        | 
        }                                                                                  | 
LABEL_30:                                                                                  | 
        KeLeaveCriticalRegion();                                                           | 
        goto LABEL_7;                                                                      | 
      }                                                                                    | 
      v7 = 1;                                                                              | 
    }                                                                                      | 
    CmpAttachToRegistryProcess(&ApcState);                                                 | 
    LOBYTE(v19) = PreviousMode;                                                            | 
    v8 = CmSaveMergedKeys(v3, v4, v20, v19);                                               | 
    CmpDetachFromRegistryProcess(&ApcState);                                               | 
    if ( v7 )                                                                              | 
    {                                                                                      | 
      v8 = CmPostCallbackNotificationEx(50, 0, v8, (unsigned int)&v28, 0LL, (__int64)v23); | 
    }                                                                                      | 
    goto LABEL_30;                                                                         | 
  }                                                                                        | 
LABEL_7:                                                                                   | 
  CmpReleaseShutdownRundown();                                                             | 
  if ( v4 )                                                                                | 
  {                                                                                        | 
    ObfDereferenceObject(v4);                                                              | 
  }                                                                                        | 
  if ( v3 )                                                                                | 
  {                                                                                        | 
    ObfDereferenceObject(v3);                                                              | 
  }                                                                                        | 
  if ( v5 && v5 != a3 )                                                                    | 
  {                                                                                        | 
    ZwClose(v5);                                                                           | 
  }                                                                                        | 
LABEL_14:                                                                                  | 
  CmpCleanupThreadInfo(&v26);                                                              | 
  return (unsigned int)v8;                                                                 | 
}                                                                                          | 
119 row(s)
