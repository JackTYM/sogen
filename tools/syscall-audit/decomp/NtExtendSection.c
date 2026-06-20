__int64 __fastcall NtExtendSection(ULONG_PTR BugCheckParameter1, unsigned __int64 a2)                     | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  Object[0] = 0LL;                                                                                        | 
  v8 = 0LL;                                                                                               | 
  if ( KeGetCurrentThread()->PreviousMode )                                                               | 
  {                                                                                                       | 
    if ( (a2 & 3) != 0 )                                                                                  | 
    {                                                                                                     | 
      ExRaiseDatatypeMisalignment();                                                                      | 
    }                                                                                                     | 
    v5 = 0x7FFFFFFF0000LL;                                                                                | 
    if ( a2 < 0x7FFFFFFF0000LL )                                                                          | 
    {                                                                                                     | 
      v5 = a2;                                                                                            | 
    }                                                                                                     | 
    *(_BYTE *)v5 = *(_BYTE *)v5;                                                                          | 
    *(_BYTE *)(v5 + 7) = *(_BYTE *)(v5 + 7);                                                              | 
    v8 = *(_QWORD *)a2;                                                                                   | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v8 = *(_QWORD *)a2;                                                                                   | 
  }                                                                                                       | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x63536D4Du, (__int64)Object, 0LL, 0LL); | 
  if ( (int)result >= 0 )                                                                                 | 
  {                                                                                                       | 
    v7 = MmExtendSection(Object[0], &v8, 0LL);                                                            | 
    ObfDereferenceObjectWithTag(Object[0], 0x63536D4Du);                                                  | 
    *(_QWORD *)a2 = v8;                                                                                   | 
    return v7;                                                                                            | 
  }                                                                                                       | 
  return result;                                                                                          | 
}                                                                                                         | 
35 row(s)
