line
__int64 __fastcall NtEnumerateKey(
        HANDLE Handle,
        unsigned int a2,
        unsigned int a3,
        volatile void *a4,
        SIZE_T Length,
        void *a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  Address = a4;
  v32 = a3;
  v7 = a2;
  Object = a6;
  v33[1] = a2;
  v43 = 0LL;
  v44 = 0LL;
  v38 = 0;
  memset_0(v52, 0, 0x58uLL);
  DWORD1(Parameter) = 0;
  memset(v51, 0, sizeof(v51));
  v9 = 0LL;
  v35 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v51, 0x20000LL);
  }
  v27 = 0;
  v28 = 0;
  memset_0(&v45, 0, 0x40uLL);
  v30 = 0LL;
  v33[0] = 0;
  v37[1] = v37;
  v37[0] = v37;
  CmpInitializeThreadInfo(&v43);
  v36 = 0LL;
  Parameter = 0LL;
  v40 = 0LL;
  v41 = 0LL;
  v42 = 0LL;
  v12 = CmpAcquireShutdownRundown();
  if ( !v12 )
  {
    v13 = -1073741431;
    goto LABEL_39;
  }
  if ( a3 > 2 )
  {
    if ( CmpTraceRoutine )
    {
      if ( Handle )
      {
        PreviousMode = KeGetCurrentThread()->PreviousMode;
        Object = 0LL;
        if ( ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)CmKeyObjectType, PreviousMode, &Object, 0LL) >= 0 )
        {
          v9 = *((_QWORD *)Object + 1);
          ObfDereferenceObject(Object);
        }
      }
    }
    v13 = -1073741811;
    goto LABEL_39;
  }
  v15 = KeGetCurrentThread()->PreviousMode;
  LOBYTE(v11) = v15;
  v13 = CmObReferenceObjectByHandle((_DWORD)Handle, 8, v10, v11, (__int64)&v30, 0LL);
  if ( v13 < 0 )
  {
    goto LABEL_39;
  }
  if ( CmpTraceRoutine && v30 )
  {
    v9 = *((_QWORD *)v30 + 1);
    v35 = v9;
  }
  if ( (_BYTE)v15 == 1 )
  {
    v17 = Length;
    v18 = (unsigned int)Length;
    v19 = Address;
    ProbeForWrite(Address, (unsigned int)Length, 4u);
    v20 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)Object < 0x7FFFFFFF0000LL )
    {
      v20 = (__int64)Object;
    }
    *(_DWORD *)v20 = *(_DWORD *)v20;
  }
  else
  {
    v17 = Length;
    v18 = (unsigned int)Length;
    v19 = Address;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v28 = 1;
  if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
  {
    v45 = v30;
    v46 = a2;
    v47 = v32;
    v48 = v19;
    v49 = v17;
    v50 = Object;
    LOBYTE(v22) = 1;
    v23 = CmpCallCallBacksEx(5, (unsigned int)&v45, 0, v22, 20, (__int64)v30, (__int64)v37);
    if ( v23 < 0 )
    {
      v13 = 0;
      if ( v23 != -1073740541 )
      {
        v13 = v23;
      }
      goto LABEL_38;
    }
    v27 = 1;
  }
  LOBYTE(v16) = v15;
  v13 = CmKeyBodyRemapToVirtualForEnum(&v30, v16, 8LL, &v36);
  if ( v13 < 0 )
  {
    goto LABEL_38;
  }
  v13 = CmpBounceContextStart(v52, Address, v18, v15, 0);
  if ( v13 < 0 )
  {
    goto LABEL_38;
  }
  *((_QWORD *)&Parameter + 1) = v30;
  *(_QWORD *)&v40 = v36;
  *((_QWORD *)&v40 + 1) = __PAIR64__(v32, a2);
  *(_QWORD *)&v41 = v53;
  DWORD2(v41) = v17;
  v42 = v33;
  v13 = KeExpandKernelStackAndCallout(CmEnumerateKeyCallout, &Parameter, 0x4800uLL);
  if ( v13 < 0
    || (v13 = Parameter, (int)Parameter < 0) && (_DWORD)Parameter != -1073741789 && (_DWORD)Parameter != -2147483643 )
  {
LABEL_38:
    v7 = a2;
    goto LABEL_39;
  }
  v24 = v33[0];
  *(_DWORD *)Object = v33[0];
  if ( v13 != -1073741789 )
  {
    if ( v17 < v24 )
    {
      v24 = v17;
    }
    CmpBounceContextCopyDataToCallerBuffer(v52, v24);
  }
  v7 = a2;
LABEL_39:
  if ( v36 )
  {
    ObfDereferenceObject(v36);
  }
  if ( v27 )
  {
    v13 = CmPostCallbackNotificationEx(20, (_DWORD)v30, v13, (unsigned int)&v45, 0LL, (__int64)v37);
  }
  if ( v28 )
  {
    KeLeaveCriticalRegion();
  }
  if ( v30 )
  {
    ObfDereferenceObject(v30);
  }
  CmpBounceContextCleanup(v52);
  if ( CmpTraceRoutine )
  {
    LOBYTE(v25) = 17;
    guard_dispatch_icall_no_overrides(v25, v51, (unsigned int)v13, v7, v9, 0LL);
  }
  if ( v12 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v43);
  return (unsigned int)v13;
}
