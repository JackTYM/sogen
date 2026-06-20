__int64 __fastcall NtDebugActiveProcess(ULONG_PTR a1, void *a2)                                                     | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                | 
  Object[0] = 0LL;                                                                                                  | 
  Object[1] = 0LL;                                                                                                  | 
  result = ObpReferenceObjectByHandleWithTag(a1, 0x4F676244u, (__int64)Object, 0LL, 0LL);                           | 
  if ( (int)result >= 0 )                                                                                           | 
  {                                                                                                                 | 
    CurrentThread = KeGetCurrentThread();                                                                           | 
    v7 = (struct _EX_RUNDOWN_REF *)Object[0];                                                                       | 
    Process = CurrentThread->ApcState.Process;                                                                      | 
    if ( Object[0] == Process || Object[0] == PsInitialSystemProcess )                                              | 
    {                                                                                                               | 
      v10 = -1073741790;                                                                                            | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      LOBYTE(v5) = PreviousMode;                                                                                    | 
      if ( (unsigned __int8)PsTestProtectedProcessIncompatibility(v5, CurrentThread->ApcState.Process, Object[0]) ) | 
      {                                                                                                             | 
        v10 = -1073740014;                                                                                          | 
      }                                                                                                             | 
      else                                                                                                          | 
      {                                                                                                             | 
        Count = v7[46].Count;                                                                                       | 
        if ( (Count & 1) == 0                                                                                       | 
          || (memset_0(v17, 0, 0x68uLL),                                                                            | 
              v18 = Count,                                                                                          | 
              v19 = 1LL,                                                                                            | 
              LOBYTE(v15) = 2,                                                                                      | 
              v10 = VslpEnterIumSecureMode(v15, 12LL, 0LL, v17),                                                    | 
              v10 >= 0) )                                                                                           | 
        {                                                                                                           | 
          if ( !Process[1].ReadyTime                                                                                | 
            || (v13 = WORD2(Process[3].PerProcessorCycleTimes), v13 != 332) && v13 != 452                           | 
            || v7[98].Count && ((v14 = WORD2(v7[221].Ptr), v14 == 332) || v14 == 452) )                             | 
          {                                                                                                         | 
            Object[0] = 0LL;                                                                                        | 
            v10 = ObReferenceObjectByHandle(a2, 2u, DbgkDebugObjectType, PreviousMode, Object, 0LL);                | 
            if ( v10 >= 0 )                                                                                         | 
            {                                                                                                       | 
              v11 = ExAcquireRundownProtection_0(v7 + 61);                                                          | 
              v12 = (struct _KEVENT *)Object[0];                                                                    | 
              if ( v11 )                                                                                            | 
              {                                                                                                     | 
                DbgkpPostFakeProcessCreateMessages((ULONG_PTR)v7);                                                  | 
                v10 = DbgkpSetProcessDebugObject((PRKPROCESS)v7, v12);                                              | 
                ExReleaseRundownProtection_0(v7 + 61);                                                              | 
              }                                                                                                     | 
              else                                                                                                  | 
              {                                                                                                     | 
                v10 = -1073741558;                                                                                  | 
              }                                                                                                     | 
              ObfDereferenceObject(v12);                                                                            | 
            }                                                                                                       | 
          }                                                                                                         | 
          else                                                                                                      | 
          {                                                                                                         | 
            v10 = -1073741637;                                                                                      | 
          }                                                                                                         | 
        }                                                                                                           | 
      }                                                                                                             | 
    }                                                                                                               | 
    ObfDereferenceObjectWithTag(v7, 0x4F676244u);                                                                   | 
    return (unsigned int)v10;                                                                                       | 
  }                                                                                                                 | 
  return result;                                                                                                    | 
}                                                                                                                   | 
70 row(s)
