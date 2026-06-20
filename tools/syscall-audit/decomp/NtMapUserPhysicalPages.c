__int64 __fastcall NtMapUserPhysicalPages(__int64 a1, unsigned __int64 a2, __int64 a3)                                           | 
{                                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                  | 
                                                                                                                                 | 
  v27 = 0LL;                                                                                                                     | 
  v3 = a3;                                                                                                                       | 
  v26 = 0LL;                                                                                                                     | 
  if ( a2 - 1 > 0xFFFFFFFFFFFFELL )                                                                                              | 
  {                                                                                                                              | 
    return 3221225712LL;                                                                                                         | 
  }                                                                                                                              | 
  CurrentThread = KeGetCurrentThread();                                                                                          | 
  v6 = a1 & 0xFFFFFFFFFFFFF000uLL;                                                                                               | 
  v7 = 0LL;                                                                                                                      | 
  v8 = 0LL;                                                                                                                      | 
  Pool = 0LL;                                                                                                                    | 
  if ( !a3 )                                                                                                                     | 
  {                                                                                                                              | 
    goto LABEL_6;                                                                                                                | 
  }                                                                                                                              | 
  if ( a2 > 0x200 )                                                                                                              | 
  {                                                                                                                              | 
    Pool = (_BYTE *)MiAllocatePool(0x40uLL, 8 * a2);                                                                             | 
    if ( !Pool )                                                                                                                 | 
    {                                                                                                                            | 
      return 3221225626LL;                                                                                                       | 
    }                                                                                                                            | 
  }                                                                                                                              | 
  else                                                                                                                           | 
  {                                                                                                                              | 
    Pool = P;                                                                                                                    | 
  }                                                                                                                              | 
  v10 = MiCaptureUlongPtrArray(Pool);                                                                                            | 
  if ( v10 >= 0 )                                                                                                                | 
  {                                                                                                                              | 
LABEL_6:                                                                                                                         | 
    v7 = MiLockAweVadsShared(CurrentThread);                                                                                     | 
    AweNode = MiGetAweNode(v6);                                                                                                  | 
    v8 = AweNode;                                                                                                                | 
    if ( AweNode )                                                                                                               | 
    {                                                                                                                            | 
      v13 = *(_QWORD *)(AweNode + 32);                                                                                           | 
      AweViewPageSize = MiGetAweViewPageSize(AweNode);                                                                           | 
      if ( !AweViewPageSize )                                                                                                    | 
      {                                                                                                                          | 
        AweViewPageSize = MiPageChainCount(v13);                                                                                 | 
      }                                                                                                                          | 
      MiPageSizeToPteLevel(AweViewPageSize);                                                                                     | 
      if ( v16 == 1 || (((v15 << 12) - 1) & v6) == 0 )                                                                           | 
      {                                                                                                                          | 
        v18 = v6 + ((v16 * a2) << 12) - 1;                                                                                       | 
        if ( v18 <= v6 )                                                                                                         | 
        {                                                                                                                        | 
          v10 = -1073741584;                                                                                                     | 
          goto LABEL_27;                                                                                                         | 
        }                                                                                                                        | 
        if ( v6 >= (*(unsigned int *)(v17 + 24) | ((unsigned __int64)*(unsigned __int8 *)(v17 + 32) << 32)) << 12                | 
          && v18 <= (((*(unsigned int *)(v17 + 28) | ((unsigned __int64)*(unsigned __int8 *)(v17 + 33) << 32)) << 12) | 0xFFF) ) | 
        {                                                                                                                        | 
          PteAddress = MiGetPteAddress(v6);                                                                                      | 
          for ( i = v20 == 0; ; i = v22 == 1 )                                                                                   | 
          {                                                                                                                      | 
            v23 = PteAddress;                                                                                                    | 
            if ( i )                                                                                                             | 
            {                                                                                                                    | 
              break;                                                                                                             | 
            }                                                                                                                    | 
            PteAddress = MiGetPteAddress(PteAddress);                                                                            | 
          }                                                                                                                      | 
          v24 = MiLockAwePagesShared(v13, CurrentThread);                                                                        | 
          if ( !Pool                                                                                                             | 
            || (v10 = MiReferenceIncomingPhysicalPages(v13, (_DWORD)Pool, a2, 0, (__int64)&v26, v8, v23), v10 >= 0) )            | 
          {                                                                                                                      | 
            MiWriteAwePtes(v8, (_DWORD)Pool, a2, 0, v23, 1, (__int64)&v26);                                                      | 
            v10 = 0;                                                                                                             | 
          }                                                                                                                      | 
          if ( v24 )                                                                                                             | 
          {                                                                                                                      | 
            MiUnlockAweVadsShared(CurrentThread, v24);                                                                           | 
          }                                                                                                                      | 
          v3 = a3;                                                                                                               | 
          goto LABEL_27;                                                                                                         | 
        }                                                                                                                        | 
      }                                                                                                                          | 
    }                                                                                                                            | 
    v10 = -1073741585;                                                                                                           | 
  }                                                                                                                              | 
LABEL_27:                                                                                                                        | 
  if ( (_QWORD)v26 )                                                                                                             | 
  {                                                                                                                              | 
    MiFreePhysicalPageChain(*(_QWORD *)(v8 + 32), &v26);                                                                         | 
  }                                                                                                                              | 
  if ( v7 )                                                                                                                      | 
  {                                                                                                                              | 
    MiUnlockAweVadsShared(CurrentThread, v7);                                                                                    | 
  }                                                                                                                              | 
  if ( v3 )                                                                                                                      | 
  {                                                                                                                              | 
    if ( Pool != P )                                                                                                             | 
    {                                                                                                                            | 
      ExFreePoolWithTag(Pool, 0);                                                                                                | 
    }                                                                                                                            | 
  }                                                                                                                              | 
  return (unsigned int)v10;                                                                                                      | 
}                                                                                                                                | 
105 row(s)
