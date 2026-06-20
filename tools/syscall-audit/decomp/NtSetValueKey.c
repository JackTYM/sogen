line
__int64 __fastcall NtSetValueKey(__int64 a1, UNICODE_STRING *a2, int a3, int a4, void *a5, size_t Size)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v37 = a4;
  v39 = a3;
  v43 = a2;
  v41 = a1;
  Src = a5;
  v56 = 0LL;
  v57 = 0LL;
  DestinationString = 0LL;
  v40 = 0LL;
  memset(v63, 0, sizeof(v63));
  v44 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v63, 0x20000LL);
  }
  CmpInitializeThreadInfo(&v56);
  v6 = 0;
  v7 = 0;
  Object = 0LL;
  v8 = 0;
  v42[1] = v42;
  v42[0] = v42;
  memset_0(v58, 0, 0x40uLL);
  RtlInitUnicodeString(&DestinationString, 0LL);
  v35 = 0LL;
  v45 = 0LL;
  v36 = 0;
  Privileges = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v30 = PreviousMode;
  v10 = 0;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  v14 = CmpAcquireShutdownRundown();
  if ( !v14 )
  {
    v15 = -1073741431;
    v31 = -1073741431;
    goto LABEL_74;
  }
  v15 = CmObReferenceObjectByHandle(v41, 2, v13, PreviousMode, (__int64)&Object, (__int64)&v40);
  v31 = v15;
  if ( v15 == -1073741790 )
  {
    SeCaptureSubjectContext(&SubjectContext);
    v10 = 1;
    if ( !CmpVEEnabled
      || (unsigned __int8)CmpIsSystemEntity((unsigned __int8)KeGetCurrentThread()->PreviousMode, &SubjectContext, &v36) )
    {
      goto LABEL_101;
    }
    v15 = CmObReferenceObjectByHandle(v41, 131097, v13, v30, (__int64)&Object, (__int64)&v40);
    v31 = v15;
    if ( v15 < 0 )
    {
      goto LABEL_74;
    }
    if ( !(unsigned __int8)CmKeyBodyNeedsVirtualImage(Object) )
    {
LABEL_101:
      v15 = -1073741790;
      v31 = -1073741790;
      goto LABEL_74;
    }
    v8 = 1;
  }
  v10 = v8;
  if ( v15 < 0 )
  {
    goto LABEL_74;
  }
  if ( CmpTraceRoutine && Object )
  {
    v44 = *((_QWORD *)Object + 1);
  }
  v12 = v30;
  if ( v30 == 1 )
  {
    v46 = 0LL;
    v16 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)v43 < 0x7FFFFFFF0000LL )
    {
      v16 = (__int64)v43;
    }
    v46.m128i_i32[0] = *(_DWORD *)v16;
    v13 = *(_QWORD *)(v16 + 8);
    v46.m128i_i64[1] = v13;
    DestinationString = (UNICODE_STRING)v46;
    DestinationString.MaximumLength = _mm_cvtsi128_si32(v46);
    Length = v46.m128i_u16[0];
    if ( v46.m128i_i16[0] )
    {
      if ( (v13 & 1) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      if ( v46.m128i_u16[0] + v13 > 0x7FFFFFFF0000LL || v46.m128i_u16[0] + v13 < v13 )
      {
        Length = DestinationString.Length;
      }
    }
    v17 = Size;
    if ( (_DWORD)Size )
    {
      v13 = (unsigned __int64)Src + (unsigned int)Size;
      if ( v13 > 0x7FFFFFFF0000LL || v13 < (unsigned __int64)Src )
      {
        Length = DestinationString.Length;
      }
    }
    goto LABEL_18;
  }
  DestinationString = *v43;
  v35 = 0LL;
  v17 = Size;
  if ( (_DWORD)Size )
  {
    Length = DestinationString.Length;
LABEL_18:
    v12 = v30;
    goto LABEL_19;
  }
  Length = DestinationString.Length;
LABEL_19:
  if ( (unsigned __int16)Length > 0x7FFFu || (Length & 1) != 0 || v17 > 0x7FFFF000 )
  {
    v15 = -1073741811;
    v31 = -1073741811;
    v10 = v8;
    v7 = 0;
    goto LABEL_74;
  }
  v18 = (unsigned __int16)Length;
  v19 = ((unsigned __int16)Length + 7) & 0xFFFFFFF8;
  v20 = v19 + v17;
  if ( v19 + v17 < v17 )
  {
    v15 = -1073741811;
    v31 = -1073741811;
    v10 = v8;
    v7 = 0;
    goto LABEL_74;
  }
  if ( (_BYTE)v12
    || (v12 = *((_QWORD *)&CmpRegistryProcess + 1), v13 = 0x7FFFFFFEFFFFLL, *((_QWORD *)&CmpRegistryProcess + 1))
    && (DestinationString.Buffer <= (wchar_t *)0x7FFFFFFEFFFFLL
     || *((_QWORD *)&CmpRegistryProcess + 1) && (unsigned __int64)Src <= 0x7FFFFFFEFFFFLL) )
  {
    if ( v20 )
    {
      if ( v20 <= 0x40 )
      {
        goto LABEL_26;
      }
      if ( v17 <= 0x40 )
      {
        if ( (unsigned __int16)Length <= 0x40u && v17 < (unsigned __int16)Length )
        {
          v18 = v17;
        }
      }
      else
      {
        v18 = v19 + v17;
        if ( (unsigned __int16)Length <= 0x40u )
        {
          v18 = v17;
        }
      }
      if ( v18 )
      {
        Privileges = (PPRIVILEGE_SET)CmpAllocateTransientPoolWithQuota(v12, v18, 1851157827LL);
        if ( !Privileges )
        {
          v15 = -1073741670;
          v31 = -1073741670;
          v10 = v8;
          v7 = 0;
          goto LABEL_74;
        }
        Length = DestinationString.Length;
        if ( v18 == v17 )
        {
          v35 = v64;
          v21 = Privileges;
        }
        else
        {
          v35 = Privileges;
          if ( v18 == DestinationString.Length )
          {
            v21 = (PPRIVILEGE_SET)v64;
          }
          else
          {
            v21 = (PPRIVILEGE_SET)((char *)Privileges + v19);
          }
        }
      }
      else
      {
LABEL_26:
        v35 = v64;
        v21 = (PPRIVILEGE_SET)&v64[v19];
      }
    }
    else
    {
      v21 = v45;
    }
    if ( (_WORD)Length )
    {
      v22 = (wchar_t *)v35;
      memmove(v35, DestinationString.Buffer, (unsigned __int16)Length);
      DestinationString.Buffer = v22;
      Length = DestinationString.Length;
    }
    else
    {
      DestinationString.Buffer = 0LL;
    }
    if ( v17 )
    {
      memmove(v21, Src, v17);
      Src = v21;
      Length = DestinationString.Length;
    }
    else
    {
      Src = 0LL;
    }
  }
  if ( (_WORD)Length )
  {
    v13 = 65534LL;
    do
    {
      if ( DestinationString.Buffer[((unsigned __int64)(unsigned __int16)Length >> 1) - 1] )
      {
        break;
      }
      v28 = (_WORD)Length == 2;
      LOWORD(Length) = Length - 2;
      DestinationString.Length = Length;
    }
    while ( !v28 );
  }
  v12 = *((_QWORD *)Object + 1);
  if ( (*(_DWORD *)(v12 + 8) & 0x80u) == 0 )
  {
    CurrentThread = KeGetCurrentThread();
    --CurrentThread->KernelApcDisable;
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired(v12, Length, v13) )
    {
      v58[0] = Object;
      v58[1] = &DestinationString;
      v59 = v39;
      v60 = v37;
      v61 = Src;
      v62 = v17;
      LOBYTE(v24) = 1;
      v25 = CmpCallCallBacksEx(1, (unsigned int)v58, 0, v24, 16, (__int64)Object, (__int64)v42);
      if ( v25 < 0 )
      {
        v10 = v8;
        v15 = 0;
        if ( v25 != -1073740541 )
        {
          v15 = v25;
        }
        v31 = v15;
        goto LABEL_73;
      }
      v6 = 1;
    }
    if ( !v8
      || (v15 = CmKeyBodyReplicateToVirtual(&Object, v30, 2LL, &SubjectContext, &v36), v31 = v15, v10 = v8, v15 >= 0) )
    {
      LODWORD(v29) = v17;
      v15 = CmSetValueKey((_DWORD)Object, (unsigned int)&DestinationString, v37, (_DWORD)Src, v29, v41, (v40 & 4) != 0);
      v31 = v15;
      v10 = v8;
    }
LABEL_73:
    v7 = 1;
    goto LABEL_74;
  }
  v15 = -1073741790;
  v31 = -1073741790;
  v10 = v8;
  v7 = 0;
LABEL_74:
  if ( v10 )
  {
    SeReleaseSubjectContext(&SubjectContext);
  }
  if ( v6 )
  {
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired(v12, Length, v13) && (_QWORD *)v42[0] != v42 )
    {
      v50 = 0;
      v53 = 0LL;
      v54 = 0LL;
      v55 = 0;
      v48 = Object;
      v49 = v15;
      v52 = v15;
      v51 = v58;
      CmpCallCallBacksEx(16, (unsigned int)&v48, 0, 0, 16, (__int64)Object, (__int64)v42);
      v15 = v52;
    }
    v31 = v15;
  }
  if ( v7 )
  {
    KeLeaveCriticalRegionThread(KeGetCurrentThread());
    v15 = v31;
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  if ( CmpTraceRoutine )
  {
    v27 = v44;
    LOBYTE(v27) = 14;
    guard_dispatch_icall_no_overrides(v27, v63, (unsigned int)v15, 0LL, v44, &DestinationString);
  }
  if ( Privileges )
  {
    CmSiFreeMemory(Privileges);
  }
  if ( v14 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v56);
  return (unsigned int)v15;
}
