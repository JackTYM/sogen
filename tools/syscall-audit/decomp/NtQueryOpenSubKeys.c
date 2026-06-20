__int64 __fastcall NtQueryOpenSubKeys(int a1, _DWORD *a2)                                                                 | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v14 = 0LL;                                                                                                              | 
  v15 = 0LL;                                                                                                              | 
  memset_0(v17, 0, sizeof(v17));                                                                                          | 
  v13 = 0;                                                                                                                | 
  memset(&ApcState, 0, sizeof(ApcState));                                                                                 | 
  Object = 0LL;                                                                                                           | 
  CmpInitializeParseContext(v17);                                                                                         | 
  v4 = 0;                                                                                                                 | 
  CmpInitializeThreadInfo(&v14);                                                                                          | 
  v5 = CmpAcquireShutdownRundown();                                                                                       | 
  if ( v5 )                                                                                                               | 
  {                                                                                                                       | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                    | 
    if ( PreviousMode == 1 )                                                                                              | 
    {                                                                                                                     | 
      v8 = 0x7FFFFFFF0000LL;                                                                                              | 
      if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                                      | 
      {                                                                                                                   | 
        v8 = (__int64)a2;                                                                                                 | 
      }                                                                                                                   | 
      *(_DWORD *)v8 = *(_DWORD *)v8;                                                                                      | 
    }                                                                                                                     | 
    v6 = ObReferenceObjectByNameEx(a1, 0, 131097, (_DWORD)CmKeyObjectType, PreviousMode, (__int64)v17, (__int64)&Object); | 
    if ( v6 >= 0 )                                                                                                        | 
    {                                                                                                                     | 
      CmpLockRegistryExclusive();                                                                                         | 
      v4 = 1;                                                                                                             | 
      v9 = Object;                                                                                                        | 
      v6 = CmpPerformKeyBodyDeletionCheck(Object, 0LL);                                                                   | 
      if ( v6 >= 0 )                                                                                                      | 
      {                                                                                                                   | 
        if ( (*(_DWORD *)(v9[1] + 184LL) & 0x40000) != 0 )                                                                | 
        {                                                                                                                 | 
          CmpAttachToRegistryProcess(&ApcState);                                                                          | 
          v10 = CmpSearchForOpenSubKeys(v9[1], 0LL, 0LL);                                                                 | 
          CmpDetachFromRegistryProcess(&ApcState);                                                                        | 
          CmpUnlockRegistry();                                                                                            | 
          v4 = 0;                                                                                                         | 
          *a2 = v10;                                                                                                      | 
          v6 = 0;                                                                                                         | 
        }                                                                                                                 | 
        else                                                                                                              | 
        {                                                                                                                 | 
          v6 = -1073741811;                                                                                               | 
        }                                                                                                                 | 
      }                                                                                                                   | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v6 = -1073741431;                                                                                                     | 
  }                                                                                                                       | 
  if ( v4 )                                                                                                               | 
  {                                                                                                                       | 
    CmpUnlockRegistry();                                                                                                  | 
  }                                                                                                                       | 
  CmpCleanupParseContext(v17, 0LL);                                                                                       | 
  if ( v5 )                                                                                                               | 
  {                                                                                                                       | 
    CmpReleaseShutdownRundown();                                                                                          | 
  }                                                                                                                       | 
  if ( Object )                                                                                                           | 
  {                                                                                                                       | 
    ObfDereferenceObject(Object);                                                                                         | 
  }                                                                                                                       | 
  CmpCleanupThreadInfo(&v14);                                                                                             | 
  return (unsigned int)v6;                                                                                                | 
}                                                                                                                         | 
72 row(s)
