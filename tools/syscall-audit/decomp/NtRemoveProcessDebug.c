__int64 __fastcall NtRemoveProcessDebug(ULONG_PTR a1, void *a2)                                                          | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                     | 
  Object[0] = 0LL;                                                                                                       | 
  result = ObpReferenceObjectByHandleWithTag(a1, 0x4F676244u, (__int64)Object, 0LL, 0LL);                                | 
  if ( (int)result >= 0 )                                                                                                | 
  {                                                                                                                      | 
    LOBYTE(v5) = PreviousMode;                                                                                           | 
    v6 = (struct _KPROCESS *)Object[0];                                                                                  | 
    if ( (unsigned __int8)PsTestProtectedProcessIncompatibility(v5, KeGetCurrentThread()->ApcState.Process, Object[0]) ) | 
    {                                                                                                                    | 
      v7 = -1073740014;                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      SecureHandle = v6->SecureState.SecureHandle;                                                                       | 
      if ( (SecureHandle & 1) == 0                                                                                       | 
        || (memset_0(v11, 0, 0x68uLL),                                                                                   | 
            v13 = 0LL,                                                                                                   | 
            v12 = SecureHandle,                                                                                          | 
            LOBYTE(v9) = 2,                                                                                              | 
            v7 = VslpEnterIumSecureMode(v9, 12LL, 0LL, v11),                                                             | 
            v7 >= 0) )                                                                                                   | 
      {                                                                                                                  | 
        Object[0] = 0LL;                                                                                                 | 
        v7 = ObReferenceObjectByHandle(a2, 2u, DbgkDebugObjectType, PreviousMode, Object, 0LL);                          | 
        if ( v7 >= 0 )                                                                                                   | 
        {                                                                                                                | 
          v7 = DbgkClearProcessDebugObject(v6);                                                                          | 
          ObfDereferenceObject(Object[0]);                                                                               | 
        }                                                                                                                | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    ObfDereferenceObjectWithTag(v6, 0x4F676244u);                                                                        | 
    return (unsigned int)v7;                                                                                             | 
  }                                                                                                                      | 
  return result;                                                                                                         | 
}                                                                                                                        | 
40 row(s)
