line
__int64 __fastcall NtRestoreKey(int a1, void *a2, unsigned int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  Object = 0LL;
  Handle = 0LL;
  v24 = 0LL;
  v22[1] = v22;
  v6 = 0;
  v22[0] = v22;
  v23 = 0LL;
  v25 = 0LL;
  v26 = 0LL;
  v27 = 0LL;
  memset(&ApcState, 0, sizeof(ApcState));
  CmpInitializeThreadInfo(&v23);
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( (unsigned __int8)CmpAcquireShutdownRundown() )
  {
    v8 = CmCheckNoTxContext();
    if ( v8 < 0 )
    {
LABEL_28:
      CmpReleaseShutdownRundown();
      goto LABEL_29;
    }
    if ( !SeSinglePrivilegeCheck(SeRestorePrivilege, PreviousMode) )
    {
      v8 = -1073741727;
      goto LABEL_28;
    }
    if ( PreviousMode == 1 )
    {
      LOBYTE(v9) = 1;
      v12 = 0LL;
      v8 = IoConvertFileHandleToKernelHandle(a2, v9);
      if ( v8 < 0 )
      {
LABEL_25:
        if ( v12 && v12 != a2 )
        {
          ZwClose(v12);
        }
        goto LABEL_28;
      }
    }
    else
    {
      v12 = a2;
      Handle = a2;
    }
    LOBYTE(v11) = PreviousMode;
    v13 = CmObReferenceObjectByHandle(a1, 0, v10, v11, (__int64)&Object, 0LL);
    v14 = Object;
    v8 = v13;
    if ( v13 < 0 )
    {
LABEL_23:
      if ( v14 )
      {
        ObfDereferenceObject(v14);
      }
      goto LABEL_25;
    }
    if ( (*(_DWORD *)(*((_QWORD *)Object + 1) + 8LL) & 0x80u) != 0 )
    {
      v8 = -1073741790;
      goto LABEL_23;
    }
    CurrentThread = KeGetCurrentThread();
    v14 = Object;
    v12 = Handle;
    --CurrentThread->KernelApcDisable;
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
    {
      *(_QWORD *)&v25 = v14;
      *((_QWORD *)&v25 + 1) = Handle;
      LOBYTE(v16) = 1;
      LODWORD(v26) = a3;
      v17 = CmpCallCallBacksEx(41, (unsigned int)&v25, 0, v16, 42, 0LL, (__int64)v22);
      if ( v17 < 0 )
      {
        v8 = 0;
        if ( v17 != -1073740541 )
        {
          v8 = v17;
        }
LABEL_22:
        KeLeaveCriticalRegion();
        goto LABEL_23;
      }
      v6 = 1;
    }
    CmpAttachToRegistryProcess(&ApcState);
    LOBYTE(v18) = PreviousMode;
    v8 = CmRestoreKey(v14, Handle, a3, v18);
    CmpDetachFromRegistryProcess(&ApcState);
    if ( v6 )
    {
      v8 = CmPostCallbackNotificationEx(42, (_DWORD)v14, v8, (unsigned int)&v25, 0LL, (__int64)v22);
    }
    goto LABEL_22;
  }
  v8 = -1073741431;
LABEL_29:
  CmpCleanupThreadInfo(&v23);
  return (unsigned int)v8;
}
