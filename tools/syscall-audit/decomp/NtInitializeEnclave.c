__int64 __fastcall NtInitializeEnclave(                                                                     | 
        ULONG_PTR BugCheckParameter1,                                                                       | 
        __int64 a2,                                                                                         | 
        const void *a3,                                                                                     | 
        unsigned int a4,                                                                                    | 
        _DWORD *a5)                                                                                         | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v5 = a4;                                                                                                  | 
  v21 = a2;                                                                                                 | 
  v19 = BugCheckParameter1;                                                                                 | 
  v20 = a5;                                                                                                 | 
  memset(v22, 0, sizeof(v22));                                                                              | 
  Object = 0LL;                                                                                             | 
  v16 = 0;                                                                                                  | 
  Pool = 0LL;                                                                                               | 
  v18 = 0LL;                                                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                        | 
  if ( a5 && PreviousMode == 1 )                                                                            | 
  {                                                                                                         | 
    v10 = 0x7FFFFFFF0000LL;                                                                                 | 
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                          | 
    {                                                                                                       | 
      v10 = (__int64)a5;                                                                                    | 
    }                                                                                                       | 
    *(_DWORD *)v10 = *(_DWORD *)v10;                                                                        | 
  }                                                                                                         | 
  if ( !a4 )                                                                                                | 
  {                                                                                                         | 
    goto LABEL_12;                                                                                          | 
  }                                                                                                         | 
  if ( a4 <= 0x1000 )                                                                                       | 
  {                                                                                                         | 
    Pool = (void *)MiAllocatePool(0x100uLL, a4);                                                            | 
    v18 = Pool;                                                                                             | 
    if ( !Pool )                                                                                            | 
    {                                                                                                       | 
      v11 = -1073741670;                                                                                    | 
      goto LABEL_17;                                                                                        | 
    }                                                                                                       | 
    memmove(Pool, a3, v5);                                                                                  | 
LABEL_12:                                                                                                   | 
    CurrentThread = KeGetCurrentThread();                                                                   | 
    Process = CurrentThread->ApcState.Process;                                                              | 
    if ( BugCheckParameter1 == -1LL )                                                                       | 
    {                                                                                                       | 
      Object = CurrentThread->ApcState.Process;                                                             | 
    }                                                                                                       | 
    else                                                                                                    | 
    {                                                                                                       | 
      v11 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x6D566D4Du, (__int64)&Object, 0LL, 0LL); | 
      if ( v11 < 0 )                                                                                        | 
      {                                                                                                     | 
        goto LABEL_17;                                                                                      | 
      }                                                                                                     | 
      KiStackAttachProcess((ULONG_PTR)Object);                                                              | 
    }                                                                                                       | 
    v11 = MiInitializeEnclave((_DWORD)Process, v21, (_DWORD)Pool, v5, (__int64)&v16);                       | 
    goto LABEL_17;                                                                                          | 
  }                                                                                                         | 
  v11 = -1073741820;                                                                                        | 
LABEL_17:                                                                                                   | 
  v14 = Object;                                                                                             | 
  if ( Object && BugCheckParameter1 != -1LL )                                                               | 
  {                                                                                                         | 
    KiUnstackDetachProcess(v22, 0LL);                                                                       | 
    ObfDereferenceObjectWithTag(v14, 0x6D566D4Du);                                                          | 
  }                                                                                                         | 
  if ( a5 )                                                                                                 | 
  {                                                                                                         | 
    *a5 = v16;                                                                                              | 
  }                                                                                                         | 
  if ( Pool )                                                                                               | 
  {                                                                                                         | 
    ExFreePoolWithTag(Pool, 0);                                                                             | 
  }                                                                                                         | 
  return (unsigned int)v11;                                                                                 | 
}                                                                                                           | 
79 row(s)
