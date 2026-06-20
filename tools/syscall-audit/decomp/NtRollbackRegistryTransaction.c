__int64 __fastcall NtRollbackRegistryTransaction(HANDLE Handle, int a2) | 
{                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]         | 
                                                                        | 
  v10 = 0LL;                                                            | 
  v9 = 0LL;                                                             | 
  memset(&ApcState, 0, sizeof(ApcState));                               | 
  CmpInitializeThreadInfo(&v9);                                         | 
  if ( (unsigned __int8)CmpAcquireShutdownRundown() )                   | 
  {                                                                     | 
    if ( a2 )                                                           | 
    {                                                                   | 
      v4 = -1073741811;                                                 | 
    }                                                                   | 
    else                                                                | 
    {                                                                   | 
      Object = 0LL;                                                     | 
      v5 = ObReferenceObjectByHandle(                                   | 
             Handle,                                                    | 
             0x10u,                                                     | 
             CmRegistryTransactionType,                                 | 
             KeGetCurrentThread()->PreviousMode,                        | 
             &Object,                                                   | 
             0LL);                                                      | 
      v6 = Object;                                                      | 
      v4 = v5;                                                          | 
      if ( v5 >= 0 )                                                    | 
      {                                                                 | 
        CmpAttachToRegistryProcess(&ApcState);                          | 
        v4 = CmpRollbackLightWeightTransaction(v6);                     | 
        CmpDetachFromRegistryProcess(&ApcState);                        | 
        if ( v4 >= 0 )                                                  | 
        {                                                               | 
          v4 = 0;                                                       | 
        }                                                               | 
      }                                                                 | 
      if ( v6 )                                                         | 
      {                                                                 | 
        ObfDereferenceObject(v6);                                       | 
      }                                                                 | 
    }                                                                   | 
    CmpReleaseShutdownRundown();                                        | 
  }                                                                     | 
  else                                                                  | 
  {                                                                     | 
    v4 = -1073741431;                                                   | 
  }                                                                     | 
  CmpCleanupThreadInfo(&v9);                                            | 
  return (unsigned int)v4;                                              | 
}                                                                       | 
50 row(s)
