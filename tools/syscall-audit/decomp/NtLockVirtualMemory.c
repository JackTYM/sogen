__int64 __fastcall NtLockVirtualMemory(ULONG_PTR BugCheckParameter1, unsigned __int64 *a2, _QWORD *a3)                 | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v63 = a3;                                                                                                            | 
  v64 = a2;                                                                                                            | 
  memset(v65, 0, sizeof(v65));                                                                                         | 
  v56 = 0LL;                                                                                                           | 
  v54 = 0LL;                                                                                                           | 
  Object = 0LL;                                                                                                        | 
  v4 = 0;                                                                                                              | 
  v45 = 0;                                                                                                             | 
  v48 = 0;                                                                                                             | 
  result = MiLockUnlockCommon(BugCheckParameter1, (__int64)&v54, (__int64)&v56, (__int64)&Object);                     | 
  if ( (int)result < 0 )                                                                                               | 
  {                                                                                                                    | 
    return result;                                                                                                     | 
  }                                                                                                                    | 
  CurrentThread = KeGetCurrentThread();                                                                                | 
  v6 = (v54 + v56 - 1) & 0xFFFFFFFFFFFFF000uLL;                                                                        | 
  v7 = v54 & 0xFFFFFFFFFFFFF000uLL;                                                                                    | 
  v44 = 0LL;                                                                                                           | 
  Pool = (PVOID)MiAllocatePool(                                                                                        | 
                  0x40uLL,                                                                                             | 
                  8                                                                                                    | 
                * (((((v6 - (v54 & 0xFFFFFFFFFFFFF000uLL)) >> 12) + 1) >> 6)                                           | 
                 + ((((unsigned __int8)((v6 - (v54 & 0xFFFFFFFFFFFFF000uLL)) >> 12) + 1) & 0x3F) != 0)));              | 
  P = Pool;                                                                                                            | 
  if ( !Pool )                                                                                                         | 
  {                                                                                                                    | 
    ObfDereferenceObjectWithTag(Object, 0x6D566D4Du);                                                                  | 
    return 3221225626LL;                                                                                               | 
  }                                                                                                                    | 
  if ( BugCheckParameter1 != -1LL )                                                                                    | 
  {                                                                                                                    | 
    KiStackAttachProcess((ULONG_PTR)Object);                                                                           | 
    v4 = 2;                                                                                                            | 
    v45 = 2;                                                                                                           | 
    v48 = 2;                                                                                                           | 
  }                                                                                                                    | 
  v9 = ((v54 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                                | 
  v52 = v9;                                                                                                            | 
  v55 = v7;                                                                                                            | 
  v59 = ((v6 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                                | 
  v10 = MiLockVadRange(Object, v7, v6, 0LL);                                                                           | 
  v57 = v10;                                                                                                           | 
  if ( !v10 )                                                                                                          | 
  {                                                                                                                    | 
    v34 = -1073741819;                                                                                                 | 
    goto LABEL_68;                                                                                                     | 
  }                                                                                                                    | 
  Address = MiLocateAddress(v7);                                                                                       | 
  NextVad = Address;                                                                                                   | 
  for ( i = 0LL; i < v13; i = v33 + 1 )                                                                                | 
  {                                                                                                                    | 
    if ( !(unsigned int)MiVadPagesTradable(Address) )                                                                  | 
    {                                                                                                                  | 
      v34 = -1073741747;                                                                                               | 
      goto LABEL_68;                                                                                                   | 
    }                                                                                                                  | 
    Address = MiGetNextVad(v32);                                                                                       | 
  }                                                                                                                    | 
  v50 = MiVadPureReserve(NextVad, v12, i, Address);                                                                    | 
  v15 = 0LL;                                                                                                           | 
  v16 = (char *)Object + 1024;                                                                                         | 
  v60 = (char *)Object + 1024;                                                                                         | 
  v61 = *((_QWORD *)Object + 130);                                                                                     | 
LABEL_9:                                                                                                               | 
  LOBYTE(v18) = MiLockWorkingSetShared(v16);                                                                           | 
  v42 = v18;                                                                                                           | 
LABEL_10:                                                                                                              | 
  v19 = 0xFFFFDE0000000000uLL;                                                                                         | 
  v20 = 0xFFFFF6FB40000000uLL;                                                                                         | 
  while ( 1 )                                                                                                          | 
  {                                                                                                                    | 
    if ( v7 > v6 )                                                                                                     | 
    {                                                                                                                  | 
      if ( v15 )                                                                                                       | 
      {                                                                                                                | 
        MiUnlockPageTableInternal(v16, v15, v18);                                                                      | 
        LOBYTE(v18) = v42;                                                                                             | 
      }                                                                                                                | 
      LOBYTE(v17) = v18;                                                                                               | 
      MiUnlockWorkingSetShared(v16, v17);                                                                              | 
      MiUnlockVadRange(Object, v55, v57, 0LL);                                                                         | 
      if ( (v4 & 2) != 0 )                                                                                             | 
      {                                                                                                                | 
        KiUnstackDetachProcess(v65, 0LL);                                                                              | 
      }                                                                                                                | 
      ObfDereferenceObjectWithTag(Object, 0x6D566D4Du);                                                                | 
      ExFreePoolWithTag(P, 0);                                                                                         | 
      v31 = v54 & 0xFFFFFFFFFFFFF000uLL;                                                                               | 
      *v63 = v6 - (v54 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                | 
      *v64 = v31;                                                                                                      | 
      return (v4 & 1) != 0 ? 0x40000019 : 0;                                                                           | 
    }                                                                                                                  | 
    if ( v7 >> 12 > (*(unsigned int *)(NextVad + 28) | ((unsigned __int64)*(unsigned __int8 *)(NextVad + 33) << 32)) ) | 
    {                                                                                                                  | 
      NextVad = MiGetNextVad(NextVad);                                                                                 | 
      v50 = MiVadPureReserve(NextVad, v35, v36, v37);                                                                  | 
    }                                                                                                                  | 
    BugCheckParameter2 = ((v7 >> 18) & 0x3FFFFFF8) + v20;                                                              | 
    if ( v15 != BugCheckParameter2 )                                                                                   | 
    {                                                                                                                  | 
      if ( v15 )                                                                                                       | 
      {                                                                                                                | 
        MiUnlockPageTableInternal(v16, v15, v18);                                                                      | 
        v15 = 0LL;                                                                                                     | 
        LOBYTE(v18) = v42;                                                                                             | 
      }                                                                                                                | 
      if ( v50 )                                                                                                       | 
      {                                                                                                                | 
        NextPageTable = MiGetNextPageTable(v9, v59, v18, 0, (__int64)v58);                                             | 
        if ( NextPageTable )                                                                                           | 
        {                                                                                                              | 
          v15 = ((NextPageTable >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                             | 
        }                                                                                                              | 
        if ( NextPageTable != v9 )                                                                                     | 
        {                                                                                                              | 
          v34 = -1073741819;                                                                                           | 
          goto LABEL_62;                                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
      else                                                                                                             | 
      {                                                                                                                | 
        MiMakeSystemAddressValid(v9);                                                                                  | 
        v15 = ((v9 >> 9) & 0x7FFFFFFFF8LL) - 0x98000000000LL;                                                          | 
      }                                                                                                                | 
      v19 = 0xFFFFDE0000000000uLL;                                                                                     | 
    }                                                                                                                  | 
    if ( (*(_QWORD *)v9 & 1) == 0 )                                                                                    | 
    {                                                                                                                  | 
      MiUnlockPageTableInternal(v16, v15, v18);                                                                        | 
      v15 = 0LL;                                                                                                       | 
      BugCheckParameter2 = 0LL;                                                                                        | 
      LOBYTE(v21) = v42;                                                                                               | 
      MiUnlockWorkingSetShared(v16, v21);                                                                              | 
      goto LABEL_9;                                                                                                    | 
    }                                                                                                                  | 
    v22 = v19 + 48 * ((*(_QWORD *)v9 >> 12) & 0xFFFFFFFFFFLL);                                                         | 
    if ( (MiGetWsleContents(0xFFFFFFFFFFLL, v7, v18) & 0xF) == 8 )                                                     | 
    {                                                                                                                  | 
      v4 |= 1u;                                                                                                        | 
      v45 = v4;                                                                                                        | 
      v48 = v4;                                                                                                        | 
      v25 = v44;                                                                                                       | 
      goto LABEL_27;                                                                                                   | 
    }                                                                                                                  | 
    if ( *((_QWORD *)v16 + 14) <= (unsigned __int64)(*(_QWORD *)(v61 + 32) + 6LL) )                                    | 
    {                                                                                                                  | 
      v34 = -1073741663;                                                                                               | 
      goto LABEL_62;                                                                                                   | 
    }                                                                                                                  | 
    if ( *(__int64 *)(v22 + 40) < 0 )                                                                                  | 
    {                                                                                                                  | 
      if ( !(unsigned int)MiLockPageTablePage(v22, 1LL) )                                                              | 
      {                                                                                                                | 
        goto LABEL_95;                                                                                                 | 
      }                                                                                                                | 
      v23 = 0xFFFFDE0000000000uLL;                                                                                     | 
    }                                                                                                                  | 
    BugCheckParameter2 = v23 + 48 * ((*(_QWORD *)BugCheckParameter2 >> 12) & 0xFFFFFFFFFFLL);                          | 
    if ( !(unsigned int)MiLockPageTablePage(BugCheckParameter2, 2LL) )                                                 | 
    {                                                                                                                  | 
      break;                                                                                                           | 
    }                                                                                                                  | 
    v51 = 0LL;                                                                                                         | 
    if ( (*(_DWORD *)(v22 + 16) & 0x400LL) != 0 )                                                                      | 
    {                                                                                                                  | 
      if ( *(__int64 *)(v22 + 40) < 0 )                                                                                | 
      {                                                                                                                | 
        v51 = *((_QWORD *)qword_140E2FFC8 + ((*(_QWORD *)(v22 + 40) >> 43) & 0x3FFLL));                                | 
        if ( !(unsigned int)MiChargeCommit(v51, 1LL, 0LL) )                                                            | 
        {                                                                                                              | 
          MiUnlockPageTableCharges(BugCheckParameter2);                                                                | 
          MiUnlockPageTableCharges(48 * (*(_QWORD *)(v22 + 40) & 0xFFFFFFFFFFLL) - 0x220000000000LL);                  | 
          v34 = -1073741523;                                                                                           | 
          goto LABEL_62;                                                                                               | 
        }                                                                                                              | 
        _InterlockedAdd64((volatile signed __int64 *)(v51 + 19632), 1uLL);                                             | 
      }                                                                                                                | 
    }                                                                                                                  | 
    else if ( (unsigned int)MiGetPagingFileOffset(*(_QWORD *)(v22 + 16)) )                                             | 
    {                                                                                                                  | 
      MiLockPageAndSetDirty(v22, 1LL);                                                                                 | 
    }                                                                                                                  | 
    LOBYTE(v24) = 8;                                                                                                   | 
    if ( !(unsigned int)MiSetVaAgeList(v16, v7, 1LL, v24) )                                                            | 
    {                                                                                                                  | 
      MiUnlockPageTableCharges(BugCheckParameter2);                                                                    | 
      if ( *(__int64 *)(v22 + 40) < 0 )                                                                                | 
      {                                                                                                                | 
        MiUnlockPageTableCharges(48 * (*(_QWORD *)(v22 + 40) & 0xFFFFFFFFFFLL) - 0x220000000000LL);                    | 
        if ( (*(_DWORD *)(v22 + 16) & 0x400LL) != 0 )                                                                  | 
        {                                                                                                              | 
          MiReturnCommit(v51, 1LL, 0LL);                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
      goto LABEL_95;                                                                                                   | 
    }                                                                                                                  | 
    LODWORD(v51) = MiGetEffectivePagePriorityThread(CurrentThread);                                                    | 
    v43 = 0;                                                                                                           | 
    while ( _interlockedbittestandset64((volatile signed __int32 *)(v22 + 24), 0x3FuLL) )                              | 
    {                                                                                                                  | 
      do                                                                                                               | 
      {                                                                                                                | 
        if ( (++v43 & HvlLongSpinCountMask) == 0                                                                       | 
          && (HvlEnlightenments & 0x40) != 0                                                                           | 
          && (unsigned __int8)KiCheckVpBackingLongSpinWaitHypercall() )                                                | 
        {                                                                                                              | 
          HvlNotifyLongSpinWait(v43);                                                                                  | 
        }                                                                                                              | 
        else                                                                                                           | 
        {                                                                                                              | 
          _mm_pause();                                                                                                 | 
        }                                                                                                              | 
      }                                                                                                                | 
      while ( *(__int64 *)(v22 + 24) < 0 );                                                                            | 
    }                                                                                                                  | 
    MiUpdatePfnPriority(v22, (unsigned int)v51, 0LL);                                                                  | 
    _InterlockedAnd64((volatile signed __int64 *)(v22 + 24), 0x7FFFFFFFFFFFFFFFuLL);                                   | 
    v25 = v44;                                                                                                         | 
    _bittestandset64((signed __int64 *)P, v44);                                                                        | 
LABEL_27:                                                                                                              | 
    v44 = v25 + 1;                                                                                                     | 
    v26 = v52 + 8;                                                                                                     | 
    v52 += 8LL;                                                                                                        | 
    v7 += 4096LL;                                                                                                      | 
    v27 = 0;                                                                                                           | 
    if ( v7 <= v6 && (v26 & 0x78) == 0 )                                                                               | 
    {                                                                                                                  | 
      if ( (unsigned int)MiWorkingSetIsContended(v16, 0LL) || (unsigned int)MiPageTableLockIsContended(v16, v15) )     | 
      {                                                                                                                | 
LABEL_29:                                                                                                              | 
        MiUnlockPageTableInternal(v16, v15, v18);                                                                      | 
        LOBYTE(v29) = v42;                                                                                             | 
        MiUnlockWorkingSetShared(v16, v29);                                                                            | 
        v15 = 0LL;                                                                                                     | 
        MiLockWorkingSetShared(v16);                                                                                   | 
        v9 = v52;                                                                                                      | 
        LOBYTE(v18) = v42;                                                                                             | 
        goto LABEL_10;                                                                                                 | 
      }                                                                                                                | 
      v27 = KeShouldYieldProcessor() != 0;                                                                             | 
    }                                                                                                                  | 
    v28 = !v27;                                                                                                        | 
    v9 = v52;                                                                                                          | 
    LOBYTE(v18) = v42;                                                                                                 | 
    v20 = 0xFFFFF6FB40000000uLL;                                                                                       | 
    v19 = 0xFFFFDE0000000000uLL;                                                                                       | 
    if ( !v28 )                                                                                                        | 
    {                                                                                                                  | 
      goto LABEL_29;                                                                                                   | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  if ( *(__int64 *)(v22 + 40) < 0 )                                                                                    | 
  {                                                                                                                    | 
    MiUnlockPageTableCharges(48 * (*(_QWORD *)(v22 + 40) & 0xFFFFFFFFFFLL) - 0x220000000000LL);                        | 
  }                                                                                                                    | 
LABEL_95:                                                                                                              | 
  v34 = -1073741670;                                                                                                   | 
LABEL_62:                                                                                                              | 
  if ( v15 )                                                                                                           | 
  {                                                                                                                    | 
    MiUnlockPageTableInternal(v16, v15, v18);                                                                          | 
    v15 = 0LL;                                                                                                         | 
  }                                                                                                                    | 
  if ( v44 )                                                                                                           | 
  {                                                                                                                    | 
    v38 = v44 - 1;                                                                                                     | 
    NextVad = v44 - 1;                                                                                                 | 
    v39 = (v44 << 12) + v55 - 4096;                                                                                    | 
    v40 = v44;                                                                                                         | 
    while ( 1 )                                                                                                        | 
    {                                                                                                                  | 
      if ( _bittest64((const signed __int64 *)P, v38) == 1 )                                                           | 
      {                                                                                                                | 
        if ( v15 != ((v39 >> 18) & 0x3FFFFFF8) - 0x904C0000000LL )                                                     | 
        {                                                                                                              | 
          if ( v15 )                                                                                                   | 
          {                                                                                                            | 
            MiUnlockPageTableInternal(v16, v15, v18);                                                                  | 
          }                                                                                                            | 
          v15 = ((v39 >> 18) & 0x3FFFFFF8) - 0x904C0000000LL;                                                          | 
          MiLockNestedPageTable(v16, v15);                                                                             | 
        }                                                                                                              | 
        MiUnlockVa(v16, v39);                                                                                          | 
        v38 = NextVad;                                                                                                 | 
      }                                                                                                                | 
      v41 = 0;                                                                                                         | 
      if ( (v38 & 0xF) == 0 )                                                                                          | 
      {                                                                                                                | 
        if ( (unsigned int)MiWorkingSetIsContended(v16, 0LL)                                                           | 
          || v15 && (unsigned int)MiPageTableLockIsContended(v16, v15) )                                               | 
        {                                                                                                              | 
LABEL_102:                                                                                                             | 
          if ( v15 )                                                                                                   | 
          {                                                                                                            | 
            MiUnlockPageTableInternal(v16, v15, v18);                                                                  | 
            v15 = 0LL;                                                                                                 | 
          }                                                                                                            | 
          LOBYTE(v17) = v42;                                                                                           | 
          MiUnlockWorkingSetShared(v16, v17);                                                                          | 
          MiLockWorkingSetShared(v16);                                                                                 | 
          v38 = NextVad;                                                                                               | 
          goto LABEL_78;                                                                                               | 
        }                                                                                                              | 
        if ( KeShouldYieldProcessor() )                                                                                | 
        {                                                                                                              | 
          v41 = 1;                                                                                                     | 
        }                                                                                                              | 
        v38 = NextVad;                                                                                                 | 
      }                                                                                                                | 
      if ( v41 )                                                                                                       | 
      {                                                                                                                | 
        goto LABEL_102;                                                                                                | 
      }                                                                                                                | 
LABEL_78:                                                                                                              | 
      v39 -= 4096LL;                                                                                                   | 
      NextVad = --v38;                                                                                                 | 
      if ( !--v40 )                                                                                                    | 
      {                                                                                                                | 
        LOBYTE(v4) = v45;                                                                                              | 
        break;                                                                                                         | 
      }                                                                                                                | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  if ( v15 )                                                                                                           | 
  {                                                                                                                    | 
    MiUnlockPageTableInternal(v16, v15, v18);                                                                          | 
  }                                                                                                                    | 
  LOBYTE(v17) = v42;                                                                                                   | 
  MiUnlockWorkingSetShared(v16, v17);                                                                                  | 
  Pool = P;                                                                                                            | 
  v10 = v57;                                                                                                           | 
LABEL_68:                                                                                                              | 
  MiUnlockVadRange(Object, v55, v10, 0LL);                                                                             | 
  if ( (v4 & 2) != 0 )                                                                                                 | 
  {                                                                                                                    | 
    KiUnstackDetachProcess(v65, 0LL);                                                                                  | 
  }                                                                                                                    | 
  ObfDereferenceObjectWithTag(Object, 0x6D566D4Du);                                                                    | 
  ExFreePoolWithTag(Pool, 0);                                                                                          | 
  return v34;                                                                                                          | 
}                                                                                                                      | 
345 row(s)
