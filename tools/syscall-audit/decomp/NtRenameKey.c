line
__int64 __fastcall NtRenameKey(__int64 a1, _OWORD *a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v33 = a1;
  v37 = 0LL;
  v38 = 0LL;
  *(_OWORD *)Src = 0LL;
  memset(&ApcState, 0, sizeof(ApcState));
  CmpInitializeThreadInfo(&v37);
  v3 = 0;
  Object = 0LL;
  Privileges = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v5 = 0;
  memset(v35, 0, sizeof(v35));
  v36 = 0LL;
  v31[1] = v31;
  v31[0] = v31;
  v27 = 0;
  v6 = 0;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  v25 = CmpAcquireShutdownRundown();
  if ( !v25 )
  {
    v7 = -1073741431;
    v8 = 0;
    goto LABEL_42;
  }
  if ( (_BYTE)PreviousMode == 1 )
  {
    v32 = 0LL;
    v9 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      v9 = (__int64)a2;
    }
    v32.m128i_i32[0] = *(_DWORD *)v9;
    v10 = *(_QWORD *)(v9 + 8);
    v32.m128i_i64[1] = v10;
    *(__m128i *)Src = v32;
    if ( (unsigned __int16)_mm_cvtsi128_si32(v32) && (v10 & 1) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
  }
  else
  {
    *(_OWORD *)Src = *a2;
  }
  if ( (unsigned __int16)(LOWORD(Src[0]) - 1) > 0x1FFu
    || ((__int64)Src[0] & 1) != 0
    || (v11 = (PPRIVILEGE_SET)Src[1], !*(_WORD *)Src[1]) )
  {
    v7 = -1073741811;
    v28 = -1073741811;
    v8 = 0;
    goto LABEL_42;
  }
  if ( (unsigned __int8)CmpDoesBufferRequireCapturing(PreviousMode, Src[1]) )
  {
    Privileges = (PPRIVILEGE_SET)CmpAllocateTransientPoolWithQuota(v12, v13, 1114524995LL);
    if ( !Privileges )
    {
      v7 = -1073741670;
      v28 = -1073741670;
      v8 = 0;
      goto LABEL_42;
    }
    v11 = Privileges;
    memmove(Privileges, Src[1], LOWORD(Src[0]));
    Src[1] = Privileges;
    v13 = LOWORD(Src[0]);
    WORD1(Src[0]) = Src[0];
    v14 = 1;
  }
  for ( i = 0; i < (unsigned __int16)v13 >> 1; i += v14 )
  {
    if ( *((_WORD *)&v11->PrivilegeCount + i) == 92 )
    {
      v7 = -1073741811;
LABEL_23:
      v8 = 0;
      goto LABEL_42;
    }
  }
  LOBYTE(v14) = PreviousMode;
  v16 = v33;
  v7 = CmObReferenceObjectByHandle(v33, 131078, v13, v14, (__int64)&Object, 0LL);
  if ( v7 == -1073741790 )
  {
    SeCaptureSubjectContext(&SubjectContext);
    v6 = 1;
    if ( !(unsigned __int8)CmDoVirtualTest(&SubjectContext, &v27) )
    {
      goto LABEL_26;
    }
    LOBYTE(v19) = PreviousMode;
    v7 = CmObReferenceObjectByHandle(v16, 131097, v18, v19, (__int64)&Object, 0LL);
    if ( v7 < 0 )
    {
      goto LABEL_23;
    }
    if ( !(unsigned __int8)CmKeyBodyNeedsVirtualImage(Object) )
    {
LABEL_26:
      v7 = -1073741790;
      goto LABEL_23;
    }
    v5 = 1;
  }
  v6 = v5;
  if ( v7 < 0 )
  {
    goto LABEL_23;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v8 = 1;
  if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
  {
    *(_QWORD *)&v35[0] = Object;
    *((_QWORD *)&v35[0] + 1) = Src;
    LOBYTE(v21) = 1;
    v22 = CmpCallCallBacksEx(4, (unsigned int)v35, 0, v21, 19, (__int64)Object, (__int64)v31);
    if ( v22 < 0 )
    {
      v7 = 0;
      if ( v22 != -1073740541 )
      {
        v7 = v22;
      }
      goto LABEL_42;
    }
    v3 = 1;
  }
  if ( !v5
    || (LOBYTE(v17) = PreviousMode,
        v7 = CmKeyBodyReplicateToVirtual(&Object, v17, 131078LL, &SubjectContext, &v27),
        v7 >= 0) )
  {
    CmpAttachToRegistryProcess(&ApcState);
    v32 = *(__m128i *)Src;
    LOBYTE(v23) = PreviousMode;
    v7 = CmRenameKey(Object, &v32, v23);
    CmpDetachFromRegistryProcess(&ApcState);
    v6 = v5;
  }
LABEL_42:
  if ( v6 )
  {
    SeReleaseSubjectContext(&SubjectContext);
  }
  if ( v3 )
  {
    v7 = CmPostCallbackNotificationEx(19, (_DWORD)Object, v7, (unsigned int)v35, 0LL, (__int64)v31);
  }
  if ( v8 )
  {
    KeLeaveCriticalRegion();
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  if ( Privileges )
  {
    CmSiFreeMemory(Privileges);
  }
  if ( v25 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v37);
  return (unsigned int)v7;
}
