__int64 __fastcall NtCompressKey(int a1)                                                                          | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  Object = 0LL;                                                                                                   | 
  v17 = 0LL;                                                                                                      | 
  v16 = 0LL;                                                                                                      | 
  memset(&ApcState, 0, sizeof(ApcState));                                                                         | 
  CmpInitializeThreadInfo(&v16);                                                                                  | 
  v2 = CmCheckNoTxContext();                                                                                      | 
  if ( v2 >= 0 )                                                                                                  | 
  {                                                                                                               | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                                            | 
    if ( SeSinglePrivilegeCheck(SeBackupPrivilege, PreviousMode) )                                                | 
    {                                                                                                             | 
      LOBYTE(v5) = PreviousMode;                                                                                  | 
      v6 = a1 & 3;                                                                                                | 
      v7 = CmObReferenceObjectByHandle(a1, 131078, v4, v5, (__int64)&Object, 0LL);                                | 
      v8 = Object;                                                                                                | 
      v2 = v7;                                                                                                    | 
      if ( v7 >= 0 )                                                                                              | 
      {                                                                                                           | 
        if ( (unsigned __int8)CmpAcquireShutdownRundown() )                                                       | 
        {                                                                                                         | 
          while ( 1 )                                                                                             | 
          {                                                                                                       | 
            CmpLockRegistryExclusive();                                                                           | 
            v2 = CmpPerformKeyBodyDeletionCheck(v8, 0LL);                                                         | 
            if ( v2 < 0 )                                                                                         | 
            {                                                                                                     | 
              break;                                                                                              | 
            }                                                                                                     | 
            v9 = v8[1];                                                                                           | 
            v10 = *(_QWORD *)(v9 + 32);                                                                           | 
            if ( *(_DWORD *)(v9 + 40) != *(_DWORD *)(*(_QWORD *)(v10 + 64) + 36LL) )                              | 
            {                                                                                                     | 
              v2 = -1073741811;                                                                                   | 
              break;                                                                                              | 
            }                                                                                                     | 
            v11 = v10 + 4208;                                                                                     | 
            if ( !(unsigned __int8)CmpIsWriteQueueActive(v10 + 4208) )                                            | 
            {                                                                                                     | 
              v11 = v10 + 4192;                                                                                   | 
              if ( !(unsigned __int8)CmpIsWriteQueueActive(v10 + 4192) )                                          | 
              {                                                                                                   | 
                CmpAttachToRegistryProcess(&ApcState);                                                            | 
                v12 = *(_QWORD *)(v8[1] + 32LL);                                                                  | 
                if ( v12 == CmpMasterHive || !*(_QWORD *)(v12 + 1544) || (*(_DWORD *)(v12 + 160) & 0x8003) != 0 ) | 
                {                                                                                                 | 
                  v2 = -1073741811;                                                                               | 
                }                                                                                                 | 
                else                                                                                              | 
                {                                                                                                 | 
                  v13 = 0;                                                                                        | 
                  if ( v6 != 3 )                                                                                  | 
                  {                                                                                               | 
                    v13 = v6;                                                                                     | 
                  }                                                                                               | 
                  *(_QWORD *)(*(_QWORD *)(v12 + 64) + 168LL) = v13;                                               | 
                  HvMarkBaseBlockDirty();                                                                         | 
                  v2 = 0;                                                                                         | 
                }                                                                                                 | 
                CmpDetachFromRegistryProcess(&ApcState);                                                          | 
                break;                                                                                            | 
              }                                                                                                   | 
            }                                                                                                     | 
            HvLockHiveFlusherExclusive(v10);                                                                      | 
            CmpWaitOnHiveWriteQueue(v10, v11);                                                                    | 
          }                                                                                                       | 
          CmpUnlockRegistry();                                                                                    | 
          CmpReleaseShutdownRundown();                                                                            | 
        }                                                                                                         | 
        else                                                                                                      | 
        {                                                                                                         | 
          v2 = -1073741431;                                                                                       | 
        }                                                                                                         | 
      }                                                                                                           | 
      if ( v8 )                                                                                                   | 
      {                                                                                                           | 
        ObfDereferenceObject(v8);                                                                                 | 
      }                                                                                                           | 
    }                                                                                                             | 
    else                                                                                                          | 
    {                                                                                                             | 
      v2 = -1073741727;                                                                                           | 
    }                                                                                                             | 
  }                                                                                                               | 
  CmpCleanupThreadInfo(&v16);                                                                                     | 
  return (unsigned int)v2;                                                                                        | 
}                                                                                                                 | 
90 row(s)
