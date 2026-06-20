__int64 __fastcall NtQueryInformationCpuPartition(                                               | 
        ULONG_PTR a1,                                                                            | 
        int a2,                                                                                  | 
        volatile void *a3,                                                                       | 
        unsigned int a4,                                                                         | 
        unsigned int *a5)                                                                        | 
{                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                  | 
                                                                                                 | 
  Object[0] = 0LL;                                                                               | 
  v8 = 0LL;                                                                                      | 
  v18 = 0LL;                                                                                     | 
  v9 = 0;                                                                                        | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                             | 
  if ( a2 )                                                                                      | 
  {                                                                                              | 
    return 3221225475LL;                                                                         | 
  }                                                                                              | 
  if ( PreviousMode )                                                                            | 
  {                                                                                              | 
    v12 = 0x7FFFFFFF0000LL;                                                                      | 
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                               | 
    {                                                                                            | 
      v12 = (__int64)a5;                                                                         | 
    }                                                                                            | 
    *(_DWORD *)v12 = *(_DWORD *)v12;                                                             | 
    ProbeForWrite(a3, a4, 8u);                                                                   | 
  }                                                                                              | 
  v13 = PspReferenceCpuPartitionByHandle(a1, (__int64)Object);                                   | 
  if ( v13 >= 0 )                                                                                | 
  {                                                                                              | 
    if ( a4 < 0x10 || (a4 & 0xF) != 0 )                                                          | 
    {                                                                                            | 
      v13 = -1073741811;                                                                         | 
    }                                                                                            | 
    else                                                                                         | 
    {                                                                                            | 
      v14 = (unsigned __int16)a4 >> 4;                                                           | 
      MaximumGroupCount = KeQueryMaximumGroupCount();                                            | 
      if ( v14 < MaximumGroupCount )                                                             | 
      {                                                                                          | 
        MaximumGroupCount = v14;                                                                 | 
      }                                                                                          | 
      Pool2 = ExAllocatePool2(0x101uLL, 16LL * MaximumGroupCount, 0x50707350u);                  | 
      v8 = (void *)Pool2;                                                                        | 
      v18 = Pool2;                                                                               | 
      if ( Pool2 )                                                                               | 
      {                                                                                          | 
        LOWORD(v17) = 0;                                                                         | 
        v13 = KeQueryCpuPartitionAffinity(*(_QWORD *)Object[0], Pool2, MaximumGroupCount, &v17); | 
        v9 = 16 * (unsigned __int16)v17;                                                         | 
        v17 = v9;                                                                                | 
        if ( v13 >= 0 )                                                                          | 
        {                                                                                        | 
          memmove((void *)a3, v8, v9);                                                           | 
        }                                                                                        | 
      }                                                                                          | 
      else                                                                                       | 
      {                                                                                          | 
        v13 = -1073741670;                                                                       | 
      }                                                                                          | 
    }                                                                                            | 
  }                                                                                              | 
  *a5 = v9;                                                                                      | 
  if ( v8 )                                                                                      | 
  {                                                                                              | 
    ExFreePoolWithTag(v8, 0);                                                                    | 
  }                                                                                              | 
  if ( Object[0] )                                                                               | 
  {                                                                                              | 
    ObfDereferenceObjectWithTag(Object[0], 0x50707350u);                                         | 
  }                                                                                              | 
  return (unsigned int)v13;                                                                      | 
}                                                                                                | 
74 row(s)
