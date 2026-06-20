__int64 __fastcall NtSetWnfProcessNotificationEvent(HANDLE Handle)                                           | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  CurrentThread = KeGetCurrentThread();                                                                      | 
  --CurrentThread->KernelApcDisable;                                                                         | 
  Process = KeGetCurrentThread()->ApcState.Process;                                                          | 
  v4 = *(_QWORD *)&Process[3].BasePriority;                                                                  | 
  v8 = v4;                                                                                                   | 
  if ( !v4 )                                                                                                 | 
  {                                                                                                          | 
    v5 = ExpWnfCreateProcessContext(Process, &v8);                                                           | 
    if ( v5 < 0 )                                                                                            | 
    {                                                                                                        | 
      goto LABEL_5;                                                                                          | 
    }                                                                                                        | 
    v4 = v8;                                                                                                 | 
  }                                                                                                          | 
  Object = 0LL;                                                                                              | 
  v5 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExEventObjectType, 1, &Object, 0LL);              | 
  if ( v5 >= 0 )                                                                                             | 
  {                                                                                                          | 
    if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v4 + 128), (signed __int64)Object, 0LL) ) | 
    {                                                                                                        | 
      ObfDereferenceObject(Object);                                                                          | 
      v5 = -1073740008;                                                                                      | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      v5 = 0;                                                                                                | 
    }                                                                                                        | 
  }                                                                                                          | 
LABEL_5:                                                                                                     | 
  KeLeaveCriticalRegion();                                                                                   | 
  return (unsigned int)v5;                                                                                   | 
}                                                                                                            | 
36 row(s)
