__int64 __fastcall NtAlertThreadByThreadIdEx(__int64 a1, __int64 a2, signed __int64 a3, __int64 a4)         | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  CurrentThread = KeGetCurrentThread();                                                                     | 
  v5 = 0LL;                                                                                                 | 
  v6 = a2;                                                                                                  | 
  --CurrentThread->SpecialApcDisable;                                                                       | 
  if ( (a1 & 0x3FC) != 0 && (v7 = (_QWORD *)ExpLookupHandleTableEntry(PspCidTable, a1), (v8 = v7) != 0LL) ) | 
  {                                                                                                         | 
    v9 = PspCidTable;                                                                                       | 
    _m_prefetchw(v7);                                                                                       | 
    v10 = *v7;                                                                                              | 
LABEL_4:                                                                                                    | 
    v11 = v8[1];                                                                                            | 
    *(_QWORD *)&v35[0] = v10;                                                                               | 
    v12 = v10;                                                                                              | 
    *((_QWORD *)&v35[0] + 1) = v11;                                                                         | 
    do                                                                                                      | 
    {                                                                                                       | 
      if ( (v12 & 0x1FFFE) == 0 )                                                                           | 
      {                                                                                                     | 
        v27 = PspCidTable;                                                                                  | 
        v28 = 0;                                                                                            | 
        do                                                                                                  | 
        {                                                                                                   | 
          while ( 1 )                                                                                       | 
          {                                                                                                 | 
            _m_prefetchw(v8);                                                                               | 
            a3 = *v8;                                                                                       | 
            if ( (*v8 & 1) != 0 )                                                                           | 
            {                                                                                               | 
              break;                                                                                        | 
            }                                                                                               | 
            if ( !a3 )                                                                                      | 
            {                                                                                               | 
              goto LABEL_18;                                                                                | 
            }                                                                                               | 
            ExpBlockOnLockedHandleEntry(v27, v8);                                                           | 
          }                                                                                                 | 
        }                                                                                                   | 
        while ( a3 != _InterlockedCompareExchange64(v8, a3 - 1, a3) );                                      | 
        HandlePointer = ExGetHandlePointer(v8, a2, a3);                                                     | 
        if ( (*(_BYTE *)HandlePointer & 0x7F) == 6 )                                                        | 
        {                                                                                                   | 
          if ( (*(_DWORD *)(HandlePointer + 1440) & 3) == 2 )                                               | 
          {                                                                                                 | 
            v28 = ExSlowReplenishHandleTableEntry(v8);                                                      | 
          }                                                                                                 | 
          a2 = (unsigned int)(v28 + 1);                                                                     | 
          _m_prefetchw((const void *)(HandlePointer - 48));                                                 | 
          v29 = *(_QWORD *)(HandlePointer - 48);                                                            | 
          while ( v29 )                                                                                     | 
          {                                                                                                 | 
            v30 = v29;                                                                                      | 
            v29 = _InterlockedCompareExchange64(                                                            | 
                    (volatile signed __int64 *)(HandlePointer - 48),                                        | 
                    v29 + (unsigned int)a2,                                                                 | 
                    v29);                                                                                   | 
            if ( v30 == v29 )                                                                               | 
            {                                                                                               | 
              ObpTraceObjectReferenceIfActive(HandlePointer - 48, a2, 1953261124LL);                        | 
              goto LABEL_39;                                                                                | 
            }                                                                                               | 
          }                                                                                                 | 
          v33 = *v8 & 0xFFFFFFFFFFFE0001uLL;                                                                | 
          v35[1] = v33;                                                                                     | 
          *v8 = v33;                                                                                        | 
        }                                                                                                   | 
        HandlePointer = 0LL;                                                                                | 
LABEL_39:                                                                                                   | 
        v31 = PspCidTable;                                                                                  | 
        _InterlockedIncrement64(v8);                                                                        | 
        v32 = (_QWORD *)(v31 + 48);                                                                         | 
        _InterlockedOr(v34, 0);                                                                             | 
        if ( *v32 )                                                                                         | 
        {                                                                                                   | 
          ExfUnblockPushLock(v32, 0LL);                                                                     | 
        }                                                                                                   | 
        goto LABEL_12;                                                                                      | 
      }                                                                                                     | 
      if ( (v12 & 1) == 0 )                                                                                 | 
      {                                                                                                     | 
        ExpBlockOnLockedHandleEntry(v9, v8);                                                                | 
        _m_prefetchw(v8);                                                                                   | 
        v10 = *v8;                                                                                          | 
        goto LABEL_4;                                                                                       | 
      }                                                                                                     | 
      *(_QWORD *)&v13 = v12;                                                                                | 
      *((_QWORD *)&v13 + 1) = v11;                                                                          | 
      v14 = _InterlockedCompareExchange128(v8, v11, v12 - 2, (signed __int64 *)&v13);                       | 
      a2 = *((_QWORD *)&v13 + 1);                                                                           | 
      v12 = v13;                                                                                            | 
      v35[0] = v13;                                                                                         | 
      v11 = *((_QWORD *)&v13 + 1);                                                                          | 
    }                                                                                                       | 
    while ( !v14 );                                                                                         | 
    v15 = v12 >> 1;                                                                                         | 
    if ( (unsigned __int16)(v12 >> 1) == 16 )                                                               | 
    {                                                                                                       | 
      v12 &= 0xFFFFFFFFFFFE0001uLL;                                                                         | 
      *(_QWORD *)&v35[0] = v12 | (2LL * (unsigned __int16)(v15 - 1));                                       | 
    }                                                                                                       | 
    v16 = (_BYTE *)ExGetHandlePointer(v35, a2, v12);                                                        | 
    HandlePointer = (__int64)v16;                                                                           | 
    if ( (*v16 & 0x7F) != 6 )                                                                               | 
    {                                                                                                       | 
      ObfDereferenceObject(v16);                                                                            | 
      goto LABEL_18;                                                                                        | 
    }                                                                                                       | 
LABEL_12:                                                                                                   | 
    if ( !HandlePointer )                                                                                   | 
    {                                                                                                       | 
      goto LABEL_18;                                                                                        | 
    }                                                                                                       | 
    if ( IoThreadToProcess((PETHREAD)HandlePointer) == PsIdleProcess )                                      | 
    {                                                                                                       | 
      goto LABEL_41;                                                                                        | 
    }                                                                                                       | 
    if ( (*(_DWORD *)(HandlePointer + 1440) & 2) == 0 )                                                     | 
    {                                                                                                       | 
      v19 = HandlePointer + 1424;                                                                           | 
      _InterlockedOr(v34, 0);                                                                               | 
      if ( (*(_QWORD *)(HandlePointer + 1424) & 1) != 0 )                                                   | 
      {                                                                                                     | 
        ExfAcquireReleasePushLockExclusive(v19);                                                            | 
      }                                                                                                     | 
      if ( (*(_DWORD *)(HandlePointer + 1440) & 2) == 0 )                                                   | 
      {                                                                                                     | 
        goto LABEL_41;                                                                                      | 
      }                                                                                                     | 
    }                                                                                                       | 
    CurrentServerSilo = PsGetCurrentServerSilo(v19, v18, v20, v21);                                         | 
    if ( (unsigned __int8)PsIsProcessInSilo(*(_QWORD *)(HandlePointer + 544), CurrentServerSilo) )          | 
    {                                                                                                       | 
      v5 = (struct _KTHREAD *)HandlePointer;                                                                | 
      v23 = 0;                                                                                              | 
    }                                                                                                       | 
    else                                                                                                    | 
    {                                                                                                       | 
LABEL_41:                                                                                                   | 
      v23 = -1073741813;                                                                                    | 
      ObfDereferenceObject((PVOID)HandlePointer);                                                           | 
    }                                                                                                       | 
  }                                                                                                         | 
  else                                                                                                      | 
  {                                                                                                         | 
LABEL_18:                                                                                                   | 
    v23 = -1073741813;                                                                                      | 
  }                                                                                                         | 
  if ( CurrentThread->SpecialApcDisable++ == -1 )                                                           | 
  {                                                                                                         | 
    v25 = &CurrentThread->152;                                                                              | 
    if ( ($727077A9B6E167EAE1398C74674DC5A5 *)v25->ApcState.ApcListHead[0].Flink != v25 )                   | 
    {                                                                                                       | 
      KiCheckForKernelApcDelivery(v25, a2, a3, a4);                                                         | 
    }                                                                                                       | 
  }                                                                                                         | 
  if ( v23 < 0 )                                                                                            | 
  {                                                                                                         | 
    return (unsigned int)v23;                                                                               | 
  }                                                                                                         | 
  if ( v5->Process == CurrentThread->Process )                                                              | 
  {                                                                                                         | 
    if ( CurrentThread == v5 || !(unsigned int)KeAbIsValidUserModeLockAddress(v6) )                         | 
    {                                                                                                       | 
      v6 = 0LL;                                                                                             | 
    }                                                                                                       | 
    KeAlertThreadByThreadIdEx(v5, v6);                                                                      | 
    ObfDereferenceObject(v5);                                                                               | 
    return 0LL;                                                                                             | 
  }                                                                                                         | 
  else                                                                                                      | 
  {                                                                                                         | 
    ObfDereferenceObject(v5);                                                                               | 
    return 3221225506LL;                                                                                    | 
  }                                                                                                         | 
}                                                                                                           | 
