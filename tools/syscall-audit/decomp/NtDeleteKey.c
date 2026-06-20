line
__int64 __fastcall NtDeleteKey(HANDLE Handle)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v27 = 0LL;
  v37 = 0LL;
  v36 = 0LL;
  LODWORD(v26) = 0;
  v2 = 0LL;
  memset(v38, 0, sizeof(v38));
  v30 = 0LL;
  v28 = 0LL;
  memset(v40, 0, sizeof(v40));
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v40, 0x20000LL);
  }
  CmpInitializeThreadInfo(&v36);
  Object = 0LL;
  v3 = 0;
  v29[1] = v29;
  v4 = 0;
  v29[0] = v29;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v24 = CmpAcquireShutdownRundown();
  v9 = v24;
  if ( !v24 )
  {
    v11 = -1073741431;
    goto LABEL_38;
  }
  LOBYTE(v8) = PreviousMode;
  v10 = CmObReferenceObjectByHandle((_DWORD)Handle, 0x10000, v7, v8, (__int64)&Object, (__int64)&v27);
  v11 = v10;
  if ( v10 == -1073741790 )
  {
    SeCaptureSubjectContext(&SubjectContext);
    if ( (unsigned __int8)CmDoVirtualTest(&SubjectContext, &v26) )
    {
      LOBYTE(v22) = PreviousMode;
      v11 = CmObReferenceObjectByHandle((_DWORD)Handle, 131097, v21, v22, (__int64)&Object, (__int64)&v27);
      if ( v11 < 0 )
      {
        goto LABEL_68;
      }
      v14 = Object;
      if ( (unsigned __int8)CmKeyBodyNeedsVirtualImage(Object) )
      {
        v12 = 1;
        goto LABEL_6;
      }
    }
    else
    {
      v14 = Object;
    }
    v11 = -1073741790;
    goto LABEL_58;
  }
  v12 = 0;
  if ( v10 >= 0 )
  {
LABEL_6:
    CurrentThread = KeGetCurrentThread();
    v4 = 1;
    v14 = Object;
    --CurrentThread->KernelApcDisable;
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
    {
      *(_QWORD *)&v38[0].Data1 = v14;
      LOBYTE(v15) = 1;
      v16 = CmpCallCallBacksEx(0, (unsigned int)v38, 0, v15, 15, (__int64)v14, (__int64)v29);
      if ( v16 < 0 )
      {
        v11 = 0;
        if ( v16 != -1073740541 )
        {
          v11 = v16;
        }
        goto LABEL_27;
      }
      v3 = 1;
    }
    if ( CmpTraceRoutine && v14 )
    {
      v28 = v14[1];
    }
    v17 = KeAbPreAcquire(&ExpKeyManipLock, 0LL, 0LL);
    if ( _InterlockedCompareExchange64((volatile signed __int64 *)&ExpKeyManipLock, 17LL, 0LL) )
    {
      ExfAcquirePushLockSharedEx(&ExpKeyManipLock, 0LL, v17, &ExpKeyManipLock);
    }
    if ( v17 )
    {
      *(_BYTE *)(v17 + 10) = 1;
    }
    v11 = 0;
    if ( ExpControlKey && *((_QWORD *)Object + 1) == *((_QWORD *)ExpControlKey + 1)
      || qword_140EFEAB8 && *((_QWORD *)Object + 1) == *((_QWORD *)qword_140EFEAB8 + 1) )
    {
      if ( _InterlockedCompareExchange64((volatile signed __int64 *)&ExpKeyManipLock, 0LL, 17LL) != 17 )
      {
        ExfReleasePushLockShared(&ExpKeyManipLock, Object);
      }
      KeAbPostRelease((ULONG_PTR)&ExpKeyManipLock);
      v14 = Object;
      goto LABEL_27;
    }
    if ( _InterlockedCompareExchange64((volatile signed __int64 *)&ExpKeyManipLock, 0LL, 17LL) != 17 )
    {
      ExfReleasePushLockShared(&ExpKeyManipLock, Object);
    }
    KeAbPostRelease((ULONG_PTR)&ExpKeyManipLock);
    v14 = Object;
    v6 = *((_QWORD *)Object + 1);
    if ( (*(_DWORD *)(v6 + 8) & 0x80u) != 0 || (v19 = *(_QWORD *)(v6 + 72)) != 0 && (*(_DWORD *)(v19 + 8) & 0x80u) != 0 )
    {
      v11 = -1073741790;
      goto LABEL_27;
    }
    if ( !v12 )
    {
LABEL_25:
      v11 = CmDeleteKey(v14);
      if ( v11 >= 0 && (v27 & 4) != 0 )
      {
        if ( v14[7] || v14[8] )
        {
          TransactionId = 0LL;
          CmpLockRegistry();
          if ( (int)CmpTransSearchAddTransFromKeyBody(v14, &v30) < 0 )
          {
            CmpUnlockRegistry();
            goto LABEL_27;
          }
          TransactionId = *(GUID *)(v30 + 88);
          CmpUnlockRegistry();
          p_TransactionId = &TransactionId;
        }
        else
        {
          p_TransactionId = 0LL;
        }
        SeDeleteObjectAuditAlarmWithTransaction(v14, Handle, p_TransactionId);
      }
LABEL_27:
      if ( !v12 )
      {
        goto LABEL_28;
      }
      goto LABEL_58;
    }
    LOBYTE(v18) = PreviousMode;
    v11 = CmKeyBodyRemapToVirtual((unsigned int)&Object, v18, 0x10000, (unsigned int)&SubjectContext, (__int64)&v26);
    if ( v11 >= 0 )
    {
      v14 = Object;
      if ( CmpVEEnabled && (*(_DWORD *)(*((_QWORD *)Object + 1) + 184LL) & 0x1000000) != 0 )
      {
        goto LABEL_25;
      }
      v11 = -1073741790;
LABEL_58:
      SeReleaseSubjectContext(&SubjectContext);
LABEL_28:
      if ( v3 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v29[0] != v29 )
      {
        v35 = 0LL;
        v32[2] = v38;
        v33 = (unsigned int)v11;
        v34 = 0LL;
        v32[0] = v14;
        v32[1] = (unsigned int)v11;
        CmpCallCallBacksEx(15, (unsigned int)v32, 0, 0, 15, (__int64)v14, (__int64)v29);
        v11 = v33;
      }
      if ( v4 )
      {
        KeLeaveCriticalRegion();
      }
      goto LABEL_35;
    }
LABEL_68:
    v14 = Object;
    goto LABEL_58;
  }
  v14 = Object;
LABEL_35:
  if ( v14 )
  {
    ObfDereferenceObject(v14);
  }
  v2 = v28;
  v9 = v24;
LABEL_38:
  if ( CmpTraceRoutine )
  {
    LOBYTE(v6) = 12;
    guard_dispatch_icall_no_overrides(v6, v40, (unsigned int)v11, 0LL, v2, 0LL);
  }
  if ( v9 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v36);
  return (unsigned int)v11;
}
