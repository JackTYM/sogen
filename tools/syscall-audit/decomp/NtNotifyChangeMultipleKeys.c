__int64 __fastcall NtNotifyChangeMultipleKeys(                                                         | 
        int a1,                                                                                        | 
        unsigned int a2,                                                                               | 
        __int64 a3,                                                                                    | 
        void *a4,                                                                                      | 
        void (__stdcall *a5)(POPLOCK Oplock),                                                          | 
        __int64 a6,                                                                                    | 
        unsigned __int64 a7,                                                                           | 
        unsigned int a8,                                                                               | 
        unsigned __int8 a9,                                                                            | 
        unsigned __int64 a10,                                                                          | 
        unsigned int a11,                                                                              | 
        char a12)                                                                                      | 
{                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                        | 
                                                                                                       | 
  Handle = a4;                                                                                         | 
  v87 = a3;                                                                                            | 
  v88 = 0LL;                                                                                           | 
  v89 = 0LL;                                                                                           | 
  v86 = 0LL;                                                                                           | 
  v15 = 0LL;                                                                                           | 
  Object = 0LL;                                                                                        | 
  P = 0LL;                                                                                             | 
  v81 = 1;                                                                                             | 
  memset_0(v91, 0, sizeof(v91));                                                                       | 
  v16 = 0;                                                                                             | 
  v80 = 0;                                                                                             | 
  v85 = 0LL;                                                                                           | 
  LODWORD(v84) = 0;                                                                                    | 
  CmpInitializeThreadInfo(&v88);                                                                       | 
  v77 = CmpAcquireShutdownRundown();                                                                   | 
  if ( !v77 )                                                                                          | 
  {                                                                                                    | 
    CmpCleanupThreadInfo(&v88);                                                                        | 
    return 3221225865LL;                                                                               | 
  }                                                                                                    | 
  if ( a2 > 1 )                                                                                        | 
  {                                                                                                    | 
LABEL_125:                                                                                             | 
    CmpReleaseShutdownRundown();                                                                       | 
    CmpCleanupThreadInfo(&v88);                                                                        | 
    return 3221225485LL;                                                                               | 
  }                                                                                                    | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                   | 
  v79 = PreviousMode;                                                                                  | 
  if ( PreviousMode )                                                                                  | 
  {                                                                                                    | 
    v18 = a8;                                                                                          | 
    if ( (a8 & 0x10000000) == 0 )                                                                      | 
    {                                                                                                  | 
      Process = KeGetCurrentThread()->ApcState.Process;                                                | 
      if ( Process[1].ReadyTime )                                                                      | 
      {                                                                                                | 
        v44 = WORD2(Process[3].PerProcessorCycleTimes);                                                | 
        if ( v44 == 332 || v44 == 452 )                                                                | 
        {                                                                                              | 
          v16 = 1;                                                                                     | 
        }                                                                                              | 
      }                                                                                                | 
      v80 = v16;                                                                                       | 
      v20 = a7;                                                                                        | 
      if ( (a7 & 3) == 0 )                                                                             | 
      {                                                                                                | 
        v21 = a7 + 7 + 8 * (v16 ^ 1LL);                                                                | 
        if ( a7 > v21 || v21 >= 0x7FFFFFFF0000LL )                                                     | 
        {                                                                                              | 
          goto LABEL_20;                                                                               | 
        }                                                                                              | 
        v22 = (v21 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                    | 
        do                                                                                             | 
        {                                                                                              | 
          *(_BYTE *)v20 = *(_BYTE *)v20;                                                               | 
          v20 = (v20 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                  | 
        }                                                                                              | 
        while ( v20 != v22 );                                                                          | 
        if ( !a11 )                                                                                    | 
        {                                                                                              | 
          goto LABEL_17;                                                                               | 
        }                                                                                              | 
        v23 = a10;                                                                                     | 
        if ( (a10 & 3) == 0 )                                                                          | 
        {                                                                                              | 
          v24 = a11 + a10 - 1;                                                                         | 
          if ( a10 <= v24 && v24 < 0x7FFFFFFF0000LL )                                                  | 
          {                                                                                            | 
            v25 = (v24 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                | 
            do                                                                                         | 
            {                                                                                          | 
              *(_BYTE *)v23 = *(_BYTE *)v23;                                                           | 
              v23 = (v23 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                              | 
            }                                                                                          | 
            while ( v23 != v25 );                                                                      | 
LABEL_17:                                                                                              | 
            if ( v16 )                                                                                 | 
            {                                                                                          | 
              *(_QWORD *)a7 = 259LL;                                                                   | 
            }                                                                                          | 
            else                                                                                       | 
            {                                                                                          | 
              *(_DWORD *)a7 = 259;                                                                     | 
              *(_QWORD *)(a7 + 8) = 0LL;                                                               | 
            }                                                                                          | 
            v26 = 2;                                                                                   | 
            if ( a12 )                                                                                 | 
            {                                                                                          | 
              v81 = 2;                                                                                 | 
            }                                                                                          | 
            goto LABEL_25;                                                                             | 
          }                                                                                            | 
LABEL_20:                                                                                              | 
          ExRaiseAccessViolation();                                                                    | 
        }                                                                                              | 
      }                                                                                                | 
      ExRaiseDatatypeMisalignment();                                                                   | 
    }                                                                                                  | 
    if ( !a12 || a5 || !a4 )                                                                           | 
    {                                                                                                  | 
      goto LABEL_125;                                                                                  | 
    }                                                                                                  | 
    v81 = 4;                                                                                           | 
    v26 = 2;                                                                                           | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    if ( a12 )                                                                                         | 
    {                                                                                                  | 
      v81 = 3;                                                                                         | 
      if ( a2 )                                                                                        | 
      {                                                                                                | 
        goto LABEL_125;                                                                                | 
      }                                                                                                | 
    }                                                                                                  | 
    v18 = a8;                                                                                          | 
    v26 = 2;                                                                                           | 
  }                                                                                                    | 
LABEL_25:                                                                                              | 
  if ( v18 != (v18 & 0x1000000F) )                                                                     | 
  {                                                                                                    | 
    goto LABEL_125;                                                                                    | 
  }                                                                                                    | 
  v27 = CmObReferenceObjectByHandle(a1, 16, v18, PreviousMode, (__int64)&v86, 0LL);                    | 
  if ( v27 < 0 )                                                                                       | 
  {                                                                                                    | 
    CmpReleaseShutdownRundown();                                                                       | 
    CmpCleanupThreadInfo(&v88);                                                                        | 
    return (unsigned int)v27;                                                                          | 
  }                                                                                                    | 
  v28 = 0LL;                                                                                           | 
  v84 = 0LL;                                                                                           | 
  if ( a2 == 1 )                                                                                       | 
  {                                                                                                    | 
    CmpInitializeParseContext(v91);                                                                    | 
    v33 = CmObReferenceObjectByName(v87, 0, 16, v53, PreviousMode, (__int64)v91, (__int64)&v84);       | 
    CmpCleanupParseContext(v91, 0LL);                                                                  | 
    v29 = v86;                                                                                         | 
    if ( v33 < 0 )                                                                                     | 
    {                                                                                                  | 
      goto LABEL_55;                                                                                   | 
    }                                                                                                  | 
    v28 = v84;                                                                                         | 
    if ( *(_QWORD *)(*((_QWORD *)v86 + 1) + 32LL) == *(_QWORD *)(*((_QWORD *)v84 + 1) + 32LL) )        | 
    {                                                                                                  | 
      ObfDereferenceObject(v84);                                                                       | 
      v33 = -1073741811;                                                                               | 
      goto LABEL_55;                                                                                   | 
    }                                                                                                  | 
    v15 = Object;                                                                                      | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    v29 = v86;                                                                                         | 
  }                                                                                                    | 
  PostBlock = CmpAllocatePostBlock(v81, 0x10000LL, 0LL, 0LL);                                          | 
  v31 = PostBlock;                                                                                     | 
  v87 = PostBlock;                                                                                     | 
  if ( !PostBlock )                                                                                    | 
  {                                                                                                    | 
    if ( a2 == 1 )                                                                                     | 
    {                                                                                                  | 
      ObfDereferenceObject(v28);                                                                       | 
    }                                                                                                  | 
    v33 = -1073741670;                                                                                 | 
    goto LABEL_55;                                                                                     | 
  }                                                                                                    | 
  if ( a2 == 1 )                                                                                       | 
  {                                                                                                    | 
    P = (__int64 *)CmpAllocatePostBlock(v81, 0LL, v28, PostBlock);                                     | 
    if ( !P )                                                                                          | 
    {                                                                                                  | 
      ObfDereferenceObject(v28);                                                                       | 
      CmpFreePostBlock((PVOID)v31);                                                                    | 
      v33 = -1073741670;                                                                               | 
      goto LABEL_55;                                                                                   | 
    }                                                                                                  | 
  }                                                                                                    | 
  v32 = v81;                                                                                           | 
  if ( v81 != 1 )                                                                                      | 
  {                                                                                                    | 
    if ( Handle )                                                                                      | 
    {                                                                                                  | 
      Object = 0LL;                                                                                    | 
      v33 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExEventObjectType, v79, &Object, 0LL); | 
      if ( v33 < 0 )                                                                                   | 
      {                                                                                                | 
        if ( a2 == 1 )                                                                                 | 
        {                                                                                              | 
          CmpFreePostBlock(P);                                                                         | 
        }                                                                                              | 
        v43 = *(_QWORD *)(v31 + 48);                                                                   | 
        if ( v43 )                                                                                     | 
        {                                                                                              | 
          ObDereferenceObjectDeferDelete(*(PVOID *)(v43 + 16));                                        | 
          ExFreePoolWithTag(*(PVOID *)(v31 + 48), 0);                                                  | 
        }                                                                                              | 
        if ( (*(_DWORD *)(v31 + 56) & 0x10000) != 0 )                                                  | 
        {                                                                                              | 
          ExFreePoolWithTag(*(PVOID *)(v31 + 64), 0);                                                  | 
        }                                                                                              | 
        ExFreePoolWithTag((PVOID)v31, 0);                                                              | 
        goto LABEL_55;                                                                                 | 
      }                                                                                                | 
      v15 = Object;                                                                                    | 
      KeResetEvent((PRKEVENT)Object);                                                                  | 
      v32 = v81;                                                                                       | 
    }                                                                                                  | 
    v34 = *(_QWORD **)(v31 + 64);                                                                      | 
    if ( v32 == 2 )                                                                                    | 
    {                                                                                                  | 
      v34[13] = a7;                                                                                    | 
      *(_QWORD *)(*(_QWORD *)(v31 + 64) + 8LL) = v15;                                                  | 
      v35 = v79;                                                                                       | 
      if ( !a5 )                                                                                       | 
      {                                                                                                | 
        v35 = 0;                                                                                       | 
      }                                                                                                | 
      CurrentThread = (unsigned int)KeGetCurrentThread();                                              | 
      v37 = AlpcMessageDeleteProcedure;                                                                | 
      if ( a5 )                                                                                        | 
      {                                                                                                | 
        v37 = a5;                                                                                      | 
      }                                                                                                | 
      KeInitializeApc(                                                                                 | 
        *(_QWORD *)(v31 + 64) + 16,                                                                    | 
        CurrentThread,                                                                                 | 
        2,                                                                                             | 
        (unsigned int)CmpPostApc,                                                                      | 
        (__int64)CmpPostApcRunDown,                                                                    | 
        (__int64)v37,                                                                                  | 
        v35,                                                                                           | 
        a6);                                                                                           | 
      v29 = v86;                                                                                       | 
      v28 = v84;                                                                                       | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      *v34 = v15;                                                                                      | 
      if ( v32 != 4 )                                                                                  | 
      {                                                                                                | 
        *(_QWORD *)(*(_QWORD *)(v31 + 64) + 8LL) = a5;                                                 | 
        *(_DWORD *)(*(_QWORD *)(v31 + 64) + 16LL) = a6;                                                | 
      }                                                                                                | 
    }                                                                                                  | 
  }                                                                                                    | 
  CmpLockRegistry();                                                                                   | 
  v38 = v29[1];                                                                                        | 
  if ( a2 == 1 )                                                                                       | 
  {                                                                                                    | 
    CmpLockTwoKcbsShared(v38, v28[1]);                                                                 | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    CmpLockKcbShared(v38);                                                                             | 
  }                                                                                                    | 
  if ( (unsigned __int8)CmpIsKeyDeletedForKeyBody(v29, 0LL) )                                          | 
  {                                                                                                    | 
LABEL_44:                                                                                              | 
    v39 = v29[1];                                                                                      | 
    if ( a2 == 1 )                                                                                     | 
    {                                                                                                  | 
      CmpUnlockTwoKcbs(v39, v28[1]);                                                                   | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmpUnlockKcb(v39);                                                                               | 
    }                                                                                                  | 
    CmpUnlockRegistry();                                                                               | 
    if ( v15 )                                                                                         | 
    {                                                                                                  | 
      ObfDereferenceObject(v15);                                                                       | 
    }                                                                                                  | 
    if ( a2 == 1 )                                                                                     | 
    {                                                                                                  | 
      CmpFreePostBlock(P);                                                                             | 
    }                                                                                                  | 
    v40 = *(_QWORD *)(v31 + 48);                                                                       | 
    if ( v40 )                                                                                         | 
    {                                                                                                  | 
      ObDereferenceObjectDeferDelete(*(PVOID *)(v40 + 16));                                            | 
      ExFreePoolWithTag(*(PVOID *)(v31 + 48), 0);                                                      | 
    }                                                                                                  | 
    if ( (*(_DWORD *)(v31 + 56) & 0x10000) != 0 )                                                      | 
    {                                                                                                  | 
      ExFreePoolWithTag(*(PVOID *)(v31 + 64), 0);                                                      | 
    }                                                                                                  | 
    ExFreePoolWithTag((PVOID)v31, 0);                                                                  | 
    v33 = -1073741444;                                                                                 | 
    goto LABEL_55;                                                                                     | 
  }                                                                                                    | 
  if ( a2 != 1 )                                                                                       | 
  {                                                                                                    | 
    goto LABEL_74;                                                                                     | 
  }                                                                                                    | 
  if ( (unsigned __int8)CmpIsKeyDeletedForKeyBody(v28, 0LL) )                                          | 
  {                                                                                                    | 
    goto LABEL_44;                                                                                     | 
  }                                                                                                    | 
  v54 = *(_QWORD *)(v29[1] + 32LL);                                                                    | 
  v55 = v28[1];                                                                                        | 
  v56 = *(_QWORD *)(v55 + 32);                                                                         | 
  if ( v54 == v56 )                                                                                    | 
  {                                                                                                    | 
LABEL_74:                                                                                              | 
    CmLockHive(*(_QWORD *)(v29[1] + 32LL));                                                            | 
    v26 = 1;                                                                                           | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    if ( v54 >= v56 )                                                                                  | 
    {                                                                                                  | 
      CmLockHive(*(_QWORD *)(v55 + 32));                                                               | 
      CmLockHive(*(_QWORD *)(v29[1] + 32LL));                                                          | 
      v26 = 3;                                                                                         | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmLockHive(v54);                                                                                 | 
      CmLockHive(*(_QWORD *)(v28[1] + 32LL));                                                          | 
    }                                                                                                  | 
    v85 = *(_QWORD *)(v28[1] + 32LL);                                                                  | 
  }                                                                                                    | 
  ExAcquireFastMutexUnsafe(&CmpPostLock);                                                              | 
  v33 = CmpNotifyChangeKey(v29, v31, a8, a9);                                                          | 
  if ( v33 < 0 )                                                                                       | 
  {                                                                                                    | 
    ExReleaseFastMutexUnsafe(&CmpPostLock);                                                            | 
    if ( v26 == 1 )                                                                                    | 
    {                                                                                                  | 
      v61 = *(_QWORD *)(v29[1] + 32LL);                                                                | 
    }                                                                                                  | 
    else if ( v26 == 2 )                                                                               | 
    {                                                                                                  | 
      CmUnlockHive(v85);                                                                               | 
      v61 = *(_QWORD *)(v29[1] + 32LL);                                                                | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmUnlockHive(*(_QWORD *)(v29[1] + 32LL));                                                        | 
      v61 = v85;                                                                                       | 
    }                                                                                                  | 
    CmUnlockHive(v61);                                                                                 | 
    v67 = v29[1];                                                                                      | 
    if ( a2 == 1 )                                                                                     | 
    {                                                                                                  | 
      CmpUnlockTwoKcbs(v67, v28[1]);                                                                   | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmpUnlockKcb(v67);                                                                               | 
    }                                                                                                  | 
    CmpUnlockRegistry();                                                                               | 
    if ( Object )                                                                                      | 
    {                                                                                                  | 
      ObfDereferenceObject(Object);                                                                    | 
    }                                                                                                  | 
    if ( a2 == 1 )                                                                                     | 
    {                                                                                                  | 
      CmpFreePostBlock(P);                                                                             | 
    }                                                                                                  | 
    goto LABEL_55;                                                                                     | 
  }                                                                                                    | 
  v78 = a2 == 1;                                                                                       | 
  if ( a2 == 1 )                                                                                       | 
  {                                                                                                    | 
    PsReferenceSiloContext(v28);                                                                       | 
    if ( v33 )                                                                                         | 
    {                                                                                                  | 
      v33 = CmpNotifyChangeKey(v28, P, a8, a9);                                                        | 
      v78 = a2 == 1;                                                                                   | 
      if ( v33 < 0 )                                                                                   | 
      {                                                                                                | 
        v68 = *(_QWORD *)v31;                                                                          | 
        v69 = *(_QWORD **)(v31 + 8);                                                                   | 
        if ( *(_QWORD *)(*(_QWORD *)v31 + 8LL) != v31 || *v69 != v31 )                                 | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v69 = v68;                                                                                    | 
        *(_QWORD *)(v68 + 8) = v69;                                                                    | 
        CurrentIrql = (void *)KeGetCurrentIrql();                                                      | 
        v84 = CurrentIrql;                                                                             | 
        __writecr8(1uLL);                                                                              | 
        if ( KiIrqlFlags )                                                                             | 
        {                                                                                              | 
          KiRaiseIrqlProcessIrqlFlags((unsigned __int8)CurrentIrql, 1LL);                              | 
          LOBYTE(CurrentIrql) = (_BYTE)v84;                                                            | 
        }                                                                                              | 
        v71 = *(_QWORD *)(v31 + 16);                                                                   | 
        v72 = *(_QWORD **)(v31 + 24);                                                                  | 
        if ( *(_QWORD *)(v71 + 8) != v31 + 16 || *v72 != v31 + 16 )                                    | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v72 = v71;                                                                                    | 
        *(_QWORD *)(v71 + 8) = v72;                                                                    | 
        if ( KiIrqlFlags )                                                                             | 
        {                                                                                              | 
          KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), (unsigned __int8)CurrentIrql);               | 
          LOBYTE(CurrentIrql) = (_BYTE)v84;                                                            | 
        }                                                                                              | 
        __writecr8((unsigned __int8)CurrentIrql);                                                      | 
        v78 = a2 == 1;                                                                                 | 
      }                                                                                                | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmpFreePostBlock(P);                                                                             | 
      v78 = 0;                                                                                         | 
    }                                                                                                  | 
  }                                                                                                    | 
  ExReleaseFastMutexUnsafe(&CmpPostLock);                                                              | 
  if ( v26 == 1 )                                                                                      | 
  {                                                                                                    | 
    v45 = *(_QWORD *)(v29[1] + 32LL);                                                                  | 
  }                                                                                                    | 
  else if ( v26 == 2 )                                                                                 | 
  {                                                                                                    | 
    CmUnlockHive(v85);                                                                                 | 
    v45 = *(_QWORD *)(v29[1] + 32LL);                                                                  | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    CmUnlockHive(*(_QWORD *)(v29[1] + 32LL));                                                          | 
    v45 = v85;                                                                                         | 
  }                                                                                                    | 
  CmUnlockHive(v45);                                                                                   | 
  v46 = v29[1];                                                                                        | 
  if ( a2 == 1 )                                                                                       | 
  {                                                                                                    | 
    CmpUnlockTwoKcbs(v46, v28[1]);                                                                     | 
  }                                                                                                    | 
  else                                                                                                 | 
  {                                                                                                    | 
    CmpUnlockKcb(v46);                                                                                 | 
  }                                                                                                    | 
  CmpUnlockRegistry();                                                                                 | 
  if ( v28 )                                                                                           | 
  {                                                                                                    | 
    ObfDereferenceObject(v28);                                                                         | 
  }                                                                                                    | 
  if ( v33 < 0 )                                                                                       | 
  {                                                                                                    | 
    CmpFreePostBlock((PVOID)v31);                                                                      | 
    if ( Object )                                                                                      | 
    {                                                                                                  | 
      ObfDereferenceObject(Object);                                                                    | 
    }                                                                                                  | 
    goto LABEL_55;                                                                                     | 
  }                                                                                                    | 
  if ( v81 == 1 )                                                                                      | 
  {                                                                                                    | 
    CmpReleaseShutdownRundown();                                                                       | 
    v41 = 0;                                                                                           | 
    v47 = KeWaitForSingleObject(*(PVOID *)(v31 + 64), Executive, v79, 1u, 0LL);                        | 
    v33 = v47;                                                                                         | 
    if ( v47 == 257 || v47 == 192 )                                                                    | 
    {                                                                                                  | 
      CmpLockRegistry();                                                                               | 
      ExAcquireFastMutexUnsafe(&CmpPostLock);                                                          | 
      v48 = KeGetCurrentIrql();                                                                        | 
      __writecr8(1uLL);                                                                                | 
      if ( KiIrqlFlags )                                                                               | 
      {                                                                                                | 
        KiRaiseIrqlProcessIrqlFlags(v48, 1LL);                                                         | 
      }                                                                                                | 
      if ( v78 )                                                                                       | 
      {                                                                                                | 
        v65 = *P;                                                                                      | 
        if ( *P )                                                                                      | 
        {                                                                                              | 
          v66 = (__int64 **)P[1];                                                                      | 
          if ( *(__int64 **)(v65 + 8) != P || *v66 != P )                                              | 
          {                                                                                            | 
            goto LABEL_95;                                                                             | 
          }                                                                                            | 
          *v66 = (__int64 *)v65;                                                                       | 
          *(_QWORD *)(v65 + 8) = v66;                                                                  | 
        }                                                                                              | 
        v75 = P[2];                                                                                    | 
        v76 = (__int64 **)P[3];                                                                        | 
        if ( *(__int64 **)(v75 + 8) != P + 2 || *v76 != P + 2 )                                        | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v76 = (__int64 *)v75;                                                                         | 
        *(_QWORD *)(v75 + 8) = v76;                                                                    | 
      }                                                                                                | 
      v49 = *(_QWORD *)v31;                                                                            | 
      if ( *(_QWORD *)v31 )                                                                            | 
      {                                                                                                | 
        v50 = *(_QWORD **)(v31 + 8);                                                                   | 
        if ( *(_QWORD *)(v49 + 8) != v31 || *v50 != v31 )                                              | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v50 = v49;                                                                                    | 
        *(_QWORD *)(v49 + 8) = v50;                                                                    | 
      }                                                                                                | 
      v51 = *(_QWORD *)(v31 + 16);                                                                     | 
      v52 = *(_QWORD **)(v31 + 24);                                                                    | 
      if ( *(_QWORD *)(v51 + 8) == v31 + 16 && *v52 == v31 + 16 )                                      | 
      {                                                                                                | 
        *v52 = v51;                                                                                    | 
        *(_QWORD *)(v51 + 8) = v52;                                                                    | 
        if ( KiIrqlFlags )                                                                             | 
        {                                                                                              | 
          KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v48);                                        | 
        }                                                                                              | 
        __writecr8(v48);                                                                               | 
        ExReleaseFastMutexUnsafe(&CmpPostLock);                                                        | 
        CmpUnlockRegistry();                                                                           | 
        if ( v78 )                                                                                     | 
        {                                                                                              | 
          CmpFreePostBlock(P);                                                                         | 
        }                                                                                              | 
        goto LABEL_143;                                                                                | 
      }                                                                                                | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      CmpLockRegistry();                                                                               | 
      ExAcquireFastMutexUnsafe(&CmpPostLock);                                                          | 
      v57 = KeGetCurrentIrql();                                                                        | 
      __writecr8(1uLL);                                                                                | 
      if ( KiIrqlFlags )                                                                               | 
      {                                                                                                | 
        KiRaiseIrqlProcessIrqlFlags(v57, 1LL);                                                         | 
      }                                                                                                | 
      if ( v78 )                                                                                       | 
      {                                                                                                | 
        v62 = *P;                                                                                      | 
        if ( *P )                                                                                      | 
        {                                                                                              | 
          v63 = (__int64 **)P[1];                                                                      | 
          if ( *(__int64 **)(v62 + 8) != P || *v63 != P )                                              | 
          {                                                                                            | 
            goto LABEL_95;                                                                             | 
          }                                                                                            | 
          *v63 = (__int64 *)v62;                                                                       | 
          *(_QWORD *)(v62 + 8) = v63;                                                                  | 
        }                                                                                              | 
        v73 = P[2];                                                                                    | 
        v74 = (__int64 **)P[3];                                                                        | 
        if ( *(__int64 **)(v73 + 8) != P + 2 || *v74 != P + 2 )                                        | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v74 = (__int64 *)v73;                                                                         | 
        *(_QWORD *)(v73 + 8) = v74;                                                                    | 
      }                                                                                                | 
      v58 = *(_QWORD *)v31;                                                                            | 
      if ( *(_QWORD *)v31 )                                                                            | 
      {                                                                                                | 
        v64 = *(_QWORD **)(v31 + 8);                                                                   | 
        if ( *(_QWORD *)(v58 + 8) != v31 || *v64 != v31 )                                              | 
        {                                                                                              | 
          goto LABEL_95;                                                                               | 
        }                                                                                              | 
        *v64 = v58;                                                                                    | 
        *(_QWORD *)(v58 + 8) = v64;                                                                    | 
      }                                                                                                | 
      v59 = *(_QWORD *)(v31 + 16);                                                                     | 
      v60 = *(_QWORD **)(v31 + 24);                                                                    | 
      if ( *(_QWORD *)(v59 + 8) == v31 + 16 && *v60 == v31 + 16 )                                      | 
      {                                                                                                | 
        *v60 = v59;                                                                                    | 
        *(_QWORD *)(v59 + 8) = v60;                                                                    | 
        if ( KiIrqlFlags )                                                                             | 
        {                                                                                              | 
          KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v57);                                        | 
        }                                                                                              | 
        __writecr8(v57);                                                                               | 
        ExReleaseFastMutexUnsafe(&CmpPostLock);                                                        | 
        CmpUnlockRegistry();                                                                           | 
        v33 = *(_DWORD *)(*(_QWORD *)(v31 + 64) + 24LL);                                               | 
        *(_DWORD *)a7 = v33;                                                                           | 
        if ( v80 )                                                                                     | 
        {                                                                                              | 
          *(_DWORD *)(a7 + 4) = 0;                                                                     | 
        }                                                                                              | 
        else                                                                                           | 
        {                                                                                              | 
          *(_QWORD *)(a7 + 8) = 0LL;                                                                   | 
        }                                                                                              | 
        if ( v78 )                                                                                     | 
        {                                                                                              | 
          CmpFreePostBlock(P);                                                                         | 
        }                                                                                              | 
LABEL_143:                                                                                             | 
        CmpFreePostBlock((PVOID)v31);                                                                  | 
        goto LABEL_56;                                                                                 | 
      }                                                                                                | 
    }                                                                                                  | 
LABEL_95:                                                                                              | 
    __fastfail(3u);                                                                                    | 
  }                                                                                                    | 
LABEL_55:                                                                                              | 
  v41 = v77;                                                                                           | 
LABEL_56:                                                                                              | 
  ObfDereferenceObject(v29);                                                                           | 
  if ( v41 )                                                                                           | 
  {                                                                                                    | 
    CmpReleaseShutdownRundown();                                                                       | 
  }                                                                                                    | 
  CmpCleanupThreadInfo(&v88);                                                                          | 
  return (unsigned int)v33;                                                                            | 
}                                                                                                      | 
627 row(s)
