__int64 __fastcall NtOpenPartition(HANDLE *a1, int a2, int a3)                               | 
{                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                              | 
                                                                                             | 
  v3 = a3;                                                                                   | 
  Handle[0] = 0LL;                                                                           | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                         | 
  if ( PreviousMode )                                                                        | 
  {                                                                                          | 
    v6 = 0x7FFFFFFF0000LL;                                                                   | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                           | 
    {                                                                                        | 
      v6 = (__int64)a1;                                                                      | 
    }                                                                                        | 
    *(_QWORD *)v6 = *(_QWORD *)v6;                                                           | 
  }                                                                                          | 
  LOBYTE(a3) = PreviousMode;                                                                 | 
  result = ObOpenObjectByName(v3, (_DWORD)PsPartitionType, a3, 0, a2, 0LL, (__int64)Handle); | 
  if ( (int)result >= 0 )                                                                    | 
  {                                                                                          | 
    *a1 = Handle[0];                                                                         | 
    return (unsigned int)result;                                                             | 
  }                                                                                          | 
  return result;                                                                             | 
}                                                                                            | 
25 row(s)
