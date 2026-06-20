line
__int64 __fastcall NtSetInformationKey(void *a1, unsigned int a2, const void *a3, int a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v35 = a4;
  Handle = a1;
  v51 = 0LL;
  v52 = 0LL;
  v33 = 0LL;
  memset(v53, 0, sizeof(v53));
  v34 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v53, 0x20000LL);
  }
  v6 = 0;
  v7 = 0LL;
  Object = 0LL;
  v28 = 0;
  v39 = 0LL;
  v40 = 0LL;
  v41 = 0LL;
  v42 = 0LL;
  v37[1] = v37;
  v37[0] = v37;
  CmpInitializeThreadInfo(&v51);
  v36 = 0;
  v8 = 0;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  v9 = CmpAcquireShutdownRundown();
  v31 = v9;
  if ( !v9 )
  {
    v13 = -1073741431;
    goto LABEL_39;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v29 = PreviousMode;
  if ( a2 == 1 )
  {
    goto LABEL_5;
  }
  if ( a2 )
  {
    if ( a2 != 2 && a2 != 3 )
    {
      if ( a2 == 4 )
      {
LABEL_5:
        v27 = 1;
LABEL_6:
        v11 = 4;
        goto LABEL_7;
      }
      if ( a2 != 5 )
      {
        if ( CmpTraceRoutine )
        {
          v23 = Handle;
          if ( Handle )
          {
            v24 = KeGetCurrentThread()->PreviousMode;
            Handle = 0LL;
            if ( ObReferenceObjectByHandle(v23, 0, (POBJECT_TYPE)CmKeyObjectType, v24, &Handle, 0LL) >= 0 )
            {
              v34 = *((_QWORD *)Handle + 1);
              ObfDereferenceObject(Handle);
            }
          }
        }
        v13 = -1073741821;
        v7 = Object;
        v18 = 0;
        goto LABEL_23;
      }
    }
    v27 = 0;
    goto LABEL_6;
  }
  v11 = 8;
  v27 = 1;
LABEL_7:
  if ( v35 != v11 )
  {
    if ( CmpTraceRoutine )
    {
      v25 = Handle;
      if ( Handle )
      {
        v26 = KeGetCurrentThread()->PreviousMode;
        Handle = 0LL;
        if ( ObReferenceObjectByHandle(v25, 0, (POBJECT_TYPE)CmKeyObjectType, v26, &Handle, 0LL) >= 0 )
        {
          v34 = *((_QWORD *)Handle + 1);
          ObfDereferenceObject(Handle);
        }
      }
    }
    v13 = -1073741820;
    v7 = Object;
    v18 = 0;
    goto LABEL_23;
  }
  memmove(&v33, a3, v11);
  Object = 0LL;
  v12 = 0;
  if ( a2 != 5 )
  {
    v12 = 2;
  }
  v13 = ObReferenceObjectByHandle(Handle, v12, (POBJECT_TYPE)CmKeyObjectType, PreviousMode, &Object, 0LL);
  v7 = Object;
  v14 = Object;
  if ( v13 != -1073741790 )
  {
    v15 = 0;
    goto LABEL_12;
  }
  if ( !v27 )
  {
    v13 = -1073741790;
    v18 = 0;
    goto LABEL_23;
  }
  SeCaptureSubjectContext(&SubjectContext);
  v8 = 1;
  if ( !CmpVEEnabled )
  {
LABEL_76:
    v13 = -1073741790;
    v18 = 0;
    goto LABEL_23;
  }
  if ( (unsigned __int8)CmpIsSystemEntity((unsigned __int8)KeGetCurrentThread()->PreviousMode, &SubjectContext, &v36) )
  {
    v7 = Object;
    goto LABEL_76;
  }
  Object = 0LL;
  v13 = ObReferenceObjectByHandle(Handle, 0x20019u, (POBJECT_TYPE)CmKeyObjectType, v29, &Object, 0LL);
  v7 = Object;
  if ( v13 < 0 )
  {
    goto LABEL_39;
  }
  if ( !(unsigned __int8)CmKeyBodyNeedsVirtualImage(Object) )
  {
    v13 = -1073741790;
    v18 = 0;
    goto LABEL_23;
  }
  v13 = 0;
  v15 = 1;
  v28 = 1;
  v14 = v7;
LABEL_12:
  v8 = v15;
  if ( v13 < 0 )
  {
LABEL_39:
    v18 = 0;
    goto LABEL_23;
  }
  if ( CmpTraceRoutine && v14 )
  {
    v34 = *((_QWORD *)v7 + 1);
  }
  if ( a2 == 5 )
  {
LABEL_15:
    CurrentThread = KeGetCurrentThread();
    --CurrentThread->KernelApcDisable;
    if ( CmpCallBackCount )
    {
      if ( !(unsigned int)CmpIsRegistryLockAcquired() )
      {
        v7 = Object;
        *(_QWORD *)&v39 = Object;
        DWORD2(v39) = a2;
        *(_QWORD *)&v40 = &v33;
        DWORD2(v40) = v35;
        LOBYTE(v17) = 1;
        v13 = CmpCallCallBacksEx(3, (unsigned int)&v39, 0, v17, 18, (__int64)Object, (__int64)v37);
        if ( v13 < 0 )
        {
          v20 = 0;
          if ( v13 != -1073740541 )
          {
            v20 = v13;
          }
          v8 = v28;
          v13 = v20;
          goto LABEL_22;
        }
        v6 = 1;
        v15 = v28;
LABEL_19:
        if ( !v15
          || (v13 = CmKeyBodyReplicateToVirtual(&Object, v29, 2LL, &SubjectContext, &v36),
              v15 = v28,
              v8 = v28,
              v7 = Object,
              v13 >= 0) )
        {
          v8 = v15;
          if ( a2 == 5 )
          {
            *((_WORD *)v7 + 25) = v33;
            v13 = 0;
          }
          else if ( a2 )
          {
            if ( a2 == 1 || a2 == 2 || a2 - 3 <= 1 )
            {
              v8 = v28;
              v13 = CmSetKeyFlags(v7, a2, (unsigned int)v33);
            }
          }
          else
          {
            v8 = v28;
            v13 = CmSetLastWriteTimeKey(v7, &v33);
          }
        }
LABEL_22:
        v18 = 1;
        goto LABEL_23;
      }
      v15 = v28;
    }
    v7 = Object;
    goto LABEL_19;
  }
  v22 = *((_QWORD *)v7 + 1);
  if ( (*(_DWORD *)(v22 + 8) & 0x80u) == 0 )
  {
    if ( (*(_DWORD *)(*(_QWORD *)(v22 + 32) + 160LL) & 0x100000) == 0 )
    {
      goto LABEL_15;
    }
    v13 = -1073741790;
    v18 = 0;
  }
  else
  {
    v13 = -1073741790;
    v18 = 0;
  }
LABEL_23:
  if ( v8 )
  {
    SeReleaseSubjectContext(&SubjectContext);
  }
  if ( v6 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v37[0] != v37 )
  {
    v45 = 0;
    v48 = 0LL;
    v49 = 0LL;
    v50 = 0;
    v43 = v7;
    v44 = v13;
    v47 = v13;
    v46 = &v39;
    CmpCallCallBacksEx(18, (unsigned int)&v43, 0, 0, 18, (__int64)v7, (__int64)v37);
    v13 = v47;
  }
  if ( v18 )
  {
    KeLeaveCriticalRegionThread(KeGetCurrentThread());
    v7 = Object;
  }
  if ( v7 )
  {
    ObfDereferenceObject(v7);
  }
  if ( CmpTraceRoutine )
  {
    v21 = v34;
    LOBYTE(v21) = 20;
    if ( (__int64 (__fastcall *)(_DWORD, _DWORD, _DWORD, _DWORD, __int64, __int64))CmpTraceRoutine == EtwpTraceRegistry )
    {
      EtwpTraceRegistry(v21, (unsigned int)v53, v13, 0, v34, 0LL);
    }
    else
    {
      guard_dispatch_icall_no_overrides(v21, v53, (unsigned int)v13, 0LL, v34, 0LL);
    }
  }
  if ( v9 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v51);
  return (unsigned int)v13;
}
