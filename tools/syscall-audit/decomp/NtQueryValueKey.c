line
__int64 __fastcall NtQueryValueKey(
        HANDLE Handle,
        UNICODE_STRING *a2,
        unsigned int a3,
        __int64 a4,
        unsigned int a5,
        void *a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v7 = a3;
  Object = a6;
  v39[2] = a3;
  v63 = 0LL;
  v64 = 0LL;
  DestinationString = 0LL;
  v48 = 0;
  memset(v67, 0, sizeof(v67));
  v68 = 0;
  v69 = 0;
  v70 = 0;
  Parameter = 0LL;
  v44 = 0LL;
  v45 = 0LL;
  v46 = 0LL;
  LODWORD(v47) = 0;
  memset(v71, 0, sizeof(v71));
  v10 = 0LL;
  v40 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v71, 0x20000LL);
  }
  v32 = 0;
  v33 = 0;
  v36 = 0LL;
  v39[0] = 0;
  RtlInitUnicodeString(&DestinationString, 0LL);
  Privileges = 0LL;
  v50 = 0LL;
  v51 = 0LL;
  v52 = 0LL;
  v53 = 0LL;
  v54 = 0LL;
  *(_OWORD *)Entry = 0LL;
  v66 = 0;
  v42[1] = v42;
  v42[0] = v42;
  CmpInitializeThreadInfo(&v63);
  v41 = 0LL;
  v12 = CmpAcquireShutdownRundown();
  if ( !v12 )
  {
    v13 = -1073741431;
    v31 = -1073741431;
    goto LABEL_69;
  }
  if ( v7 > 4 )
  {
    if ( CmpTraceRoutine )
    {
      if ( Handle )
      {
        PreviousMode = KeGetCurrentThread()->PreviousMode;
        Object = 0LL;
        if ( ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)CmKeyObjectType, PreviousMode, &Object, 0LL) >= 0 )
        {
          v10 = *((_QWORD *)Object + 1);
          ObfDereferenceObject(Object);
        }
      }
    }
    v13 = -1073741811;
    v31 = -1073741811;
    goto LABEL_69;
  }
  v15 = KeGetCurrentThread()->PreviousMode;
  v13 = CmObReferenceObjectByHandle((_DWORD)Handle, 1, v11, (unsigned __int8)v15, (__int64)&v36, 0LL);
  v31 = v13;
  if ( v13 < 0 )
  {
    goto LABEL_68;
  }
  if ( CmpTraceRoutine && v36 )
  {
    v10 = *((_QWORD *)v36 + 1);
    v40 = v10;
  }
  if ( (_BYTE)v15 == 1 )
  {
    v49 = 0LL;
    v16 = 0x7FFFFFFF0000LL;
    v17 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      v17 = (__int64)a2;
    }
    v49.m128i_i32[0] = *(_DWORD *)v17;
    v18 = *(_QWORD *)(v17 + 8);
    v49.m128i_i64[1] = v18;
    DestinationString = (UNICODE_STRING)v49;
    if ( (unsigned __int16)_mm_cvtsi128_si32(v49) && (v18 & 1) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v19 = a5;
    if ( a5 && (a4 & 3) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v20 = Object;
    if ( (unsigned __int64)Object < 0x7FFFFFFF0000LL )
    {
      v16 = (__int64)Object;
    }
    *(_DWORD *)v16 = *(_DWORD *)v16;
  }
  else
  {
    DestinationString = *a2;
    v19 = a5;
    v20 = Object;
  }
  DestinationString.MaximumLength = DestinationString.Length;
  if ( (unsigned __int8)CmpDoesBufferRequireCapturing(v15, DestinationString.Buffer) )
  {
    if ( (_WORD)v23 )
    {
      if ( (unsigned int)v23 > 0x40 )
      {
        Privileges = (PPRIVILEGE_SET)CmpAllocateTransientPoolWithQuota(v22, v23, 1853246787LL);
        if ( !Privileges )
        {
          v13 = -1073741670;
          v31 = -1073741670;
          v7 = a3;
          goto LABEL_69;
        }
        Buffer = DestinationString.Buffer;
        LOWORD(v23) = DestinationString.Length;
      }
      else
      {
        Privileges = (PPRIVILEGE_SET)v72;
      }
      if ( Privileges )
      {
        memmove(Privileges, Buffer, (unsigned __int16)v23);
        LOWORD(v23) = DestinationString.Length;
      }
    }
    else
    {
      Privileges = 0LL;
    }
    Buffer = Privileges;
    DestinationString.Buffer = (wchar_t *)Privileges;
  }
  if ( (v23 & 1) != 0 )
  {
    v13 = -1073741811;
    v31 = -1073741811;
    v7 = a3;
    goto LABEL_69;
  }
  while ( (_WORD)v23 && !*((_WORD *)Buffer + ((unsigned __int64)(unsigned __int16)v23 >> 1) - 1) )
  {
    LOWORD(v23) = v23 - 2;
    DestinationString.Length = v23;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v33 = 1;
  if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
  {
    *(_QWORD *)&v50 = v36;
    *((_QWORD *)&v50 + 1) = &DestinationString;
    LODWORD(v51) = a3;
    *((_QWORD *)&v51 + 1) = a4;
    LODWORD(v52) = v19;
    *((_QWORD *)&v52 + 1) = v20;
    LOBYTE(v25) = 1;
    v26 = CmpCallCallBacksEx(8, (unsigned int)&v50, 0, v25, 23, (__int64)v36, (__int64)v42);
    if ( v26 < 0 )
    {
      v13 = 0;
      if ( v26 != -1073740541 )
      {
        v13 = v26;
      }
      v31 = v13;
      goto LABEL_68;
    }
    v32 = 1;
  }
  v13 = CmKeyBodyRemapToVirtualForEnum(&v36, (unsigned __int8)v15, 1LL, &v41);
  v31 = v13;
  if ( v13 < 0 || (v13 = CmpBounceContextStart(Entry, a4, v19, v15, 3), v31 = v13, v13 < 0) )
  {
LABEL_68:
    v7 = a3;
    goto LABEL_69;
  }
  *(_QWORD *)&Parameter = 0LL;
  v44 = DestinationString;
  v7 = a3;
  *(_QWORD *)&v45 = a3;
  *((PVOID *)&v45 + 1) = Entry[1];
  v46 = v19;
  v47 = v39;
  if ( !v41 )
  {
    goto LABEL_59;
  }
  *((_QWORD *)&Parameter + 1) = v41;
  v13 = KeExpandKernelStackAndCallout(CmQueryValueKeyCallout, &Parameter, 0x4800uLL);
  v31 = v13;
  if ( v13 < 0 )
  {
    goto LABEL_69;
  }
  v27 = Parameter;
  v13 = Parameter;
  v31 = Parameter;
  if ( (int)Parameter < 0 && (_DWORD)Parameter != -1073741789 && (_DWORD)Parameter != -2147483643 )
  {
    if ( (_DWORD)Parameter != -1073741772 )
    {
      goto LABEL_69;
    }
LABEL_59:
    *((_QWORD *)&Parameter + 1) = v36;
    v13 = KeExpandKernelStackAndCallout(CmQueryValueKeyCallout, &Parameter, 0x4800uLL);
    v31 = v13;
    if ( v13 < 0 )
    {
      goto LABEL_69;
    }
    v27 = Parameter;
    v13 = Parameter;
    v31 = Parameter;
    if ( (int)Parameter < 0 && (_DWORD)Parameter != -1073741789 && (_DWORD)Parameter != -2147483643 )
    {
      goto LABEL_69;
    }
  }
  v28 = v39[0];
  *(_DWORD *)Object = v39[0];
  if ( v27 != -1073741789 )
  {
    if ( v28 < v19 )
    {
      v19 = v28;
    }
    CmpBounceContextCopyDataToCallerBuffer(Entry, v19);
  }
LABEL_69:
  if ( v41 )
  {
    ObfDereferenceObject(v41);
  }
  if ( v32 )
  {
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v42[0] != v42 )
    {
      v57 = 0;
      v60 = 0LL;
      v61 = 0LL;
      v62 = 0;
      v55 = v36;
      v56 = v13;
      v59 = v13;
      v58 = &v50;
      CmpCallCallBacksEx(23, (unsigned int)&v55, 0, 0, 23, (__int64)v36, (__int64)v42);
      v13 = v59;
    }
    v31 = v13;
  }
  if ( v33 )
  {
    KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
    v13 = v31;
  }
  if ( v36 )
  {
    ObfDereferenceObject(v36);
  }
  if ( Entry[1] && Entry[1] != Entry[0] && Entry[1] != v67 )
  {
    if ( (v66 & 1) != 0 )
    {
      ExFreeToLookasideListEx(&CmpBounceBufferLookaside, Entry[1]);
    }
    else
    {
      CmSiFreeMemory((PPRIVILEGE_SET)Entry[1]);
    }
  }
  if ( CmpTraceRoutine )
  {
    p_DestinationString = &DestinationString;
    LOBYTE(p_DestinationString) = 16;
    guard_dispatch_icall_no_overrides(p_DestinationString, v71, (unsigned int)v13, v7, v10, &DestinationString);
  }
  if ( Privileges && Privileges != (PPRIVILEGE_SET)v72 )
  {
    CmSiFreeMemory(Privileges);
  }
  if ( v12 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v63);
  return (unsigned int)v13;
}
