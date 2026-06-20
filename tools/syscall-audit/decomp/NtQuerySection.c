__int64 __fastcall NtQuerySection(                                                                                    | 
        ULONG_PTR BugCheckParameter1,                                                                                 | 
        unsigned int a2,                                                                                              | 
        unsigned __int64 a3,                                                                                          | 
        unsigned __int64 a4,                                                                                          | 
        unsigned __int64 *a5)                                                                                         | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v5 = a3;                                                                                                            | 
  Object[0] = 0LL;                                                                                                    | 
  if ( KeGetCurrentThread()->PreviousMode )                                                                           | 
  {                                                                                                                   | 
    if ( a4 )                                                                                                         | 
    {                                                                                                                 | 
      if ( (a3 & 3) != 0 )                                                                                            | 
      {                                                                                                               | 
        ExRaiseDatatypeMisalignment();                                                                                | 
      }                                                                                                               | 
      v8 = a3 + a4 - 1;                                                                                               | 
      if ( a3 > v8 || (v9 = 0x7FFFFFFF0000LL, v8 >= 0x7FFFFFFF0000LL) )                                               | 
      {                                                                                                               | 
        ExRaiseAccessViolation();                                                                                     | 
      }                                                                                                               | 
      v10 = (v8 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                      | 
      do                                                                                                              | 
      {                                                                                                               | 
        *(_BYTE *)a3 = *(_BYTE *)a3;                                                                                  | 
        a3 = (a3 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                     | 
      }                                                                                                               | 
      while ( a3 != v10 );                                                                                            | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v9 = 0x7FFFFFFF0000LL;                                                                                          | 
    }                                                                                                                 | 
    if ( a5 )                                                                                                         | 
    {                                                                                                                 | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                  | 
      {                                                                                                               | 
        v9 = (__int64)a5;                                                                                             | 
      }                                                                                                               | 
      *(_QWORD *)v9 = *(_QWORD *)v9;                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  if ( a2 )                                                                                                           | 
  {                                                                                                                   | 
    if ( a2 == 1 )                                                                                                    | 
    {                                                                                                                 | 
      v11 = 64LL;                                                                                                     | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      if ( a2 != 2 && a2 != 3 )                                                                                       | 
      {                                                                                                               | 
        return 3221225475LL;                                                                                          | 
      }                                                                                                               | 
      v11 = 8LL;                                                                                                      | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v11 = 24LL;                                                                                                       | 
  }                                                                                                                   | 
  if ( a4 < v11 )                                                                                                     | 
  {                                                                                                                   | 
    return 3221225476LL;                                                                                              | 
  }                                                                                                                   | 
  SectionInformation = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x6D566D4Du, (__int64)Object, 0LL, 0LL); | 
  if ( SectionInformation >= 0 )                                                                                      | 
  {                                                                                                                   | 
    v13 = a2;                                                                                                         | 
    v14 = Object[0];                                                                                                  | 
    SectionInformation = MmGetSectionInformation(Object[0], v13, v5);                                                 | 
    if ( SectionInformation >= 0 )                                                                                    | 
    {                                                                                                                 | 
      if ( a5 )                                                                                                       | 
      {                                                                                                               | 
        *a5 = v11;                                                                                                    | 
      }                                                                                                               | 
    }                                                                                                                 | 
    ObfDereferenceObjectWithTag(v14, 0x6D566D4Du);                                                                    | 
  }                                                                                                                   | 
  return (unsigned int)SectionInformation;                                                                            | 
}                                                                                                                     | 
