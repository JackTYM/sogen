__int64 __fastcall NtGetCompleteWnfStateSubscription(                       | 
        unsigned __int64 Src,                                               | 
        unsigned __int64 a2,                                                | 
        __int64 a3,                                                         | 
        int a4,                                                             | 
        unsigned __int64 a5,                                                | 
        unsigned int a6)                                                    | 
{                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]             | 
                                                                            | 
  v8 = Src;                                                                 | 
  v19 = 0LL;                                                                | 
  v18[0] = 0LL;                                                             | 
  CurrentThread = KeGetCurrentThread();                                     | 
  --CurrentThread->KernelApcDisable;                                        | 
  v10 = -1073741811;                                                        | 
  v11 = *(_QWORD *)&KeGetCurrentThread()->ApcState.Process[3].BasePriority; | 
  if ( v11 )                                                                | 
  {                                                                         | 
    if ( !Src || !a2 )                                                      | 
    {                                                                       | 
      goto LABEL_3;                                                         | 
    }                                                                       | 
    if ( (_DWORD)a3 && (!a4 || a4 == -1073741267) )                         | 
    {                                                                       | 
      v16 = 0x7FFFFFFF0000LL;                                               | 
      if ( Src < 0x7FFFFFFF0000LL )                                         | 
      {                                                                     | 
        v16 = Src;                                                          | 
      }                                                                     | 
      RtlCopyVolatileMemory(&v19, (const void *)v16, 8uLL);                 | 
      v17 = 0x7FFFFFFF0000LL;                                               | 
      if ( a2 < 0x7FFFFFFF0000LL )                                          | 
      {                                                                     | 
        v17 = a2;                                                           | 
      }                                                                     | 
      RtlCopyVolatileMemory(v18, (const void *)v17, 8uLL);                  | 
      v10 = ExpWnfCompleteThreadSubscriptions(v11, &v19, v18[0]);           | 
      if ( v10 >= 0 )                                                       | 
      {                                                                     | 
LABEL_3:                                                                    | 
        a3 = a6;                                                            | 
        if ( a6 )                                                           | 
        {                                                                   | 
          if ( a6 < 0x1030 )                                                | 
          {                                                                 | 
            v10 = -1073741789;                                              | 
          }                                                                 | 
          else                                                              | 
          {                                                                 | 
            v12 = a5;                                                       | 
            if ( (a5 & 7) != 0 )                                            | 
            {                                                               | 
              ExRaiseDatatypeMisalignment();                                | 
            }                                                               | 
            v13 = a6 + a5 - 1;                                              | 
            if ( a5 > v13 || v13 >= 0x7FFFFFFF0000LL )                      | 
            {                                                               | 
              ExRaiseAccessViolation();                                     | 
            }                                                               | 
            v14 = (v13 & 0xFFFFFFFFFFFFF000uLL) + 4096;                     | 
            do                                                              | 
            {                                                               | 
              *(_BYTE *)v12 = *(_BYTE *)v12;                                | 
              v12 = (v12 & 0xFFFFFFFFFFFFF000uLL) + 4096;                   | 
            }                                                               | 
            while ( v12 != v14 );                                           | 
            v10 = ExpWnfDeliverThreadNotifications(v11, a5);                | 
          }                                                                 | 
        }                                                                   | 
      }                                                                     | 
    }                                                                       | 
  }                                                                         | 
  KeLeaveCriticalRegionThread(KeGetCurrentThread(), a2, a3, v8);            | 
  return (unsigned int)v10;                                                 | 
}                                                                           | 
76 row(s)
