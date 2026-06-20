__int64 __fastcall NtCompactKeys(unsigned int a1, const void *a2)                                                 | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  v23 = 0LL;                                                                                                      | 
  v24 = 0LL;                                                                                                      | 
  TransientPoolWithQuota = 0LL;                                                                                   | 
  v5 = 0;                                                                                                         | 
  v6 = 0;                                                                                                         | 
  CmpInitializeThreadInfo(&v23);                                                                                  | 
  v7 = CmCheckNoTxContext();                                                                                      | 
  if ( v7 < 0 )                                                                                                   | 
  {                                                                                                               | 
    goto LABEL_4;                                                                                                 | 
  }                                                                                                               | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                              | 
  if ( !SeSinglePrivilegeCheck(SeBackupPrivilege, PreviousMode) )                                                 | 
  {                                                                                                               | 
    v7 = -1073741727;                                                                                             | 
LABEL_4:                                                                                                          | 
    v9 = 0;                                                                                                       | 
    goto LABEL_38;                                                                                                | 
  }                                                                                                               | 
  if ( !a1 )                                                                                                      | 
  {                                                                                                               | 
    v7 = 0;                                                                                                       | 
    goto LABEL_4;                                                                                                 | 
  }                                                                                                               | 
  if ( a1 >= 0x1FFFFFFF )                                                                                         | 
  {                                                                                                               | 
    v7 = -1073741811;                                                                                             | 
    goto LABEL_4;                                                                                                 | 
  }                                                                                                               | 
  v10 = 8 * a1;                                                                                                   | 
  TransientPoolWithQuota = (struct _PRIVILEGE_SET *)CmpAllocateTransientPoolWithQuota(v8, 8 * a1, 1633832259LL);  | 
  if ( TransientPoolWithQuota )                                                                                   | 
  {                                                                                                               | 
    if ( PreviousMode == 1 && v10 && ((unsigned __int8)a2 & 3) != 0 )                                             | 
    {                                                                                                             | 
      ExRaiseDatatypeMisalignment();                                                                              | 
    }                                                                                                             | 
    memmove(TransientPoolWithQuota, a2, v10);                                                                     | 
    while ( v5 < a1 )                                                                                             | 
    {                                                                                                             | 
      LOBYTE(v12) = PreviousMode;                                                                                 | 
      v7 = CmObReferenceObjectByHandle(                                                                           | 
             *((_QWORD *)&TransientPoolWithQuota->PrivilegeCount + v5),                                           | 
             131078,                                                                                              | 
             v11,                                                                                                 | 
             v12,                                                                                                 | 
             (__int64)TransientPoolWithQuota + 8 * v5,                                                            | 
             0LL);                                                                                                | 
      if ( v7 < 0 )                                                                                               | 
      {                                                                                                           | 
        v9 = 0;                                                                                                   | 
        v6 = 0;                                                                                                   | 
        goto LABEL_38;                                                                                            | 
      }                                                                                                           | 
      ++v5;                                                                                                       | 
    }                                                                                                             | 
    v6 = CmpAcquireShutdownRundown();                                                                             | 
    v25 = v6;                                                                                                     | 
    if ( v6 )                                                                                                     | 
    {                                                                                                             | 
      CmpLockRegistryExclusive();                                                                                 | 
      v22 = 0LL;                                                                                                  | 
      for ( i = 0; i < a1; ++i )                                                                                  | 
      {                                                                                                           | 
        v14 = *((_QWORD *)&TransientPoolWithQuota->PrivilegeCount + i);                                           | 
        v7 = CmpPerformKeyBodyDeletionCheck(v14, 0LL);                                                            | 
        if ( v7 < 0 )                                                                                             | 
        {                                                                                                         | 
          goto LABEL_34;                                                                                          | 
        }                                                                                                         | 
        if ( i )                                                                                                  | 
        {                                                                                                         | 
          v15 = v22;                                                                                              | 
        }                                                                                                         | 
        else                                                                                                      | 
        {                                                                                                         | 
          v15 = *(_QWORD *)(*(_QWORD *)(v14 + 8) + 32LL);                                                         | 
          v22 = v15;                                                                                              | 
        }                                                                                                         | 
        v16 = *(_QWORD *)(v14 + 8);                                                                               | 
        if ( v15 != *(_QWORD *)(v16 + 32) || (v17 = *(_WORD *)(v16 + 186), (v17 & 4) != 0) || (v17 & 0x10) != 0 ) | 
        {                                                                                                         | 
          v7 = -1073741811;                                                                                       | 
          goto LABEL_34;                                                                                          | 
        }                                                                                                         | 
        if ( *(_WORD *)(v16 + 66) )                                                                               | 
        {                                                                                                         | 
          v7 = -1073741822;                                                                                       | 
LABEL_34:                                                                                                         | 
          v9 = 1;                                                                                                 | 
          goto LABEL_35;                                                                                          | 
        }                                                                                                         | 
      }                                                                                                           | 
      v7 = 0;                                                                                                     | 
      CmpUnlockRegistry();                                                                                        | 
      CmpLogUnsupportedOperation(7LL);                                                                            | 
      v9 = 0;                                                                                                     | 
LABEL_35:                                                                                                         | 
      v6 = v25;                                                                                                   | 
    }                                                                                                             | 
    else                                                                                                          | 
    {                                                                                                             | 
      v7 = -1073741431;                                                                                           | 
      v9 = 0;                                                                                                     | 
    }                                                                                                             | 
  }                                                                                                               | 
  else                                                                                                            | 
  {                                                                                                               | 
    v7 = -1073741670;                                                                                             | 
    v9 = 0;                                                                                                       | 
    v6 = 0;                                                                                                       | 
  }                                                                                                               | 
LABEL_38:                                                                                                         | 
  if ( v9 )                                                                                                       | 
  {                                                                                                               | 
    CmpUnlockRegistry();                                                                                          | 
  }                                                                                                               | 
  if ( v6 )                                                                                                       | 
  {                                                                                                               | 
    CmpReleaseShutdownRundown();                                                                                  | 
  }                                                                                                               | 
  if ( TransientPoolWithQuota )                                                                                   | 
  {                                                                                                               | 
    if ( v5 )                                                                                                     | 
    {                                                                                                             | 
      v18 = (PVOID *)TransientPoolWithQuota;                                                                      | 
      v19 = v5;                                                                                                   | 
      do                                                                                                          | 
      {                                                                                                           | 
        ObfDereferenceObject(*v18++);                                                                             | 
        --v19;                                                                                                    | 
      }                                                                                                           | 
      while ( v19 );                                                                                              | 
    }                                                                                                             | 
    CmSiFreeMemory(TransientPoolWithQuota);                                                                       | 
  }                                                                                                               | 
  CmpCleanupThreadInfo(&v23);                                                                                     | 
  return (unsigned int)v7;                                                                                        | 
}                                                                                                                 | 
143 row(s)
