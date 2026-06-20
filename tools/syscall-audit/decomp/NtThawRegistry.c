__int64 NtThawRegistry()                                                               | 
{                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                        | 
                                                                                       | 
  v2 = 0LL;                                                                            | 
  v3 = 0LL;                                                                            | 
  memset(&ApcState, 0, sizeof(ApcState));                                              | 
  CmpInitializeThreadInfo(&v2);                                                        | 
  if ( SeSinglePrivilegeCheck(SeBackupPrivilege, KeGetCurrentThread()->PreviousMode) ) | 
  {                                                                                    | 
    CmpAttachToRegistryProcess(&ApcState);                                             | 
    v0 = CmThawRegistry();                                                             | 
    CmpDetachFromRegistryProcess(&ApcState);                                           | 
  }                                                                                    | 
  else                                                                                 | 
  {                                                                                    | 
    v0 = -1073741727;                                                                  | 
  }                                                                                    | 
  CmpCleanupThreadInfo(&v2);                                                           | 
  return v0;                                                                           | 
}                                                                                      | 
21 row(s)
