__int64 __fastcall NtFreezeRegistry(unsigned int a1)                                     | 
{                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                          | 
                                                                                         | 
  v5 = 0LL;                                                                              | 
  v4 = 0LL;                                                                              | 
  memset(&ApcState, 0, sizeof(ApcState));                                                | 
  CmpInitializeThreadInfo(&v4);                                                          | 
  if ( a1 <= 0x384 )                                                                     | 
  {                                                                                      | 
    if ( SeSinglePrivilegeCheck(SeBackupPrivilege, KeGetCurrentThread()->PreviousMode) ) | 
    {                                                                                    | 
      CmpAttachToRegistryProcess(&ApcState);                                             | 
      v2 = CmFreezeRegistry(a1);                                                         | 
      CmpDetachFromRegistryProcess(&ApcState);                                           | 
    }                                                                                    | 
    else                                                                                 | 
    {                                                                                    | 
      v2 = -1073741727;                                                                  | 
    }                                                                                    | 
  }                                                                                      | 
  else                                                                                   | 
  {                                                                                      | 
    v2 = -1073741811;                                                                    | 
  }                                                                                      | 
  CmpCleanupThreadInfo(&v4);                                                             | 
  return v2;                                                                             | 
}                                                                                        | 
28 row(s)
