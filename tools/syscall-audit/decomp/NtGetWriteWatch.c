__int64 __fastcall NtGetWriteWatch(                                                                                       | 
        HANDLE Handle,                                                                                                    | 
        int a2,                                                                                                           | 
        unsigned __int64 a3,                                                                                              | 
        unsigned __int64 a4,                                                                                              | 
        volatile void *Address,                                                                                           | 
        _QWORD *a6,                                                                                                       | 
        unsigned __int64 a7)                                                                                              | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v69 = a2;                                                                                                               | 
  v83 = Handle;                                                                                                           | 
  v86 = (_DWORD *)a7;                                                                                                     | 
  v85 = a6;                                                                                                               | 
  v84 = (void *)Address;                                                                                                  | 
  Object = 0LL;                                                                                                           | 
  v72 = 0;                                                                                                                | 
  LODWORD(v66) = 0;                                                                                                       | 
  memset(v87, 0, sizeof(v87));                                                                                            | 
  ProcessorFlushList = 0LL;                                                                                               | 
  if ( (a2 & 0xFFFFFFFE) != 0 )                                                                                           | 
  {                                                                                                                       | 
    return 3221225712LL;                                                                                                  | 
  }                                                                                                                       | 
  CurrentThread = KeGetCurrentThread();                                                                                   | 
  Process = CurrentThread->ApcState.Process;                                                                              | 
  PreviousMode = CurrentThread->PreviousMode;                                                                             | 
  if ( PreviousMode )                                                                                                     | 
  {                                                                                                                       | 
    if ( a3 > 0x7FFFFFFEFFFFLL )                                                                                          | 
    {                                                                                                                     | 
      return 3221225713LL;                                                                                                | 
    }                                                                                                                     | 
    v14 = 0x7FFFFFFF0000LL;                                                                                               | 
    if ( 0x7FFFFFFF0000LL - a3 < a4 )                                                                                     | 
    {                                                                                                                     | 
      return 3221225714LL;                                                                                                | 
    }                                                                                                                     | 
    v15 = 0x7FFFFFFF0000LL;                                                                                               | 
    if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                                        | 
    {                                                                                                                     | 
      v15 = (__int64)a6;                                                                                                  | 
    }                                                                                                                     | 
    *(_QWORD *)v15 = *(_QWORD *)v15;                                                                                      | 
    v16 = *a6;                                                                                                            | 
    v77 = v16;                                                                                                            | 
    if ( !v16 )                                                                                                           | 
    {                                                                                                                     | 
      return 3221225715LL;                                                                                                | 
    }                                                                                                                     | 
    if ( v16 > 0x1FFFFFFFFFFFFFFFLL )                                                                                     | 
    {                                                                                                                     | 
      return 3221225715LL;                                                                                                | 
    }                                                                                                                     | 
    ProbeForWrite(Address, 8 * v16, 8u);                                                                                  | 
    if ( a7 < 0x7FFFFFFF0000LL )                                                                                          | 
    {                                                                                                                     | 
      v14 = a7;                                                                                                           | 
    }                                                                                                                     | 
    *(_DWORD *)v14 = *(_DWORD *)v14;                                                                                      | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v77 = *a6;                                                                                                            | 
  }                                                                                                                       | 
  Pool = v88;                                                                                                             | 
  Src = v88;                                                                                                              | 
  if ( v77 > 0x100 )                                                                                                      | 
  {                                                                                                                       | 
    Pool = (_BYTE *)MiAllocatePool(0x41uLL, 8 * v77);                                                                     | 
    Src = Pool;                                                                                                           | 
    if ( !Pool )                                                                                                          | 
    {                                                                                                                     | 
      return 3221225626LL;                                                                                                | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v75 = 1LL;                                                                                                              | 
  v18 = 0LL;                                                                                                              | 
  v76 = (__int64 *)Pool;                                                                                                  | 
  P = 0LL;                                                                                                                | 
  v19 = 0;                                                                                                                | 
  if ( Handle == (HANDLE)-1LL )                                                                                           | 
  {                                                                                                                       | 
    Object = Process;                                                                                                     | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v20 = ObReferenceObjectByHandleWithTag(                                                                               | 
            Handle,                                                                                                       | 
            8u,                                                                                                           | 
            (POBJECT_TYPE)PsProcessType,                                                                                  | 
            PreviousMode,                                                                                                 | 
            0x77576D4Du,                                                                                                  | 
            &Object,                                                                                                      | 
            0LL);                                                                                                         | 
    LODWORD(v66) = v20;                                                                                                   | 
    if ( v20 < 0 )                                                                                                        | 
    {                                                                                                                     | 
LABEL_119:                                                                                                                | 
      v63 = Src;                                                                                                          | 
      goto LABEL_120;                                                                                                     | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v74 = 0LL;                                                                                                              | 
  v21 = a4 + a3 - 1;                                                                                                      | 
  if ( a3 > v21 )                                                                                                         | 
  {                                                                                                                       | 
    v20 = -1073741582;                                                                                                    | 
    goto LABEL_109;                                                                                                       | 
  }                                                                                                                       | 
  if ( Process != Object )                                                                                                | 
  {                                                                                                                       | 
    KiStackAttachProcess((ULONG_PTR)Object);                                                                              | 
    v19 = 1;                                                                                                              | 
  }                                                                                                                       | 
  v68 = (char *)Object + 1024;                                                                                            | 
  v22 = ((a3 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                                   | 
  v80 = ((v21 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                                  | 
  v23 = MiObtainReferencedVadEx(a3, 0LL, &v66);                                                                           | 
  if ( !v23 )                                                                                                             | 
  {                                                                                                                       | 
    v20 = v66;                                                                                                            | 
    if ( (_DWORD)v66 != -1073741664 )                                                                                     | 
    {                                                                                                                     | 
      goto LABEL_109;                                                                                                     | 
    }                                                                                                                     | 
    goto LABEL_108;                                                                                                       | 
  }                                                                                                                       | 
  P = (PVOID)v23;                                                                                                         | 
  if ( (*(_DWORD *)(v23 + 48) & 0x600000) != 0x600000                                                                     | 
    || v21 > (((*(unsigned int *)(v23 + 28) | ((unsigned __int64)*(unsigned __int8 *)(v23 + 33) << 32)) << 12) | 0xFFF) ) | 
  {                                                                                                                       | 
LABEL_108:                                                                                                                | 
    v20 = -1073741585;                                                                                                    | 
    goto LABEL_109;                                                                                                       | 
  }                                                                                                                       | 
  VadMandatoryPageSize = MiGetVadMandatoryPageSize(v23);                                                                  | 
  v28 = VadMandatoryPageSize;                                                                                             | 
  v75 = VadMandatoryPageSize;                                                                                             | 
  if ( VadMandatoryPageSize > 1 )                                                                                         | 
  {                                                                                                                       | 
    v29 = (VadMandatoryPageSize << 12) - 1;                                                                               | 
    if ( (a3 & v29) != 0 )                                                                                                | 
    {                                                                                                                     | 
      v20 = -1073741583;                                                                                                  | 
      goto LABEL_109;                                                                                                     | 
    }                                                                                                                     | 
    if ( (a4 & v29) != 0 )                                                                                                | 
    {                                                                                                                     | 
      v20 = -1073741582;                                                                                                  | 
      goto LABEL_109;                                                                                                     | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  if ( (v26 & 0xA00000) == 0xA00000 )                                                                                     | 
  {                                                                                                                       | 
    v30 = 16LL;                                                                                                           | 
    if ( MiVadPageSizes[(v27 >> 19) & 3] != 16 )                                                                          | 
    {                                                                                                                     | 
      v30 = 1LL;                                                                                                          | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v30 = 1LL;                                                                                                            | 
  }                                                                                                                       | 
  v78 = v30;                                                                                                              | 
  LockedVadEvent = MiLocateLockedVadEvent(v25, 4LL);                                                                      | 
  v32 = (a3 >> 12) - (*(unsigned int *)(v31 + 24) | ((unsigned __int64)*(unsigned __int8 *)(v31 + 32) << 32));            | 
  v33 = MiGetVadMandatoryPageSize(v31);                                                                                   | 
  v34 = v32 % v33;                                                                                                        | 
  v66 = v32 / v33;                                                                                                        | 
  v35 = v68;                                                                                                              | 
  v65 = MiLockWorkingSetShared(v68, v34);                                                                                 | 
  v37 = (_QWORD *)v80;                                                                                                    | 
  if ( v22 > v80 )                                                                                                        | 
  {                                                                                                                       | 
    goto LABEL_101;                                                                                                       | 
  }                                                                                                                       | 
  SpinLock = (PEX_SPIN_LOCK)(LockedVadEvent + 8);                                                                         | 
  while ( 2 )                                                                                                             | 
  {                                                                                                                       | 
    if ( v18 )                                                                                                            | 
    {                                                                                                                     | 
      if ( ProcessorFlushList )                                                                                           | 
      {                                                                                                                   | 
        MiFlushTbList(ProcessorFlushList);                                                                                | 
        MiReleaseProcessorFlushList();                                                                                    | 
        ProcessorFlushList = 0LL;                                                                                         | 
      }                                                                                                                   | 
      v35 = v68;                                                                                                          | 
      MiUnlockPageTableInternal(v68, v18);                                                                                | 
      v18 = 0LL;                                                                                                          | 
    }                                                                                                                     | 
    if ( (unsigned int)MiWorkingSetIsContended(v35, 0LL) || KeShouldYieldProcessor() )                                    | 
    {                                                                                                                     | 
      LOBYTE(v38) = v65;                                                                                                  | 
      MiUnlockWorkingSetShared(v35, v38);                                                                                 | 
      MiLockWorkingSetShared(v35, v40);                                                                                   | 
    }                                                                                                                     | 
    LOBYTE(v39) = v65;                                                                                                    | 
    NextPageTable = MiGetNextPageTable(v22, (_DWORD)v37, v39, 1, (__int64)&v72);                                          | 
    v42 = NextPageTable;                                                                                                  | 
    if ( NextPageTable )                                                                                                  | 
    {                                                                                                                     | 
      v18 = ((NextPageTable >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                    | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      v42 = (unsigned __int64)(v37 + 1);                                                                                  | 
    }                                                                                                                     | 
    v43 = ((__int64)(v42 - v22) >> 3) / v28;                                                                              | 
    v19 |= 4u;                                                                                                            | 
    ExAcquireSpinLockExclusiveAtDpcLevel(SpinLock);                                                                       | 
    v44 = v66;                                                                                                            | 
    v45 = LockedVadEvent;                                                                                                 | 
    if ( v43 )                                                                                                            | 
    {                                                                                                                     | 
      while ( 1 )                                                                                                         | 
      {                                                                                                                   | 
        if ( _bittest64(*(const signed __int64 **)(v45 + 24), v44) == 1 )                                                 | 
        {                                                                                                                 | 
          if ( (v69 & 1) != 0 )                                                                                           | 
          {                                                                                                               | 
            _bittestandreset64(*(signed __int64 **)(v45 + 24), v44);                                                      | 
          }                                                                                                               | 
          v46 = v76;                                                                                                      | 
          *v76 = (__int64)(v22 << 25) >> 16;                                                                              | 
          v76 = v46 + 1;                                                                                                  | 
          v36 = v74 + 1;                                                                                                  | 
          v74 = v36;                                                                                                      | 
          if ( v36 == v77 )                                                                                               | 
          {                                                                                                               | 
            break;                                                                                                        | 
          }                                                                                                               | 
        }                                                                                                                 | 
        v66 = ++v44;                                                                                                      | 
        v22 += 8 * v28;                                                                                                   | 
        if ( !--v43 )                                                                                                     | 
        {                                                                                                                 | 
          goto LABEL_61;                                                                                                  | 
        }                                                                                                                 | 
      }                                                                                                                   | 
LABEL_100:                                                                                                                | 
      v35 = v68;                                                                                                          | 
      break;                                                                                                              | 
    }                                                                                                                     | 
LABEL_61:                                                                                                                 | 
    if ( v22 > (unsigned __int64)v37 )                                                                                    | 
    {                                                                                                                     | 
      goto LABEL_100;                                                                                                     | 
    }                                                                                                                     | 
    if ( v72 )                                                                                                            | 
    {                                                                                                                     | 
      v22 = v18;                                                                                                          | 
      v47 = ((v80 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                              | 
      v48 = 512LL;                                                                                                        | 
      if ( v72 > 1 )                                                                                                      | 
      {                                                                                                                   | 
        v49 = v72 - 1;                                                                                                    | 
        do                                                                                                                | 
        {                                                                                                                 | 
          v48 <<= 9;                                                                                                      | 
          v22 = ((v22 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                          | 
          v47 = ((v47 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                          | 
          --v49;                                                                                                          | 
        }                                                                                                                 | 
        while ( v49 );                                                                                                    | 
      }                                                                                                                   | 
      v18 = ((v22 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                              | 
      for ( i = (_QWORD *)(v22 + 8);                                                                                      | 
            ((unsigned __int16)i & 0xFFF) != 0 && (unsigned __int64)i <= v47 && (*i & 0x81) == 0x81;                      | 
            ++i )                                                                                                         | 
      {                                                                                                                   | 
        ;                                                                                                                 | 
      }                                                                                                                   | 
      v37 = i - 1;                                                                                                        | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      v48 = v30;                                                                                                          | 
    }                                                                                                                     | 
    v79 = v48 / v28;                                                                                                      | 
    while ( v22 <= (unsigned __int64)v37 )                                                                                | 
    {                                                                                                                     | 
      LeafVa = MiGetLeafVa(v22);                                                                                          | 
      v19 &= ~2u;                                                                                                         | 
      v56 = (_QWORD *)v22;                                                                                                | 
      if ( _bittest64(*(const signed __int64 **)(v54 + 24), v53) == 1 )                                                   | 
      {                                                                                                                   | 
        v19 |= 2u;                                                                                                        | 
        if ( (v69 & 1) == 0 )                                                                                             | 
        {                                                                                                                 | 
          goto LABEL_91;                                                                                                  | 
        }                                                                                                                 | 
        _bittestandreset64(*(signed __int64 **)(v54 + 24), v53);                                                          | 
        v57 = 0;                                                                                                          | 
        do                                                                                                                | 
        {                                                                                                                 | 
          LOBYTE(v51) = (*v56 & 0x42) != 0;                                                                               | 
          if ( ((unsigned __int8)v51 & ((*v56 & 1) != 0)) != 0 )                                                          | 
          {                                                                                                               | 
            v58 = ProcessorFlushList;                                                                                     | 
            if ( !ProcessorFlushList )                                                                                    | 
            {                                                                                                             | 
              ProcessorFlushList = MiGetProcessorFlushList();                                                             | 
              MiInitializeTbFlushList(ProcessorFlushList, (_DWORD)v68, *(_DWORD *)(ProcessorFlushList + 12), 0, 10);      | 
              v58 = v51;                                                                                                  | 
            }                                                                                                             | 
            MiMakePteClean(v51, v56, v58);                                                                                | 
          }                                                                                                               | 
          ++v56;                                                                                                          | 
          ++v57;                                                                                                          | 
        }                                                                                                                 | 
        while ( v57 < v78 );                                                                                              | 
      }                                                                                                                   | 
      else                                                                                                                | 
      {                                                                                                                   | 
        v59 = 0;                                                                                                          | 
        do                                                                                                                | 
        {                                                                                                                 | 
          LOBYTE(v51) = (*v56 & 0x42) != 0;                                                                               | 
          if ( ((unsigned __int8)v51 & ((*v56 & 1) != 0)) != 0 )                                                          | 
          {                                                                                                               | 
            v19 |= 2u;                                                                                                    | 
            if ( (v69 & 1) == 0 )                                                                                         | 
            {                                                                                                             | 
              break;                                                                                                      | 
            }                                                                                                             | 
            v60 = ProcessorFlushList;                                                                                     | 
            if ( !ProcessorFlushList )                                                                                    | 
            {                                                                                                             | 
              ProcessorFlushList = MiGetProcessorFlushList();                                                             | 
              MiInitializeTbFlushList(ProcessorFlushList, (_DWORD)v68, *(_DWORD *)(ProcessorFlushList + 12), 0, 10);      | 
              v60 = v51;                                                                                                  | 
            }                                                                                                             | 
            MiMakePteClean(v51, v56, v60);                                                                                | 
          }                                                                                                               | 
          ++v56;                                                                                                          | 
          ++v59;                                                                                                          | 
        }                                                                                                                 | 
        while ( v59 < v78 );                                                                                              | 
      }                                                                                                                   | 
      v52 = v79;                                                                                                          | 
      v28 = v75;                                                                                                          | 
      v53 = v66;                                                                                                          | 
LABEL_91:                                                                                                                 | 
      if ( (v19 & 2) != 0 )                                                                                               | 
      {                                                                                                                   | 
        v61 = 0LL;                                                                                                        | 
        if ( v52 )                                                                                                        | 
        {                                                                                                                 | 
          v36 = v74;                                                                                                      | 
          v62 = v76;                                                                                                      | 
          do                                                                                                              | 
          {                                                                                                               | 
            *v62++ = LeafVa;                                                                                              | 
            v76 = v62;                                                                                                    | 
            v74 = ++v36;                                                                                                  | 
            if ( v36 == v77 )                                                                                             | 
            {                                                                                                             | 
              goto LABEL_100;                                                                                             | 
            }                                                                                                             | 
            ++v61;                                                                                                        | 
            LeafVa += v28 << 12;                                                                                          | 
          }                                                                                                               | 
          while ( v61 < v52 );                                                                                            | 
        }                                                                                                                 | 
      }                                                                                                                   | 
      v66 = v52 + v53;                                                                                                    | 
      v30 = v78;                                                                                                          | 
      v22 += 8 * v78;                                                                                                     | 
      if ( (v22 & 0xFFF) == 0 )                                                                                           | 
      {                                                                                                                   | 
        break;                                                                                                            | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    ExReleaseSpinLockExclusiveFromDpcLevel(SpinLock);                                                                     | 
    v19 &= ~4u;                                                                                                           | 
    v22 = (((unsigned __int64)MiGetLeafVa(v22) >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                 | 
    v37 = (_QWORD *)v80;                                                                                                  | 
    v35 = v68;                                                                                                            | 
    if ( v22 <= v80 )                                                                                                     | 
    {                                                                                                                     | 
      continue;                                                                                                           | 
    }                                                                                                                     | 
    break;                                                                                                                | 
  }                                                                                                                       | 
LABEL_101:                                                                                                                | 
  if ( ProcessorFlushList )                                                                                               | 
  {                                                                                                                       | 
    MiFlushTbList(ProcessorFlushList);                                                                                    | 
    MiReleaseProcessorFlushList();                                                                                        | 
  }                                                                                                                       | 
  if ( (v19 & 4) != 0 )                                                                                                   | 
  {                                                                                                                       | 
    ExReleaseSpinLockExclusiveFromDpcLevel((PEX_SPIN_LOCK)(LockedVadEvent + 8));                                          | 
  }                                                                                                                       | 
  if ( v18 )                                                                                                              | 
  {                                                                                                                       | 
    MiUnlockPageTableInternal(v35, v18);                                                                                  | 
  }                                                                                                                       | 
  LOBYTE(v36) = v65;                                                                                                      | 
  MiUnlockWorkingSetShared(v35, v36);                                                                                     | 
  v20 = 0;                                                                                                                | 
LABEL_109:                                                                                                                | 
  if ( P )                                                                                                                | 
  {                                                                                                                       | 
    MiUnlockAndDereferenceVad(P);                                                                                         | 
  }                                                                                                                       | 
  if ( (v19 & 1) != 0 )                                                                                                   | 
  {                                                                                                                       | 
    KiUnstackDetachProcess(v87, 0LL);                                                                                     | 
  }                                                                                                                       | 
  if ( v83 != (HANDLE)-1LL )                                                                                              | 
  {                                                                                                                       | 
    ObfDereferenceObjectWithTag(Object, 0x77576D4Du);                                                                     | 
  }                                                                                                                       | 
  if ( v20 )                                                                                                              | 
  {                                                                                                                       | 
    goto LABEL_119;                                                                                                       | 
  }                                                                                                                       | 
  v63 = Src;                                                                                                              | 
  if ( v84 )                                                                                                              | 
  {                                                                                                                       | 
    v64 = v74;                                                                                                            | 
    memmove(v84, Src, 8 * v74);                                                                                           | 
    *v85 = v64;                                                                                                           | 
  }                                                                                                                       | 
  *v86 = (_DWORD)v75 << 12;                                                                                               | 
LABEL_120:                                                                                                                | 
  if ( v63 != v88 )                                                                                                       | 
  {                                                                                                                       | 
    ExFreePoolWithTag(v63, 0);                                                                                            | 
  }                                                                                                                       | 
  return (unsigned int)v20;                                                                                               | 
}                                                                                                                         | 
437 row(s)
