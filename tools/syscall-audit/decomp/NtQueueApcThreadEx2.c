NTSTATUS __fastcall NtQueueApcThreadEx2(void *a1, void *a2, int a3, __int64 a4, __int64 a5, __int64 a6, __int64 a7) | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  v10 = 1;                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                | 
  if ( (a3 & 0xFFFEFFFE) != 0 )                                                                                     | 
  {                                                                                                                 | 
    return -1073741811;                                                                                             | 
  }                                                                                                                 | 
  if ( (a3 & 1) == 0 )                                                                                              | 
  {                                                                                                                 | 
    v12 = 0;                                                                                                        | 
    goto LABEL_4;                                                                                                   | 
  }                                                                                                                 | 
  if ( a2 )                                                                                                         | 
  {                                                                                                                 | 
    return -1073741811;                                                                                             | 
  }                                                                                                                 | 
  v12 = 1;                                                                                                          | 
LABEL_4:                                                                                                            | 
  Object = 0LL;                                                                                                     | 
  result = ObReferenceObjectByHandle(a1, 0x10u, (POBJECT_TYPE)PsThreadType, PreviousMode, &Object, 0LL);            | 
  if ( result < 0 )                                                                                                 | 
  {                                                                                                                 | 
    return result;                                                                                                  | 
  }                                                                                                                 | 
  v14 = Object;                                                                                                     | 
  if ( (*((_DWORD *)Object + 29) & 0x400) != 0                                                                      | 
    || (v14 = Object, Process = KeGetCurrentThread()->ApcState.Process, Process[1].ReadyTime)                       | 
    && ((v20 = WORD2(Process[3].PerProcessorCycleTimes), v20 == 332) || v20 == 452)                                 | 
    && ((v21 = *((_QWORD *)Object + 68), !*(_QWORD *)(v21 + 784)) || *(_WORD *)(v21 + 1772) == 0x8664)              | 
    && (unsigned __int64)-(a4 >> 2) <= 0xFFFFFFFF )                                                                 | 
  {                                                                                                                 | 
    v19 = -1073741816;                                                                                              | 
    goto LABEL_15;                                                                                                  | 
  }                                                                                                                 | 
  if ( !a2 )                                                                                                        | 
  {                                                                                                                 | 
    Pool2 = (char *)ExAllocatePool2(0x41uLL, 0x58uLL, 0x70617350u);                                                 | 
    if ( !Pool2 )                                                                                                   | 
    {                                                                                                               | 
      v19 = -1073741801;                                                                                            | 
      goto LABEL_15;                                                                                                | 
    }                                                                                                               | 
    v17 = (__int64 (__fastcall *)())KeSpecialUserApcKernelRoutine;                                                  | 
    v10 = v12 ^ 1;                                                                                                  | 
    v18 = ExFreePool;                                                                                               | 
    if ( !v12 )                                                                                                     | 
    {                                                                                                               | 
      v17 = PspUserApcKernelRoutine;                                                                                | 
    }                                                                                                               | 
LABEL_11:                                                                                                           | 
    KeInitializeApc((_DWORD)Pool2, (_DWORD)v14, 0, (_DWORD)v17, (__int64)v18, a4, v10, a5);                         | 
    if ( (a3 & 0x10000) != 0 )                                                                                      | 
    {                                                                                                               | 
      Pool2[1] |= 1u;                                                                                               | 
    }                                                                                                               | 
    if ( (unsigned __int8)KeInsertQueueApc(Pool2, a6, a7, 0LL) )                                                    | 
    {                                                                                                               | 
      v19 = 0;                                                                                                      | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      guard_dispatch_icall_no_overrides(Pool2);                                                                     | 
      v19 = -1073741823;                                                                                            | 
    }                                                                                                               | 
    goto LABEL_15;                                                                                                  | 
  }                                                                                                                 | 
  v23 = 0LL;                                                                                                        | 
  v19 = ObReferenceObjectByHandle(a2, 2u, PspMemoryReserveObjectTypes, PreviousMode, &v23, 0LL);                    | 
  if ( v19 >= 0 )                                                                                                   | 
  {                                                                                                                 | 
    if ( _InterlockedCompareExchange((volatile signed __int32 *)v23, 1, 0) )                                        | 
    {                                                                                                               | 
      ObfDereferenceObject(v23);                                                                                    | 
      v14 = Object;                                                                                                 | 
      v19 = -1073741584;                                                                                            | 
      goto LABEL_15;                                                                                                | 
    }                                                                                                               | 
    v17 = PspUserApcReserveKernelRoutine;                                                                           | 
    v14 = Object;                                                                                                   | 
    v18 = (void (__stdcall *)(PVOID))PspUserApcReserveRundownRoutine;                                               | 
    Pool2 = (char *)v23 + 8;                                                                                        | 
    goto LABEL_11;                                                                                                  | 
  }                                                                                                                 | 
LABEL_15:                                                                                                           | 
  ObfDereferenceObject(v14);                                                                                        | 
  return v19;                                                                                                       | 
}                                                                                                                   | 
