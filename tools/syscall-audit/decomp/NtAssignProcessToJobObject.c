__int64 __fastcall NtAssignProcessToJobObject(void *a1, ULONG_PTR a2)                                                   | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  Object = 0LL;                                                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                    | 
  v9 = 0LL;                                                                                                             | 
  v4 = ObReferenceObjectByHandle(a1, 1u, (POBJECT_TYPE)PsJobType, PreviousMode, &v9, 0LL);                              | 
  if ( v4 >= 0                                                                                                          | 
    && (a2 == -7LL || (v4 = ObpReferenceObjectByHandleWithTag(a2, 0x624A7350u, (__int64)&Object, 0LL, 0LL), v4 >= 0)) ) | 
  {                                                                                                                     | 
    v5 = Object;                                                                                                        | 
    v6 = v9;                                                                                                            | 
    v4 = PsAssignProcessToJobObject(v9, (ULONG_PTR)Object);                                                             | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    v5 = Object;                                                                                                        | 
    v6 = v9;                                                                                                            | 
  }                                                                                                                     | 
  if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                           | 
  {                                                                                                                     | 
    EtwTraceJobAssignProcess(v6, v5, (unsigned int)v4);                                                                 | 
  }                                                                                                                     | 
  if ( v5 )                                                                                                             | 
  {                                                                                                                     | 
    ObfDereferenceObjectWithTag(v5, 0x624A7350u);                                                                       | 
  }                                                                                                                     | 
  if ( v6 )                                                                                                             | 
  {                                                                                                                     | 
    ObfDereferenceObject(v6);                                                                                           | 
  }                                                                                                                     | 
  return (unsigned int)v4;                                                                                              | 
}                                                                                                                       | 
34 row(s)
