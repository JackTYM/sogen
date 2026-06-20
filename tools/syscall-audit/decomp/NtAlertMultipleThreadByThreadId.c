__int64 __fastcall NtAlertMultipleThreadByThreadId(__int64 a1, unsigned int a2, __int128 *a3, int a4) | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v8 = 0LL;                                                                                           | 
  pullResult = 0LL;                                                                                   | 
  memset_0(v25, 0, sizeof(v25));                                                                      | 
  Pool2 = 0LL;                                                                                        | 
  v19 = 0LL;                                                                                          | 
  Thread = 0LL;                                                                                       | 
  CurrentThreadProcess = PsGetCurrentThreadProcess(v10);                                              | 
  v22 = a2;                                                                                           | 
  v11 = RtlULongLongMult(a2, 8uLL, &pullResult);                                                      | 
  if ( v11 >= 0 )                                                                                     | 
  {                                                                                                   | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                                | 
    if ( PreviousMode && pullResult && (a1 & 7) != 0 )                                                | 
    {                                                                                                 | 
      ExRaiseDatatypeMisalignment();                                                                  | 
    }                                                                                                 | 
    while ( a4 )                                                                                      | 
    {                                                                                                 | 
      if ( PreviousMode )                                                                             | 
      {                                                                                               | 
        if ( ((unsigned __int8)a3 & 7) != 0 )                                                         | 
        {                                                                                             | 
          ExRaiseDatatypeMisalignment();                                                              | 
        }                                                                                             | 
        v23 = *a3;                                                                                    | 
      }                                                                                               | 
      else                                                                                            | 
      {                                                                                               | 
        v23 = *a3;                                                                                    | 
      }                                                                                               | 
      if ( (_BYTE)v23 )                                                                               | 
      {                                                                                               | 
        v11 = -1073741811;                                                                            | 
        goto LABEL_26;                                                                                | 
      }                                                                                               | 
      v8 = *((_QWORD *)&v23 + 1);                                                                     | 
      --a4;                                                                                           | 
      ++a3;                                                                                           | 
    }                                                                                                 | 
    if ( a2 <= 0x40 )                                                                                 | 
    {                                                                                                 | 
      Pool2 = v25;                                                                                    | 
      v19 = v25;                                                                                      | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      Pool2 = (_BYTE *)ExAllocatePool2(0x43uLL, pullResult, 0x6D41654Bu);                             | 
      v19 = Pool2;                                                                                    | 
      if ( !Pool2 )                                                                                   | 
      {                                                                                               | 
        v11 = -1073741801;                                                                            | 
        goto LABEL_26;                                                                                | 
      }                                                                                               | 
    }                                                                                                 | 
    memset_0(Pool2, 0, pullResult);                                                                   | 
    for ( i = 0LL; (unsigned int)i < a2; i = (unsigned int)(i + 1) )                                  | 
    {                                                                                                 | 
      v24 = *(HANDLE *)(a1 + 8 * i);                                                                  | 
      v11 = PsLookupThreadByThreadId(v24, &Thread);                                                   | 
      if ( v11 < 0 )                                                                                  | 
      {                                                                                               | 
        goto LABEL_26;                                                                                | 
      }                                                                                               | 
      v14 = Thread;                                                                                   | 
      *(_QWORD *)&Pool2[8 * i] = Thread;                                                              | 
      if ( (_KPROCESS *)CurrentThreadProcess != v14->Process )                                        | 
      {                                                                                               | 
        v11 = -1073741790;                                                                            | 
        goto LABEL_26;                                                                                | 
      }                                                                                               | 
    }                                                                                                 | 
    KeAlertMultipleThreadByThreadId(Pool2, a2, v8);                                                   | 
    v11 = 0;                                                                                          | 
  }                                                                                                   | 
LABEL_26:                                                                                             | 
  if ( Pool2 )                                                                                        | 
  {                                                                                                   | 
    if ( a2 )                                                                                         | 
    {                                                                                                 | 
      v15 = (PVOID *)Pool2;                                                                           | 
      v16 = v22;                                                                                      | 
      do                                                                                              | 
      {                                                                                               | 
        if ( *v15 )                                                                                   | 
        {                                                                                             | 
          ObfDereferenceObject(*v15);                                                                 | 
        }                                                                                             | 
        ++v15;                                                                                        | 
        --v16;                                                                                        | 
      }                                                                                               | 
      while ( v16 );                                                                                  | 
    }                                                                                                 | 
    if ( Pool2 != v25 )                                                                               | 
    {                                                                                                 | 
      ExFreePoolWithTag(Pool2, 0);                                                                    | 
    }                                                                                                 | 
  }                                                                                                   | 
  return (unsigned int)v11;                                                                           | 
}                                                                                                     | 
103 row(s)
