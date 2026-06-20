line
__int64 __fastcall NtFlushKey(int a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v19 = 0LL;
  v1 = 0LL;
  v26 = 0LL;
  v25 = 0LL;
  memset(v27, 0, sizeof(v27));
  memset(&ApcState, 0, sizeof(ApcState));
  memset(v29, 0, sizeof(v29));
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v29, 0x20000LL);
  }
  v3 = 0LL;
  v18[1] = v18;
  Object = 0LL;
  v18[0] = v18;
  v4 = 0;
  CmpInitializeThreadInfo(&v25);
  v8 = CmpAcquireShutdownRundown();
  if ( v8 )
  {
    PreviousMode = KeGetCurrentThread()->PreviousMode;
    LOBYTE(v7) = PreviousMode;
    v10 = CmObReferenceObjectByHandle(a1, 0, v6, v7, (__int64)&Object, (__int64)&v19);
    if ( v10 >= 0 )
    {
      if ( CmpTraceRoutine && Object )
      {
        v1 = *((_QWORD *)Object + 1);
      }
      CurrentThread = KeGetCurrentThread();
      v3 = (ULONG_PTR *)Object;
      --CurrentThread->KernelApcDisable;
      if ( (*(_DWORD *)(*(_QWORD *)(v3[1] + 32) + 160LL) & 2) != 0 && PreviousMode && (~HIDWORD(v19) & 0x20006) != 0 )
      {
        v10 = -1073741790;
LABEL_15:
        KeLeaveCriticalRegion();
        goto LABEL_16;
      }
      if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
      {
        *(_QWORD *)&v27[0] = v3;
        LOBYTE(v16) = 1;
        v15 = CmpCallCallBacksEx(30, (unsigned int)v27, 0, v16, 31, (__int64)v3, (__int64)v18);
        if ( v15 < 0 )
        {
          v10 = 0;
          if ( v15 != -1073740541 )
          {
            v10 = v15;
          }
          goto LABEL_15;
        }
        v4 = 1;
      }
      CmpLockRegistryFreezeAware(0LL);
      CmpLockKcbShared(v3[1]);
      v10 = CmpPerformKeyBodyDeletionCheck(v3, 0LL);
      if ( v10 >= 0 )
      {
        v12 = v3[1];
        v13 = *(struct _EX_RUNDOWN_REF **)(v12 + 32);
        if ( v13 == (struct _EX_RUNDOWN_REF *)CmpMasterHive )
        {
          CmpUnlockKcb(v12);
          CmpUnlockRegistry();
          CmpAttachToRegistryProcess(&ApcState);
          CmpDoFlushAll(0LL);
          CmpDetachFromRegistryProcess(&ApcState);
          v10 = 0;
LABEL_14:
          if ( v4 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v18[0] != v18 )
          {
            v24 = 0LL;
            v22 = 0;
            v20[2] = v27;
            v20[0] = v3;
            v23 = 0LL;
            v20[1] = (unsigned int)v10;
            v21 = v10;
            CmpCallCallBacksEx(31, (unsigned int)v20, 0, 0, 31, (__int64)v3, (__int64)v18);
            v10 = v21;
          }
          goto LABEL_15;
        }
        if ( ExAcquireRundownProtection_0(v13 + 205) )
        {
          CmpUnlockKcb(v3[1]);
          CmpUnlockRegistry();
          CmpAttachToRegistryProcess(&ApcState);
          v10 = CmpFlushHive(v13, 0LL);
          if ( v10 < 0 )
          {
            v10 = -1073741491;
          }
          CmpDetachFromRegistryProcess(&ApcState);
          ExReleaseRundownProtection_0(v13 + 205);
          goto LABEL_14;
        }
        v10 = -1073740763;
      }
      CmpUnlockKcb(v3[1]);
      CmpUnlockRegistry();
      goto LABEL_14;
    }
    v3 = (ULONG_PTR *)Object;
  }
  else
  {
    v10 = -1073741431;
  }
LABEL_16:
  if ( v3 )
  {
    ObfDereferenceObject(v3);
  }
  if ( CmpTraceRoutine )
  {
    LOBYTE(v5) = 21;
    guard_dispatch_icall_no_overrides(v5, v29, (unsigned int)v10, 0LL, v1, 0LL);
  }
  if ( v8 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v25);
  return (unsigned int)v10;
}
