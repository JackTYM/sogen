__int64 __fastcall NtTerminateJobObject(void *a1)                                                                    | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  Object = 0LL;                                                                                                      | 
  v1 = ObReferenceObjectByHandle(a1, 8u, (POBJECT_TYPE)PsJobType, KeGetCurrentThread()->PreviousMode, &Object, 0LL); | 
  v2 = v1;                                                                                                           | 
  if ( v1 < 0 )                                                                                                      | 
  {                                                                                                                  | 
    if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                      | 
    {                                                                                                                | 
      EtwTraceJob(0LL, 0LL, (unsigned int)v1, 1825LL);                                                               | 
    }                                                                                                                | 
  }                                                                                                                  | 
  else                                                                                                               | 
  {                                                                                                                  | 
    PspTerminateAllProcessesInJobHierarchy((PRKEVENT)Object);                                                        | 
    ObfDereferenceObject(Object);                                                                                    | 
  }                                                                                                                  | 
  return v2;                                                                                                         | 
}                                                                                                                    | 
21 row(s)
