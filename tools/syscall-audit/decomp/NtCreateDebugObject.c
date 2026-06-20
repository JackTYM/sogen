__int64 __fastcall NtCreateDebugObject(__int64 a1, __int64 a2, int a3, int a4) | 
{                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                | 
                                                                               | 
  v4 = a4;                                                                     | 
  v5 = (_QWORD *)a1;                                                           | 
  v11[0] = 0LL;                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                           | 
  if ( PreviousMode )                                                          | 
  {                                                                            | 
    a1 = 0x7FFFFFFF0000LL;                                                     | 
    if ( (unsigned __int64)v5 < 0x7FFFFFFF0000LL )                             | 
    {                                                                          | 
      a1 = (__int64)v5;                                                        | 
    }                                                                          | 
    *(_QWORD *)a1 = *(_QWORD *)a1;                                             | 
  }                                                                            | 
  *v5 = 0LL;                                                                   | 
  if ( (a4 & 0xFFFFFFFE) != 0 )                                                | 
  {                                                                            | 
    return 3221225485LL;                                                       | 
  }                                                                            | 
  LOBYTE(a4) = PreviousMode;                                                   | 
  LOBYTE(a1) = PreviousMode;                                                   | 
  result = ObCreateObjectEx(a1, (_DWORD)DbgkDebugObjectType, a3, a4);          | 
  if ( (int)result >= 0 )                                                      | 
  {                                                                            | 
    v8 = 1;                                                                    | 
    MEMORY[0x18] = 1;                                                          | 
    MEMORY[0x20] = 0LL;                                                        | 
    MEMORY[0x28] = 0;                                                          | 
    KeInitializeEvent((PRKEVENT)0x30, SynchronizationEvent, 0);                | 
    MEMORY[0x58] = 80LL;                                                       | 
    MEMORY[0x50] = 80LL;                                                       | 
    KeInitializeEvent(0LL, NotificationEvent, 0);                              | 
    MEMORY[0x60] = 2 * (v4 & 1);                                               | 
    Process = KeGetCurrentThread()->ApcState.Process;                          | 
    if ( Process[1].ReadyTime )                                                | 
    {                                                                          | 
      v10 = WORD2(Process[3].PerProcessorCycleTimes);                          | 
      if ( v10 != 332 )                                                        | 
      {                                                                        | 
        v8 = v10 == 452;                                                       | 
      }                                                                        | 
      if ( v8 )                                                                | 
      {                                                                        | 
        MEMORY[0x60] |= 4u;                                                    | 
      }                                                                        | 
    }                                                                          | 
    result = ObInsertObjectEx(0LL, 0, 0LL, (__int64)v11);                      | 
    if ( (int)result >= 0 )                                                    | 
    {                                                                          | 
      *v5 = v11[0];                                                            | 
      return (unsigned int)result;                                             | 
    }                                                                          | 
  }                                                                            | 
  return result;                                                               | 
}                                                                              | 
58 row(s)
