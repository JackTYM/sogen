__int64 __fastcall NtLockRegistryKey(int a1)                                    | 
{                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                 | 
                                                                                | 
  v6 = 0LL;                                                                     | 
  v5 = 0LL;                                                                     | 
  CmpInitializeThreadInfo(&v5);                                                 | 
  Object = 0LL;                                                                 | 
  if ( KeGetCurrentThread()->PreviousMode )                                     | 
  {                                                                             | 
    v2 = -1073741727;                                                           | 
  }                                                                             | 
  else if ( (unsigned __int8)CmpAcquireShutdownRundown() )                      | 
  {                                                                             | 
    v2 = CmObReferenceObjectByHandle(a1, 131078, v3, 0, (__int64)&Object, 0LL); | 
    if ( v2 >= 0 )                                                              | 
    {                                                                           | 
      v2 = CmLockKeyForWrite(Object);                                           | 
      if ( v2 >= 0 )                                                            | 
      {                                                                         | 
        v2 = 0;                                                                 | 
      }                                                                         | 
    }                                                                           | 
    if ( Object )                                                               | 
    {                                                                           | 
      ObfDereferenceObject(Object);                                             | 
    }                                                                           | 
    CmpReleaseShutdownRundown();                                                | 
  }                                                                             | 
  else                                                                          | 
  {                                                                             | 
    v2 = -1073741431;                                                           | 
  }                                                                             | 
  CmpCleanupThreadInfo(&v5);                                                    | 
  return (unsigned int)v2;                                                      | 
}                                                                               | 
36 row(s)
