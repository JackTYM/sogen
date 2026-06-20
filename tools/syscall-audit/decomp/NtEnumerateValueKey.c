line
__int64 __fastcall NtEnumerateValueKey(
        HANDLE Handle,
        unsigned int a2,
        unsigned int a3,
        unsigned __int64 a4,
        size_t Size,
        void *a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v57 = a3;
  v8 = a2;
  v60 = a6;
  v61 = a2;
  v78 = 0LL;
  v79 = 0LL;
  LODWORD(v59) = 0;
  memset(&v91, 0, sizeof(v91));
  memset_0(v88, 0, 0x47uLL);
  memset(v89, 0, sizeof(v89));
  *(_QWORD *)&v58[1] = 0LL;
  v65 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v89, 0x20000LL);
  }
  v52 = 0;
  v53 = 0;
  memset_0(&v80, 0, 0x40uLL);
  v10 = 0LL;
  v56 = 0LL;
  v58[0] = 0;
  *(_OWORD *)Src = 0LL;
  v87 = 0;
  v66[1] = v66;
  v66[0] = v66;
  CmpInitializeThreadInfo(&v78);
  Object = 0LL;
  v12 = CmpAcquireShutdownRundown();
  v50 = v12;
  if ( !v12 )
  {
    KeyValueData = -1073741431;
LABEL_89:
    v40 = 0LL;
    goto LABEL_62;
  }
  if ( a3 > 2 )
  {
    if ( CmpTraceRoutine && Handle )
    {
      PreviousMode = KeGetCurrentThread()->PreviousMode;
      v60 = 0LL;
      if ( ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)CmKeyObjectType, PreviousMode, &v60, 0LL) >= 0 )
      {
        v40 = *((_QWORD *)v60 + 1);
        ObfDereferenceObject(v60);
        v10 = v56;
LABEL_122:
        KeyValueData = -1073741811;
        goto LABEL_62;
      }
      v10 = v56;
    }
    v40 = 0LL;
    goto LABEL_122;
  }
  v13 = KeGetCurrentThread()->PreviousMode;
  v14 = 1;
  KeyValueData = CmObReferenceObjectByHandle((_DWORD)Handle, 1, v11, v13, (__int64)&v56, 0LL);
  if ( KeyValueData < 0 )
  {
    v10 = v56;
    v12 = v50;
    goto LABEL_89;
  }
  if ( CmpTraceRoutine && v56 )
  {
    *(_QWORD *)&v58[1] = *((_QWORD *)v56 + 1);
    v65 = *(_QWORD *)&v58[1];
  }
  if ( v13 == 1 )
  {
    v16 = Size;
    if ( (_DWORD)Size )
    {
      if ( (a4 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v17 = 0x7FFFFFFF0000LL;
    }
    else
    {
      v17 = 0x7FFFFFFF0000LL;
    }
    v18 = v60;
    if ( (unsigned __int64)v60 < 0x7FFFFFFF0000LL )
    {
      v17 = (__int64)v60;
    }
    *(_DWORD *)v17 = *(_DWORD *)v17;
  }
  else
  {
    v16 = Size;
    v18 = v60;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v53 = 1;
  if ( !CmpCallBackCount || (unsigned int)CmpIsRegistryLockAcquired() )
  {
LABEL_20:
    KeyValueData = CmKeyBodyRemapToVirtualForEnum(&v56, v13, 1LL, &Object);
    if ( KeyValueData < 0 )
    {
      goto LABEL_107;
    }
    Src[0] = (void *)a4;
    if ( !v16 )
    {
      Src[1] = 0LL;
      goto LABEL_28;
    }
    if ( !*((_QWORD *)&CmpRegistryProcess + 1) || !v13 && a4 > 0x7FFFFFFEFFFFLL )
    {
      Src[1] = (void *)a4;
      goto LABEL_28;
    }
    if ( (unsigned int)dword_140E09ED8 > 5 && (qword_140E09EE8 & 4) != 0 && (qword_140E09EF0 & 4) == qword_140E09EF0 )
    {
      v67 = 1LL;
      v93 = &v67;
      v94 = 8LL;
      v55 = 1;
      v95 = &v55;
      v96 = 1LL;
      v45 = v16;
      if ( ((v16 - 1) & v16) != 0 )
      {
        v46 = v16;
        v47 = -1;
        do
        {
          ++v47;
          v46 >>= 1;
        }
        while ( v46 );
        v45 = 1 << (v47 + 1);
      }
      v68 = v45;
      v97 = &v68;
      v98 = 8LL;
      tlgWriteAgg((unsigned int)&dword_140E09ED8, (unsigned int)&dword_140054FD4, v23, 5, (__int64)v92);
    }
    if ( v16 > 0x40uLL )
    {
      if ( v16 <= 0x1000uLL )
      {
        v42 = ExAllocateFromLookasideListEx(&CmpBounceBufferLookaside);
        v43 = v42;
        if ( v42 )
        {
          memset_0(v42, 0, v16);
          v87 |= 1u;
          Src[1] = v43;
          goto LABEL_28;
        }
      }
      TransientPoolWithQuota = (_BYTE *)CmpAllocateTransientPoolWithQuota(v22, v16, 1111641411LL);
      if ( !TransientPoolWithQuota )
      {
        KeyValueData = -1073741670;
LABEL_29:
        if ( KeyValueData >= 0 )
        {
          if ( Object )
          {
            CmpAttachToRegistryProcess(&v91);
            v10 = v56;
            KeyValueData = CmEnumerateValueKeyFromMergedView(
                             (int)v56,
                             (int)Object,
                             0,
                             a2,
                             v57,
                             (size_t)Src[1],
                             v16,
                             (__int64)v58,
                             0LL);
            p_ApcState = &v91;
LABEL_55:
            CmpDetachFromRegistryProcess(p_ApcState);
            if ( KeyValueData >= 0 || KeyValueData == -2147483643 || KeyValueData == -1073741789 )
            {
              v39 = v58[0];
              *(_DWORD *)v60 = v58[0];
              if ( KeyValueData != -1073741789 )
              {
                if ( v16 >= v39 )
                {
                  v16 = v39;
                }
                if ( Src[0] != Src[1] )
                {
                  memmove(Src[0], Src[1], v16);
                }
              }
              v12 = v50;
              v8 = a2;
              v40 = *(_QWORD *)&v58[1];
            }
            else
            {
              v12 = v50;
              v8 = a2;
              v40 = *(_QWORD *)&v58[1];
            }
            goto LABEL_62;
          }
          v25 = Src[1];
          v69 = (size_t)Src[1];
          v62 = 0LL;
          memset(&ApcState, 0, sizeof(ApcState));
          v59 = 0LL;
          v64 = 0LL;
          HvpGetCellContextInitialize(&v62);
          v26 = 0LL;
          HvpGetCellContextInitialize(&v59);
          v51 = 0;
          CmpAttachToRegistryProcess(&ApcState);
          CmpLockRegistry();
          v10 = v56;
          v27 = *((_QWORD *)v56 + 1);
          if ( *(_WORD *)(v27 + 66) )
          {
            KeyValueData = CmEnumerateValueFromLayeredKey(v56, a2, (unsigned int)v57, v25, v16, v58);
            v32 = 0LL;
            goto LABEL_46;
          }
          CmpLockKcbShared(*((_QWORD *)v56 + 1));
          v51 = 1;
          KeyValueData = CmpPerformKeyBodyDeletionCheck(v10, 0LL);
          if ( KeyValueData >= 0 )
          {
            if ( !v10[7] && !v10[8] )
            {
              v14 = 0;
            }
            if ( !v14 )
            {
              v28 = v64;
LABEL_38:
              if ( v28 && v28 == *(_QWORD *)(v27 + 288) )
              {
                v29 = 280LL;
              }
              else
              {
                v29 = 96LL;
              }
              if ( a2 < *(_DWORD *)(v27 + v29) )
              {
                v30 = *(_QWORD *)(v27 + 32);
                if ( (*(_BYTE *)(v30 + 140) & 1) != 0 )
                {
                  CellFlat = HvpGetCellFlat(v30, *(unsigned int *)(v27 + v29 + 4));
                }
                else
                {
                  CellFlat = HvpGetCellPaged(v30);
                }
                v32 = CellFlat;
                v33 = *(_DWORD *)(CellFlat + 4LL * a2);
                v34 = *(_QWORD *)(v27 + 32);
                if ( (*(_BYTE *)(v34 + 140) & 1) != 0 )
                {
                  CellPaged = HvpGetCellFlat(v34, v33);
                }
                else
                {
                  CellPaged = HvpGetCellPaged(v34);
                }
                v26 = CellPaged;
                KeyValueData = CmpQueryKeyValueData(v27, v33, CellPaged, v57, v69, v16, (__int64)v58);
                goto LABEL_46;
              }
              KeyValueData = -2147483622;
              goto LABEL_101;
            }
            KeyValueData = CmpTransSearchAddTransFromKeyBody(v10, &v64);
            if ( KeyValueData >= 0 )
            {
              v28 = v64;
              KeyValueData = CmpPerformKeyBodyDeletionCheck(v10, v64);
              if ( KeyValueData < 0 )
              {
                v32 = 0LL;
                goto LABEL_46;
              }
              goto LABEL_38;
            }
          }
LABEL_101:
          v32 = 0LL;
LABEL_46:
          if ( v26 )
          {
            v36 = *(_QWORD *)(v27 + 32);
            if ( (*(_BYTE *)(v36 + 140) & 1) != 0 )
            {
              HvpReleaseCellFlat(v36, &v59);
            }
            else
            {
              HvpReleaseCellPaged(v36, &v59);
            }
          }
          if ( v32 )
          {
            v37 = *(_QWORD *)(v27 + 32);
            if ( (*(_BYTE *)(v37 + 140) & 1) != 0 )
            {
              HvpReleaseCellFlat(v37, &v62);
            }
            else
            {
              HvpReleaseCellPaged(v37, &v62);
            }
          }
          if ( v51 )
          {
            CmpUnlockKcb(v27);
          }
          CmpUnlockRegistry();
          p_ApcState = &ApcState;
          goto LABEL_55;
        }
LABEL_107:
        v10 = v56;
        v12 = v50;
        v8 = a2;
        v40 = *(_QWORD *)&v58[1];
        goto LABEL_62;
      }
    }
    else
    {
      memset_0(v88, 0, v16);
      TransientPoolWithQuota = v88;
    }
    Src[1] = TransientPoolWithQuota;
LABEL_28:
    KeyValueData = 0;
    goto LABEL_29;
  }
  v10 = v56;
  v80 = v56;
  v81 = a2;
  v82 = v57;
  v83 = a4;
  v84 = v16;
  v85 = v18;
  LOBYTE(v20) = 1;
  v21 = CmpCallCallBacksEx(6, (unsigned int)&v80, 0, v20, 21, (__int64)v56, (__int64)v66);
  KeyValueData = v21;
  if ( v21 >= 0 )
  {
    v52 = 1;
    goto LABEL_20;
  }
  v12 = v50;
  v8 = a2;
  v40 = *(_QWORD *)&v58[1];
  if ( v21 == -1073740541 )
  {
    KeyValueData = 0;
  }
LABEL_62:
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  if ( v52 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired() && (_QWORD *)v66[0] != v66 )
  {
    v72 = 0;
    v75 = 0LL;
    v76 = 0LL;
    v77 = 0;
    v70 = v10;
    v71 = KeyValueData;
    v74 = KeyValueData;
    v73 = &v80;
    CmpCallCallBacksEx(21, (unsigned int)&v70, 0, 0, 21, (__int64)v10, (__int64)v66);
    KeyValueData = v74;
  }
  if ( v53 )
  {
    KeLeaveCriticalRegionThread(KeGetCurrentThread());
    v10 = v56;
  }
  if ( v10 )
  {
    ObfDereferenceObject(v10);
  }
  if ( Src[1] && Src[1] != Src[0] && Src[1] != v88 )
  {
    if ( (v87 & 1) != 0 )
    {
      ExFreeToLookasideListEx(&CmpBounceBufferLookaside, Src[1]);
    }
    else
    {
      CmSiFreeMemory((PPRIVILEGE_SET)Src[1]);
    }
  }
  if ( CmpTraceRoutine )
  {
    v48 = EtwpTraceRegistry;
    LOBYTE(v48) = 18;
    if ( (__int64 (__fastcall *)(_DWORD, _DWORD, _DWORD, _DWORD, __int64, __int64))CmpTraceRoutine == EtwpTraceRegistry )
    {
      EtwpTraceRegistry((_DWORD)v48, (unsigned int)v89, KeyValueData, v8, v40, 0LL);
    }
    else
    {
      guard_dispatch_icall_no_overrides(v48, v89, (unsigned int)KeyValueData, v8, v40, 0LL);
    }
  }
  if ( v12 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v78);
  return (unsigned int)KeyValueData;
}
