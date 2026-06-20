NTSTATUS __stdcall NtClose(HANDLE Handle)                                                                 | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                      | 
  if ( (MmVerifierData & 0x100) == 0 || PreviousMode || (unsigned __int8)ObpIsKernelHandle(Handle, 0LL) ) | 
  {                                                                                                       | 
    CurrentThread = KeGetCurrentThread();                                                                 | 
    v4 = 0;                                                                                               | 
    v15 = 0;                                                                                              | 
    v5 = (signed __int64)Handle;                                                                          | 
    Process = CurrentThread->ApcState.Process;                                                            | 
    if ( PreviousMode )                                                                                   | 
    {                                                                                                     | 
      goto LABEL_3;                                                                                       | 
    }                                                                                                     | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    VfCheckUserHandle(v14);                                                                               | 
    CurrentThread = KeGetCurrentThread();                                                                 | 
    v5 = (signed __int64)Handle;                                                                          | 
    v15 = 0;                                                                                              | 
    v4 = 0;                                                                                               | 
    Process = CurrentThread->ApcState.Process;                                                            | 
  }                                                                                                       | 
  if ( ((unsigned __int64)Handle & 0xFFFFFFFF80000000uLL) == 0xFFFFFFFF80000000uLL                        | 
    && (unsigned __int64)Handle <= 0xFFFFFFFFFFFFFFFDuLL )                                                | 
  {                                                                                                       | 
    KernelTime = ObpKernelHandleTable;                                                                    | 
    v7 = (struct _EX_RUNDOWN_REF *)PsInitialSystemProcess;                                                | 
    v5 = (unsigned __int64)Handle ^ 0xFFFFFFFF80000000uLL;                                                | 
    goto LABEL_5;                                                                                         | 
  }                                                                                                       | 
LABEL_3:                                                                                                  | 
  v7 = (struct _EX_RUNDOWN_REF *)Process;                                                                 | 
  if ( KeGetCurrentThread()->ApcStateIndex == 1 )                                                         | 
  {                                                                                                       | 
    KernelTime = ObReferenceProcessHandleTable(Process);                                                  | 
    if ( KernelTime )                                                                                     | 
    {                                                                                                     | 
      v4 = 1;                                                                                             | 
      goto LABEL_5;                                                                                       | 
    }                                                                                                     | 
    return -1073741816;                                                                                   | 
  }                                                                                                       | 
  KernelTime = Process[1].KernelTime;                                                                     | 
  if ( KernelTime == ObpKernelHandleTable )                                                               | 
  {                                                                                                       | 
    return -1073741816;                                                                                   | 
  }                                                                                                       | 
LABEL_5:                                                                                                  | 
  --CurrentThread->KernelApcDisable;                                                                      | 
  if ( (v5 & 0x3FC) != 0 )                                                                                | 
  {                                                                                                       | 
    v9 = (signed __int64 *)ExpLookupHandleTableEntry(KernelTime, v5);                                     | 
    if ( v9 )                                                                                             | 
    {                                                                                                     | 
      do                                                                                                  | 
      {                                                                                                   | 
        while ( 1 )                                                                                       | 
        {                                                                                                 | 
          _m_prefetchw(v9);                                                                               | 
          v10 = *v9;                                                                                      | 
          if ( (*v9 & 1) != 0 )                                                                           | 
          {                                                                                               | 
            break;                                                                                        | 
          }                                                                                               | 
          if ( !v10 )                                                                                     | 
          {                                                                                               | 
            goto LABEL_19;                                                                                | 
          }                                                                                               | 
          ExpBlockOnLockedHandleEntry(KernelTime, v9);                                                    | 
        }                                                                                                 | 
      }                                                                                                   | 
      while ( v10 != _InterlockedCompareExchange64(v9, v10 - 1, v10) );                                   | 
      v11 = ObCloseHandleTableEntry(KernelTime, v9, v7, v5, PreviousMode, 0);                             | 
      goto LABEL_10;                                                                                      | 
    }                                                                                                     | 
  }                                                                                                       | 
LABEL_19:                                                                                                 | 
  KeLeaveCriticalRegionThread(CurrentThread);                                                             | 
  if ( v5 <= 0 && v5 >= -6 )                                                                              | 
  {                                                                                                       | 
    goto LABEL_21;                                                                                        | 
  }                                                                                                       | 
  ExQueryHandleExceptionsPermanency(KernelTime, &v16, &v15);                                              | 
  if ( (*(_BYTE *)(KernelTime + 44) & 0x10) != 0 && v15 )                                                 | 
  {                                                                                                       | 
    ExHandleLogBadReference(KernelTime, v5);                                                              | 
  }                                                                                                       | 
  if ( !PreviousMode )                                                                                    | 
  {                                                                                                       | 
    if ( (*(_DWORD *)(&CurrentThread[1].SwapListEntry + 1) & 1) == 0                                      | 
      && Process[1].ProcessListEntry.Blink                                                                | 
      && (_BYTE)KdDebuggerEnabled )                                                                       | 
    {                                                                                                     | 
      KeBugCheckEx(0x93u, v5, 1uLL, 0LL, 0LL);                                                            | 
    }                                                                                                     | 
    goto LABEL_21;                                                                                        | 
  }                                                                                                       | 
  if ( (NtGlobalFlag & 0x400000) == 0 && !Process[1].UserTime && !*(_QWORD *)(KernelTime + 96) )          | 
  {                                                                                                       | 
LABEL_21:                                                                                                 | 
    v11 = -1073741816;                                                                                    | 
    if ( (unsigned __int64)v5 >= 0xFFFFFFFFFFFFFFFAuLL )                                                  | 
    {                                                                                                     | 
      v11 = 0;                                                                                            | 
    }                                                                                                     | 
    goto LABEL_10;                                                                                        | 
  }                                                                                                       | 
  if ( KeGetCurrentThread()->ApcStateIndex == 1 )                                                         | 
  {                                                                                                       | 
    v11 = -1073741816;                                                                                    | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v11 = KeRaiseUserException(3221225480LL);                                                             | 
  }                                                                                                       | 
LABEL_10:                                                                                                 | 
  if ( v4 )                                                                                               | 
  {                                                                                                       | 
    _m_prefetchw(&v7[61]);                                                                                | 
    v12 = v7[61].Count & 0xFFFFFFFFFFFFFFFEuLL;                                                           | 
    if ( v12 != _InterlockedCompareExchange64((volatile signed __int64 *)&v7[61], v12 - 2, v12) )         | 
    {                                                                                                     | 
      ExfReleaseRundownProtection(v7 + 61);                                                               | 
    }                                                                                                     | 
  }                                                                                                       | 
  return v11;                                                                                             | 
}                                                                                                         | 
