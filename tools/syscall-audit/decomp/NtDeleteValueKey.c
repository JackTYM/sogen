line
__int64 __fastcall NtDeleteValueKey(__int64 a1, _OWORD *a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v3 = a1;
  v35 = a1;
  v45 = 0LL;
  v46 = 0LL;
  memset(v43, 0, sizeof(v43));
  v44 = 0LL;
  v32 = 0LL;
  *(_OWORD *)Src = 0LL;
  Privileges = 0LL;
  v4 = 0;
  v31[0] = 0;
  memset(v47, 0, sizeof(v47));
  v36 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v47, 0x20000LL);
  }
  v5 = 0;
  Object = 0LL;
  v34[1] = v34;
  v34[0] = v34;
  CmpInitializeThreadInfo(&v45);
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v7 = 0;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  v28 = CmpAcquireShutdownRundown();
  if ( !v28 )
  {
    v12 = -1073741431;
    goto LABEL_59;
  }
  LOBYTE(v10) = PreviousMode;
  v12 = CmObReferenceObjectByHandle(v3, 2, v9, v10, (__int64)&Object, (__int64)&v32);
  if ( v12 == -1073741790 )
  {
    SeCaptureSubjectContext(&SubjectContext);
    v7 = 1;
    if ( !CmpVEEnabled )
    {
      goto LABEL_58;
    }
    LOBYTE(v8) = KeGetCurrentThread()->PreviousMode;
    if ( (unsigned __int8)CmpIsSystemEntity(v8, &SubjectContext, v31) )
    {
      goto LABEL_58;
    }
    LOBYTE(v25) = PreviousMode;
    v12 = CmObReferenceObjectByHandle(v35, 131097, v24, v25, (__int64)&Object, (__int64)&v32);
    if ( v12 < 0 )
    {
      goto LABEL_59;
    }
    if ( !(unsigned __int8)CmKeyBodyNeedsVirtualImage(Object) )
    {
LABEL_58:
      v12 = -1073741790;
      goto LABEL_59;
    }
    v4 = 1;
  }
  v7 = v4;
  if ( v12 < 0 )
  {
LABEL_59:
    v17 = 0;
    goto LABEL_32;
  }
  if ( CmpTraceRoutine )
  {
    v8 = (__int64)Object;
    if ( Object )
    {
      v36 = *((_QWORD *)Object + 1);
    }
  }
  if ( (_BYTE)PreviousMode == 1 )
  {
    v37 = 0LL;
    v8 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      v8 = (__int64)a2;
    }
    v37.m128i_i32[0] = *(_DWORD *)v8;
    v11 = *(_QWORD *)(v8 + 8);
    v37.m128i_i64[1] = v11;
    *(__m128i *)Src = v37;
    v13 = _mm_cvtsi128_si32(v37);
    if ( v13 )
    {
      if ( (v11 & 1) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v8 = v13 + v11;
    }
  }
  else
  {
    *(_OWORD *)Src = *a2;
  }
  LOWORD(v14) = Src[0];
  v15 = (char *)((unsigned __int64)Src[1] & -(__int64)(LOWORD(Src[0]) != 0));
  Src[1] = v15;
  WORD1(Src[0]) = Src[0];
  if ( LOWORD(Src[0]) && (unsigned __int8)CmpDoesBufferRequireCapturing(PreviousMode, v15) )
  {
    TransientPoolWithQuota = (struct _PRIVILEGE_SET *)CmpAllocateTransientPoolWithQuota(v8, v14, 1650675011LL);
    Privileges = TransientPoolWithQuota;
    if ( !TransientPoolWithQuota )
    {
      v12 = -1073741670;
      v31[1] = -1073741670;
      v7 = v4;
      v17 = 0;
      goto LABEL_32;
    }
    v15 = (char *)TransientPoolWithQuota;
    memmove(TransientPoolWithQuota, Src[1], LOWORD(Src[0]));
    Src[1] = v15;
    LOWORD(v14) = Src[0];
  }
  if ( (v14 & 1) != 0 )
  {
    LOWORD(Src[0]) = 0;
    v12 = -1073741811;
    goto LABEL_57;
  }
  v8 = *((_QWORD *)Object + 1);
  if ( (*(_DWORD *)(v8 + 8) & 0x80u) != 0 )
  {
    v12 = -1073741790;
LABEL_57:
    v7 = v4;
    v17 = 0;
    goto LABEL_32;
  }
  for ( i = &v15[2 * ((unsigned __int64)(unsigned __int16)v14 >> 1) - 2]; (_WORD)v14 && !*(_WORD *)i; i -= 2 )
  {
    LOWORD(v14) = v14 - 2;
    LOWORD(Src[0]) = v14;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v17 = 1;
  if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired(i) )
  {
    v43[0] = Object;
    v43[1] = Src;
    LOBYTE(v20) = 1;
    v21 = CmpCallCallBacksEx(2, (unsigned int)v43, 0, v20, 17, (__int64)Object, (__int64)v34);
    if ( v21 < 0 )
    {
      v7 = v4;
      v12 = 0;
      if ( v21 != -1073740541 )
      {
        v12 = v21;
      }
      goto LABEL_32;
    }
    v5 = 1;
  }
  if ( !v4 )
  {
    goto LABEL_30;
  }
  LOBYTE(v11) = PreviousMode;
  v12 = CmKeyBodyRemapToVirtual((unsigned int)&Object, v11, 2, (unsigned int)&SubjectContext, (__int64)v31);
  v7 = v4;
  if ( v12 >= 0 )
  {
    if ( !CmpVEEnabled || (v8 = *((_QWORD *)Object + 1), v26 = 1, (*(_DWORD *)(v8 + 184) & 0x1000000) == 0) )
    {
      v26 = 0;
    }
    if ( !v26 )
    {
      v12 = -1073741790;
      goto LABEL_31;
    }
LABEL_30:
    v37 = *(__m128i *)Src;
    v22 = (unsigned int)v32 >> 2;
    LOBYTE(v22) = (v32 & 4) != 0;
    v12 = CmDeleteValueKey(Object, &v37, v35, v22);
LABEL_31:
    v7 = v4;
  }
LABEL_32:
  if ( v7 )
  {
    SeReleaseSubjectContext(&SubjectContext);
  }
  if ( v5 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired(v8) && (_QWORD *)v34[0] != v34 )
  {
    v41 = 0LL;
    v42 = 0LL;
    v39[0] = Object;
    v39[1] = (unsigned int)v12;
    v40 = (unsigned int)v12;
    v39[2] = v43;
    CmpCallCallBacksEx(17, (unsigned int)v39, 0, 0, 17, (__int64)Object, (__int64)v34);
    v12 = v40;
  }
  if ( v17 )
  {
    KeLeaveCriticalRegion();
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  if ( CmpTraceRoutine )
  {
    v27 = v36;
    LOBYTE(v27) = 15;
    guard_dispatch_icall_no_overrides(v27, v47, (unsigned int)v12, 0LL, v36, Src);
  }
  if ( Privileges )
  {
    CmSiFreeMemory(Privileges);
  }
  if ( v28 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v45);
  return (unsigned int)v12;
}
