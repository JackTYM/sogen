__int64 __fastcall NtCreateCpuPartition(HANDLE *a1, unsigned int a2, __int64 a3, __int128 *a4, int a5)             | 
{                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                    | 
                                                                                                                   | 
  v20 = 0LL;                                                                                                       | 
  v7 = 0;                                                                                                          | 
  v18 = 0LL;                                                                                                       | 
  v8 = 0LL;                                                                                                        | 
  Object = 0LL;                                                                                                    | 
  v17 = 0LL;                                                                                                       | 
  Handle = 0LL;                                                                                                    | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                               | 
  if ( PreviousMode )                                                                                              | 
  {                                                                                                                | 
    v10 = 0x7FFFFFFF0000LL;                                                                                        | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                                 | 
    {                                                                                                              | 
      v10 = (__int64)a1;                                                                                           | 
    }                                                                                                              | 
    *(_QWORD *)v10 = *(_QWORD *)v10;                                                                               | 
  }                                                                                                                | 
  while ( a5 )                                                                                                     | 
  {                                                                                                                | 
    if ( PreviousMode )                                                                                            | 
    {                                                                                                              | 
      if ( ((unsigned __int8)a4 & 7) != 0 )                                                                        | 
      {                                                                                                            | 
        ExRaiseDatatypeMisalignment();                                                                             | 
      }                                                                                                            | 
      v20 = *a4;                                                                                                   | 
    }                                                                                                              | 
    else                                                                                                           | 
    {                                                                                                              | 
      v20 = *a4;                                                                                                   | 
    }                                                                                                              | 
    if ( (unsigned __int8)v20 == 1LL )                                                                             | 
    {                                                                                                              | 
      if ( v8 )                                                                                                    | 
      {                                                                                                            | 
LABEL_15:                                                                                                          | 
        PermanentObject = -1073741811;                                                                             | 
        goto LABEL_27;                                                                                             | 
      }                                                                                                            | 
      v8 = *((_QWORD *)&v20 + 1);                                                                                  | 
    }                                                                                                              | 
    else                                                                                                           | 
    {                                                                                                              | 
      if ( (unsigned __int8)v20 != 2LL )                                                                           | 
      {                                                                                                            | 
        goto LABEL_15;                                                                                             | 
      }                                                                                                            | 
      v7 = BYTE8(v20);                                                                                             | 
    }                                                                                                              | 
    ++a4;                                                                                                          | 
    --a5;                                                                                                          | 
  }                                                                                                                | 
  if ( !v8 )                                                                                                       | 
  {                                                                                                                | 
    v8 = -1LL;                                                                                                     | 
  }                                                                                                                | 
  PermanentObject = PspReferenceCpuPartitionByHandle(v8, (__int64)&v18);                                           | 
  if ( PermanentObject >= 0 )                                                                                      | 
  {                                                                                                                | 
    LOBYTE(v13) = PreviousMode;                                                                                    | 
    PermanentObject = PsCreateCpuPartition(a3, a2, v13, 0LL, v18, &Object, &v17);                                  | 
    if ( PermanentObject >= 0 )                                                                                    | 
    {                                                                                                              | 
      v14 = Object;                                                                                                | 
      PermanentObject = PspQueryAndCheckCpuPartitionName(Object);                                                  | 
      if ( PermanentObject >= 0 )                                                                                  | 
      {                                                                                                            | 
        if ( !v7                                                                                                   | 
          || (PermanentObject = ObOpenObjectByPointer(v14, 0x200u, 0LL, 0xF0007u, PsCpuPartitionType, 0, &Handle), | 
              PermanentObject >= 0)                                                                                | 
          && (PermanentObject = ZwMakePermanentObject(Handle), PermanentObject >= 0) )                             | 
        {                                                                                                          | 
          *a1 = v17;                                                                                               | 
          v17 = 0LL;                                                                                               | 
        }                                                                                                          | 
      }                                                                                                            | 
    }                                                                                                              | 
  }                                                                                                                | 
LABEL_27:                                                                                                          | 
  if ( v18 )                                                                                                       | 
  {                                                                                                                | 
    ObfDereferenceObjectWithTag(v18, 0x50707350u);                                                                 | 
  }                                                                                                                | 
  if ( Handle )                                                                                                    | 
  {                                                                                                                | 
    ZwClose(Handle);                                                                                               | 
  }                                                                                                                | 
  if ( v17 )                                                                                                       | 
  {                                                                                                                | 
    ObCloseHandle(v17, PreviousMode);                                                                              | 
  }                                                                                                                | 
  if ( Object )                                                                                                    | 
  {                                                                                                                | 
    ObfDereferenceObject(Object);                                                                                  | 
  }                                                                                                                | 
  return (unsigned int)PermanentObject;                                                                            | 
}                                                                                                                  | 
101 row(s)
