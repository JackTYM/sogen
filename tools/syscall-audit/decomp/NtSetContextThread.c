__int64 __fastcall NtSetContextThread(void *a1)                                                                     | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  CurrentThread = KeGetCurrentThread();                                                                             | 
  Object = 0LL;                                                                                                     | 
  v2 = ObReferenceObjectByHandle(a1, 0x10u, (POBJECT_TYPE)PsThreadType, CurrentThread->PreviousMode, &Object, 0LL); | 
  if ( v2 >= 0 )                                                                                                    | 
  {                                                                                                                 | 
    v3 = IoThreadToProcess(CurrentThread);                                                                          | 
    v4 = (struct _KTHREAD *)Object;                                                                                 | 
    if ( (v3[1].DirectoryTableBase & 0x20000) != 0 && v3 == IoThreadToProcess((PETHREAD)Object) )                   | 
    {                                                                                                               | 
      v2 = -1073740278;                                                                                             | 
    }                                                                                                               | 
    else if ( (v4->MiscFlags & 0x400) != 0 || *(_QWORD *)&v4[1].WaitBlockFill11[112] )                              | 
    {                                                                                                               | 
      v2 = -1073741816;                                                                                             | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v2 = PspSetContextThreadInternal(v4, 1);                                                                      | 
    }                                                                                                               | 
    ObfDereferenceObject(v4);                                                                                       | 
  }                                                                                                                 | 
  PspLogAuditSetContextThreadEvent((unsigned int)v2);                                                               | 
  return (unsigned int)v2;                                                                                          | 
}                                                                                                                   | 
