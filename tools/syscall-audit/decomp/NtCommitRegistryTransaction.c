__int64 __fastcall NtCommitRegistryTransaction(HANDLE Handle, int a2) | 
{                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]       | 
                                                                      | 
  v10 = 0LL;                                                          | 
  v9 = 0LL;                                                           | 
  memset(&ApcState, 0, sizeof(ApcState));                             | 
  CmpInitializeThreadInfo(&v9);                                       | 
  if ( (unsigned __int8)CmpAcquireShutdownRundown() )                 | 
  {                                                                   | 
    if ( a2 )                                                         | 
    {                                                                 | 
      v6 = -1073741811;                                               | 
    }                                                                 | 
    else                                                              | 
    {                                                                 | 
      Object = 0LL;                                                   | 
      v4 = ObReferenceObjectByHandle(                                 | 
             Handle,                                                  | 
             8u,                                                      | 
             CmRegistryTransactionType,                               | 
             KeGetCurrentThread()->PreviousMode,                      | 
             &Object,                                                 | 
             0LL);                                                    | 
      v5 = Object;                                                    | 
      v6 = v4;                                                        | 
      if ( v4 >= 0 )                                                  | 
      {                                                               | 
        CmpAttachToRegistryProcess(&ApcState);                        | 
        v6 = CmpCommitLightWeightTransaction(v5);                     | 
        CmpDetachFromRegistryProcess(&ApcState);                      | 
        if ( v6 >= 0 )                                                | 
        {                                                             | 
          v6 = 0;                                                     | 
        }                                                             | 
      }                                                               | 
      if ( v5 )                                                       | 
      {                                                               | 
        ObfDereferenceObject(v5);                                     | 
      }                                                               | 
    }                                                                 | 
    CmpReleaseShutdownRundown();                                      | 
  }                                                                   | 
  else                                                                | 
  {                                                                   | 
    v6 = -1073741431;                                                 | 
  }                                                                   | 
  CmpCleanupThreadInfo(&v9);                                          | 
  return (unsigned int)v6;                                            | 
}                                                                     | 
50 row(s)
