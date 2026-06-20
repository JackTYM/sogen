__int64 __fastcall NtCreateJobObject(__int64 *a1, __int64 a2, int a3)                                                     | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v15 = 0LL;                                                                                                              | 
  v17 = 0;                                                                                                                | 
  CurrentThread = KeGetCurrentThread();                                                                                   | 
  PreviousMode = CurrentThread->PreviousMode;                                                                             | 
  if ( PreviousMode )                                                                                                     | 
  {                                                                                                                       | 
    v7 = 0x7FFFFFFF0000LL;                                                                                                | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                                        | 
    {                                                                                                                     | 
      v7 = (__int64)a1;                                                                                                   | 
    }                                                                                                                     | 
    *(_QWORD *)v7 = *(_QWORD *)v7;                                                                                        | 
  }                                                                                                                       | 
  *a1 = 0LL;                                                                                                              | 
  v8 = PoEnergyEstimationEnabled();                                                                                       | 
  LOBYTE(v9) = PreviousMode;                                                                                              | 
  LOBYTE(v10) = PreviousMode;                                                                                             | 
  Object = ObCreateObjectEx(v10, (_DWORD)PsJobType, a3, v9);                                                              | 
  if ( Object >= 0 )                                                                                                      | 
  {                                                                                                                       | 
    memset_0(0LL, 0, v8 != 0 ? 2272 : 1832);                                                                              | 
    MEMORY[0x520] = 0LL;                                                                                                  | 
    MEMORY[0x30] = 40LL;                                                                                                  | 
    MEMORY[0x28] = 40LL;                                                                                                  | 
    MEMORY[0x510] = 1288LL;                                                                                               | 
    MEMORY[0x508] = 1288LL;                                                                                               | 
    MEMORY[0x500] = 1272LL;                                                                                               | 
    MEMORY[0x4F8] = 1272LL;                                                                                               | 
    MEMORY[0x530] = 1320LL;                                                                                               | 
    MEMORY[0x528] = 1320LL;                                                                                               | 
    KeInitializeEvent(0LL, NotificationEvent, 0);                                                                         | 
    MEMORY[0x4F0] = 0LL;                                                                                                  | 
    MEMORY[0x108] = 2097153LL;                                                                                            | 
    memset_0((void *)0x110, 0, 0x100uLL);                                                                                 | 
    MEMORY[0x2D8] = 2097153LL;                                                                                            | 
    memset_0((void *)0x2E0, 0, 0x100uLL);                                                                                 | 
    MEMORY[0x5F0] = 1512LL;                                                                                               | 
    MEMORY[0x5E8] = 1512LL;                                                                                               | 
    MEMORY[0x5F8] = 0LL;                                                                                                  | 
    if ( v8 )                                                                                                             | 
    {                                                                                                                     | 
      MEMORY[0x618] = 1832LL;                                                                                             | 
    }                                                                                                                     | 
    MEMORY[0x240] = -1;                                                                                                   | 
    MEMORY[0x450] = 16382;                                                                                                | 
    MEMORY[0x6D8] = 1LL;                                                                                                  | 
    MEMORY[0x424] = 10;                                                                                                   | 
    MEMORY[0x244] = 5;                                                                                                    | 
    ExInitializeResourceLite((PERESOURCE)0x38);                                                                           | 
    PspLockJobListExclusive(CurrentThread);                                                                               | 
    if ( *(__int64 **)qword_140FD9070 != &PspJobList )                                                                    | 
    {                                                                                                                     | 
      __fastfail(3u);                                                                                                     | 
    }                                                                                                                     | 
    MEMORY[0x18] = &PspJobList;                                                                                           | 
    MEMORY[0x20] = qword_140FD9070;                                                                                       | 
    *(_QWORD *)qword_140FD9070 = 24LL;                                                                                    | 
    qword_140FD9070 = 24LL;                                                                                               | 
    PspUnlockJobListExclusive(CurrentThread);                                                                             | 
    MEMORY[0x43C] = 5;                                                                                                    | 
    MEMORY[0x440] = 5;                                                                                                    | 
    MEMORY[0x444] = 8;                                                                                                    | 
    MEMORY[0x448] = 8;                                                                                                    | 
    MEMORY[0x610] |= 0x200000u;                                                                                           | 
    v17 = MEMORY[0x610];                                                                                                  | 
    while ( 1 )                                                                                                           | 
    {                                                                                                                     | 
      v12 = ExUuidCreate((UUID *)0x5C0);                                                                                  | 
      if ( v12 != -1073741267 )                                                                                           | 
      {                                                                                                                   | 
        break;                                                                                                            | 
      }                                                                                                                   | 
      Interval.QuadPart = -10000LL;                                                                                       | 
      KeDelayExecutionThread(0, 0, &Interval);                                                                            | 
    }                                                                                                                     | 
    Object = 0;                                                                                                           | 
    if ( v12 != 1073872982 )                                                                                              | 
    {                                                                                                                     | 
      Object = v12;                                                                                                       | 
    }                                                                                                                     | 
    if ( Object >= 0 )                                                                                                    | 
    {                                                                                                                     | 
      Handle = ExCreateHandleEx(PspUniqueJobIdTable, 0, 0, 0, 0LL);                                                       | 
      if ( Handle )                                                                                                       | 
      {                                                                                                                   | 
        MEMORY[0x5BC] = Handle;                                                                                           | 
      }                                                                                                                   | 
      else                                                                                                                | 
      {                                                                                                                   | 
        Object = -1073741670;                                                                                             | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    PspIoRateEntryInitialize(1632LL);                                                                                     | 
    MEMORY[0x6A0] = 0LL;                                                                                                  | 
    MEMORY[0x6A8] = 0LL;                                                                                                  | 
    MEMORY[0x6D0] = 0LL;                                                                                                  | 
    if ( Object < 0 || (PsReferenceSiloContext(0LL), Object = ObInsertObjectEx(0LL, 0, 0LL, (__int64)&v15), Object < 0) ) | 
    {                                                                                                                     | 
      ObfDereferenceObject(0LL);                                                                                          | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      *a1 = v15;                                                                                                          | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                             | 
  {                                                                                                                       | 
    EtwTraceJob(0LL, v17, (unsigned int)Object, 1824LL);                                                                  | 
  }                                                                                                                       | 
  return (unsigned int)Object;                                                                                            | 
}                                                                                                                         | 
115 row(s)
