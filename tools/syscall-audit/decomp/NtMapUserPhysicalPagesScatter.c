__int64 __fastcall NtMapUserPhysicalPagesScatter(__int64 a1, unsigned __int64 a2, __int64 a3)                                 | 
{                                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                               | 
                                                                                                                              | 
  memset_0(P, 0, sizeof(P));                                                                                                  | 
  v32 = 0LL;                                                                                                                  | 
  v31 = 0LL;                                                                                                                  | 
  if ( a2 > 0xFFFFFFFFFFFFFLL )                                                                                               | 
  {                                                                                                                           | 
    return 3221225712LL;                                                                                                      | 
  }                                                                                                                           | 
  v27 = 0LL;                                                                                                                  | 
  Pool = (__int64 *)P;                                                                                                        | 
  CurrentThread = KeGetCurrentThread();                                                                                       | 
  v8 = a2;                                                                                                                    | 
  v9 = 0LL;                                                                                                                   | 
  v10 = 0LL;                                                                                                                  | 
  if ( a2 > 0x200 )                                                                                                           | 
  {                                                                                                                           | 
    v11 = 16 * a2;                                                                                                            | 
    if ( !a3 )                                                                                                                | 
    {                                                                                                                         | 
      v11 = 8 * a2;                                                                                                           | 
    }                                                                                                                         | 
    Pool = (__int64 *)MiAllocatePool(0x40uLL, v11);                                                                           | 
    if ( !Pool )                                                                                                              | 
    {                                                                                                                         | 
      return 3221225626LL;                                                                                                    | 
    }                                                                                                                         | 
  }                                                                                                                           | 
  v12 = MiCaptureUlongPtrArray(Pool);                                                                                         | 
  if ( v12 < 0 )                                                                                                              | 
  {                                                                                                                           | 
    goto LABEL_41;                                                                                                            | 
  }                                                                                                                           | 
  if ( !a2 )                                                                                                                  | 
  {                                                                                                                           | 
    return 0LL;                                                                                                               | 
  }                                                                                                                           | 
  if ( a3 )                                                                                                                   | 
  {                                                                                                                           | 
    v10 = &v34;                                                                                                               | 
    if ( a2 > 0x200 )                                                                                                         | 
    {                                                                                                                         | 
      v10 = (char *)&Pool[v8];                                                                                                | 
    }                                                                                                                         | 
    v12 = MiCaptureUlongPtrArray(v10);                                                                                        | 
    if ( v12 < 0 )                                                                                                            | 
    {                                                                                                                         | 
      goto LABEL_41;                                                                                                          | 
    }                                                                                                                         | 
  }                                                                                                                           | 
  v13 = &Pool[v8];                                                                                                            | 
  v14 = *Pool;                                                                                                                | 
  v12 = 0;                                                                                                                    | 
  v29 = (unsigned __int64)v13;                                                                                                | 
  v27 = MiLockAweVadsShared(CurrentThread);                                                                                   | 
  v15 = v27;                                                                                                                  | 
  AweNode = MiGetAweNode(v14);                                                                                                | 
  if ( AweNode && (*(_DWORD *)(AweNode + 40) & 3) == 0 )                                                                      | 
  {                                                                                                                           | 
    v17 = *(_QWORD *)(AweNode + 32);                                                                                          | 
    v9 = AweNode;                                                                                                             | 
    v28 = *(_DWORD *)(*(_QWORD *)(AweNode + 24) + 48LL) & 0x4200000;                                                          | 
    v18 = MiPageChainCount(v17);                                                                                              | 
    v30 = v18;                                                                                                                | 
    v20 = -1073741585;                                                                                                        | 
    v21 = (unsigned __int64 *)Pool;                                                                                           | 
    while ( 1 )                                                                                                               | 
    {                                                                                                                         | 
      v22 = *v21;                                                                                                             | 
      if ( v18 == 1 )                                                                                                         | 
      {                                                                                                                       | 
        v23 = *v21;                                                                                                           | 
      }                                                                                                                       | 
      else                                                                                                                    | 
      {                                                                                                                       | 
        if ( (((v18 << 12) - 1) & v22) != 0 )                                                                                 | 
        {                                                                                                                     | 
          goto LABEL_40;                                                                                                      | 
        }                                                                                                                     | 
        v23 = (v18 << 12) + v22 - 1;                                                                                          | 
      }                                                                                                                       | 
      if ( v22 < (*(unsigned int *)(v19 + 24) | ((unsigned __int64)*(unsigned __int8 *)(v19 + 32) << 32)) << 12               | 
        || v23 > (((*(unsigned int *)(v19 + 28) | ((unsigned __int64)*(unsigned __int8 *)(v19 + 33) << 32)) << 12) | 0xFFF) ) | 
      {                                                                                                                       | 
        v24 = MiGetAweNode(*v21);                                                                                             | 
        if ( !v24 || (*(_DWORD *)(v24 + 40) & 3) != 0 || *(_QWORD *)(v24 + 32) != v17 )                                       | 
        {                                                                                                                     | 
LABEL_40:                                                                                                                     | 
          v12 = v20;                                                                                                          | 
          goto LABEL_41;                                                                                                      | 
        }                                                                                                                     | 
        v19 = *(_QWORD *)(v24 + 24);                                                                                          | 
        v25 = *(_DWORD *)(v19 + 48) & 0x4200000;                                                                              | 
        if ( v28 == 69206016 )                                                                                                | 
        {                                                                                                                     | 
          if ( v25 != 69206016 )                                                                                              | 
          {                                                                                                                   | 
            v12 = v20;                                                                                                        | 
          }                                                                                                                   | 
        }                                                                                                                     | 
        else if ( v25 == 69206016 )                                                                                           | 
        {                                                                                                                     | 
          v12 = v20;                                                                                                          | 
        }                                                                                                                     | 
      }                                                                                                                       | 
      if ( (unsigned __int64)++v21 >= v29 )                                                                                   | 
      {                                                                                                                       | 
        break;                                                                                                                | 
      }                                                                                                                       | 
      v18 = v30;                                                                                                              | 
    }                                                                                                                         | 
    v26 = MiLockAwePagesShared(v17, CurrentThread);                                                                           | 
    if ( !v10                                                                                                                 | 
      || (v12 = MiReferenceIncomingPhysicalPages(v17, (_DWORD)v10, a2, (_DWORD)Pool, (__int64)&v31, v9, 0LL), v12 >= 0) )     | 
    {                                                                                                                         | 
      MiWriteAwePtes(v9, (_DWORD)v10, a2, (_DWORD)Pool, 0LL, 1, (__int64)&v31);                                               | 
    }                                                                                                                         | 
    if ( v26 )                                                                                                                | 
    {                                                                                                                         | 
      MiUnlockAweVadsShared(CurrentThread, v26);                                                                              | 
    }                                                                                                                         | 
LABEL_41:                                                                                                                     | 
    v15 = v27;                                                                                                                | 
    goto LABEL_42;                                                                                                            | 
  }                                                                                                                           | 
  v12 = -1073741585;                                                                                                          | 
LABEL_42:                                                                                                                     | 
  if ( (_QWORD)v31 )                                                                                                          | 
  {                                                                                                                           | 
    MiFreePhysicalPageChain(*(_QWORD *)(v9 + 32), &v31);                                                                      | 
  }                                                                                                                           | 
  if ( v15 )                                                                                                                  | 
  {                                                                                                                           | 
    MiUnlockAweVadsShared(CurrentThread, v15);                                                                                | 
  }                                                                                                                           | 
  if ( Pool != (__int64 *)P )                                                                                                 | 
  {                                                                                                                           | 
    ExFreePoolWithTag(Pool, 0);                                                                                               | 
  }                                                                                                                           | 
  return (unsigned int)v12;                                                                                                   | 
}                                                                                                                             | 
143 row(s)
