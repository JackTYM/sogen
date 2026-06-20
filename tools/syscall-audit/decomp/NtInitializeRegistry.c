__int64 NtInitializeRegistry()                                  | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  v7 = 0LL;                                                     | 
  v6 = 0LL;                                                     | 
  CmpInitializeThreadInfo(&v6);                                 | 
  LOBYTE(v1) = KeGetCurrentThread()->PreviousMode;              | 
  if ( (_BYTE)v1 )                                              | 
  {                                                             | 
    if ( v0 != 5096 )                                           | 
    {                                                           | 
      BackupHive = ZwInitializeRegistry(v0);                    | 
LABEL_14:                                                       | 
      v3 = BackupHive;                                          | 
      goto LABEL_15;                                            | 
    }                                                           | 
    if ( SeSinglePrivilegeCheck(SeBackupPrivilege, v1) )        | 
    {                                                           | 
      BackupHive = CmpSyncNextBackupHive();                     | 
      goto LABEL_14;                                            | 
    }                                                           | 
    v3 = 3221225569LL;                                          | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    if ( (unsigned __int16)(v0 - 4096) <= 0x3E7u )              | 
    {                                                           | 
      BackupHive = CmpAcceptBoot(v0);                           | 
      goto LABEL_14;                                            | 
    }                                                           | 
    if ( v0 == 2 )                                              | 
    {                                                           | 
      BackupHive = CmpHandlePageFileOpenNotification();         | 
      goto LABEL_14;                                            | 
    }                                                           | 
    if ( v0 < 2u )                                              | 
    {                                                           | 
      BackupHive = CmCompleteRegistryInitialization(v0);        | 
      goto LABEL_14;                                            | 
    }                                                           | 
    v3 = 3221225485LL;                                          | 
  }                                                             | 
LABEL_15:                                                       | 
  CmpCleanupThreadInfo(&v6, v1, v3);                            | 
  return v4;                                                    | 
}                                                               | 
47 row(s)
