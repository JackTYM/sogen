line
__int64 __fastcall NtQueryKey(HANDLE Handle, unsigned int a2, unsigned __int64 a3, unsigned int a4, _DWORD *a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v5 = a4;
  v39 = a5;
  v38 = a2;
  v61 = 0LL;
  v62 = 0LL;
  HandleInformation = 0LL;
  LODWORD(Object) = 0;
  v40 = 0;
  memset(v65, 0, sizeof(v65));
  v66 = 0;
  v67 = 0;
  v68 = 0;
  memset(v69, 0, sizeof(v69));
  v43 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v69, 0x20000LL);
  }
  v34 = 0;
  v35 = 0;
  v42 = 0LL;
  v57 = 0LL;
  v58 = 0LL;
  v59 = 0LL;
  v60 = 0LL;
  *(_OWORD *)Entry = 0LL;
  v64 = 0;
  v45[1] = v45;
  v45[0] = v45;
  CmpInitializeThreadInfo(&v61);
  v44 = 0LL;
  v37 = CmpAcquireShutdownRundown();
  if ( !v37 )
  {
    Key = -1073741431;
    goto LABEL_34;
  }
  if ( a2 > 8 )
  {
    if ( CmpTraceRoutine )
    {
      if ( Handle )
      {
        PreviousMode = KeGetCurrentThread()->PreviousMode;
        v39 = 0LL;
        if ( ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)CmKeyObjectType, PreviousMode, &v39, 0LL) >= 0 )
        {
          v43 = *((_QWORD *)v39 + 1);
          ObfDereferenceObject(v39);
        }
      }
    }
    Key = -1073741811;
    goto LABEL_34;
  }
  v9 = KeGetCurrentThread()->PreviousMode;
  v33 = v9;
  if ( v9 == 1 )
  {
    if ( (_DWORD)v5 )
    {
      v10 = a3;
      if ( (a3 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v11 = v5 + a3 - 1;
      if ( a3 > v11 || (v12 = 0x7FFFFFFF0000LL, v11 >= 0x7FFFFFFF0000LL) )
      {
        ExRaiseAccessViolation();
      }
      v13 = (v11 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      do
      {
        *(_BYTE *)v10 = *(_BYTE *)v10;
        v10 = (v10 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      }
      while ( v10 != v13 );
    }
    else
    {
      v12 = 0x7FFFFFFF0000LL;
    }
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )
    {
      v12 = (__int64)a5;
    }
    *(_DWORD *)v12 = *(_DWORD *)v12;
    v9 = 1;
  }
  Object = 0LL;
  Key = ObReferenceObjectByHandle(
          Handle,
          ((a2 - 3) & 0xFFFFFFFB) != 0,
          (POBJECT_TYPE)CmKeyObjectType,
          v9,
          &Object,
          &HandleInformation);
  v15 = Object;
  v42 = Object;
  v32 = Key;
  if ( Key < 0 )
  {
    goto LABEL_35;
  }
  if ( *(_DWORD *)Object != 1803104306 )
  {
    if ( a2 != 4 )
    {
      Key = -1073741816;
      goto LABEL_34;
    }
    *a5 = 40;
    if ( (unsigned int)v5 < 0x28 )
    {
      Key = -1073741789;
      v32 = -1073741789;
      goto LABEL_35;
    }
    *(_OWORD *)a3 = 0LL;
    *(_OWORD *)(a3 + 16) = 0LL;
    *(_QWORD *)(a3 + 32) = 0LL;
    *(_DWORD *)(a3 + 20) = *(_DWORD *)(v15[1] + 96LL);
    goto LABEL_33;
  }
  if ( CmpTraceRoutine )
  {
    v43 = *((_QWORD *)Object + 1);
  }
  if ( ((a2 - 3) & 0xFFFFFFFB) == 0 && !HandleInformation.GrantedAccess )
  {
    Key = -1073741790;
    goto LABEL_34;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v35 = 1;
  if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() )
  {
    v18 = (unsigned __int16 *)Object;
    *(_QWORD *)&v57 = Object;
    DWORD2(v57) = a2;
    *(_QWORD *)&v58 = a3;
    DWORD2(v58) = v5;
    *(_QWORD *)&v59 = v39;
    LOBYTE(v17) = 1;
    v19 = CmpCallCallBacksEx(7, (unsigned int)&v57, 0, v17, 22, (__int64)Object, (__int64)v45);
    Key = v19;
    v32 = v19;
    if ( v19 >= 0 )
    {
      v34 = 1;
      goto LABEL_29;
    }
    if ( v19 != -1073740541 )
    {
      goto LABEL_35;
    }
LABEL_33:
    Key = 0;
LABEL_34:
    v32 = Key;
    goto LABEL_35;
  }
  v18 = (unsigned __int16 *)Object;
LABEL_29:
  if ( a2 == 7 )
  {
    *(_DWORD *)v39 = 4;
    if ( (unsigned int)v5 < 4 )
    {
      Key = -1073741789;
      v32 = -1073741789;
      goto LABEL_35;
    }
    *(_DWORD *)a3 = v18[25];
    goto LABEL_33;
  }
  Key = CmKeyBodyRemapToVirtualForEnum(&v42, v33, ((a2 - 3) & 0xFFFFFFFB) != 0, &v44);
  v32 = Key;
  if ( Key >= 0 )
  {
    Entry[0] = (PVOID)a3;
    if ( !(_DWORD)v5 )
    {
      Entry[1] = 0LL;
      goto LABEL_64;
    }
    if ( !*((_QWORD *)&CmpRegistryProcess + 1) || !v33 && a3 > 0x7FFFFFFEFFFFLL )
    {
      goto LABEL_77;
    }
    if ( (unsigned int)dword_140E09ED8 > 5 && (qword_140E09EE8 & 4) != 0 && (qword_140E09EF0 & 4) == qword_140E09EF0 )
    {
      v47 = 1LL;
      v71 = &v47;
      v72 = 8LL;
      v36 = 2;
      v73 = &v36;
      v74 = 1LL;
      v28 = v5;
      if ( (((_DWORD)v5 - 1) & (unsigned int)v5) != 0 )
      {
        v29 = -1;
        do
        {
          ++v29;
          v28 >>= 1;
        }
        while ( v28 );
        v28 = 1 << (v29 + 1);
      }
      v48 = v28;
      v75 = &v48;
      v76 = 8LL;
      tlgWriteAgg((unsigned int)&dword_140E09ED8, (unsigned int)&dword_140054FD4, v22, 5, (__int64)v70);
    }
    if ( v5 > 0x40 )
    {
      if ( v5 <= 0x1000 )
      {
        v25 = ExAllocateFromLookasideListEx(&CmpBounceBufferLookaside);
        a3 = (unsigned __int64)v25;
        if ( v25 )
        {
          memset_0(v25, 0, v5);
          v64 |= 1u;
LABEL_77:
          Entry[1] = (PVOID)a3;
          goto LABEL_64;
        }
      }
      TransientPoolWithQuota = (_OWORD *)CmpAllocateTransientPoolWithQuota(v21, v5, 1111641411LL);
      if ( !TransientPoolWithQuota )
      {
        Key = -1073741670;
LABEL_65:
        v32 = Key;
        if ( Key >= 0 )
        {
          Key = CmQueryKey((_DWORD)v42, (_DWORD)v44, v5, (__int64)&v40);
          v32 = Key;
          if ( Key >= 0 || Key == -2147483643 || Key == -1073741789 )
          {
            v24 = v40;
            *(_DWORD *)v39 = v40;
            if ( Key != -1073741789 )
            {
              if ( (unsigned int)v5 >= v24 )
              {
                LODWORD(v5) = v24;
              }
              if ( Entry[0] != Entry[1] )
              {
                memmove(Entry[0], Entry[1], (unsigned int)v5);
              }
            }
          }
        }
        goto LABEL_35;
      }
    }
    else
    {
      memset_0(v65, 0, v5);
      TransientPoolWithQuota = v65;
    }
    Entry[1] = TransientPoolWithQuota;
LABEL_64:
    Key = 0;
    goto LABEL_65;
  }
LABEL_35:
  if ( v44 )
  {
    ObfDereferenceObject(v44);
  }
  if ( v34 )
  {
    if ( CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v45[0] != v45 )
    {
      v51 = 0;
      v54 = 0LL;
      v55 = 0LL;
      v56 = 0;
      v49 = v42;
      v50 = Key;
      v53 = Key;
      v52 = &v57;
      CmpCallCallBacksEx(22, (unsigned int)&v49, 0, 0, 22, (__int64)v42, (__int64)v45);
      Key = v53;
    }
    v32 = Key;
  }
  if ( v35 )
  {
    KeLeaveCriticalRegionThread(KeGetCurrentThread());
    Key = v32;
  }
  if ( v42 )
  {
    ObfDereferenceObject(v42);
  }
  if ( Entry[1] && Entry[1] != Entry[0] && Entry[1] != v65 )
  {
    if ( (v64 & 1) != 0 )
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
    v26 = EtwpTraceRegistry;
    if ( (__int64 (__fastcall *)(_DWORD, _DWORD, _DWORD, _DWORD, __int64, __int64))CmpTraceRoutine == EtwpTraceRegistry )
    {
      LOBYTE(v26) = 13;
      EtwpTraceRegistry((_DWORD)v26, (unsigned int)v69, Key, a2, v43, 0LL);
    }
    else
    {
      v31 = v43;
      LOBYTE(v31) = 13;
      guard_dispatch_icall_no_overrides(v31, v69, (unsigned int)Key, a2, v43, 0LL);
    }
  }
  if ( v37 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v61);
  return (unsigned int)Key;
}
