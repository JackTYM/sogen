__int64 __fastcall NtQueryPerformanceCounter(LARGE_INTEGER *a1, LARGE_INTEGER *a2) | 
{                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                    | 
                                                                                   | 
  PerformanceFrequency.QuadPart = 0LL;                                             | 
  if ( KeGetCurrentThread()->PreviousMode )                                        | 
  {                                                                                | 
    if ( ((unsigned __int8)a1 & 3) == 0 )                                          | 
    {                                                                              | 
      v4 = 0x7FFFFFFF0000LL;                                                       | 
      v5 = 0x7FFFFFFF0000LL;                                                       | 
      if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                               | 
      {                                                                            | 
        v5 = (__int64)a1;                                                          | 
      }                                                                            | 
      *(_BYTE *)v5 = *(_BYTE *)v5;                                                 | 
      *(_BYTE *)(v5 + 7) = *(_BYTE *)(v5 + 7);                                     | 
      if ( !a2 )                                                                   | 
      {                                                                            | 
        goto LABEL_10;                                                             | 
      }                                                                            | 
      if ( ((unsigned __int8)a2 & 3) == 0 )                                        | 
      {                                                                            | 
        if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                             | 
        {                                                                          | 
          v4 = (__int64)a2;                                                        | 
        }                                                                          | 
        *(_BYTE *)v4 = *(_BYTE *)v4;                                               | 
        *(_BYTE *)(v4 + 7) = *(_BYTE *)(v4 + 7);                                   | 
LABEL_10:                                                                          | 
        *a1 = KeQueryPerformanceCounter(&PerformanceFrequency);                    | 
        if ( a2 )                                                                  | 
        {                                                                          | 
          *a2 = PerformanceFrequency;                                              | 
        }                                                                          | 
        return 0LL;                                                                | 
      }                                                                            | 
    }                                                                              | 
    ExRaiseDatatypeMisalignment();                                                 | 
  }                                                                                | 
  *a1 = KeQueryPerformanceCounter(&PerformanceFrequency);                          | 
  if ( a2 )                                                                        | 
  {                                                                                | 
    *a2 = PerformanceFrequency;                                                    | 
  }                                                                                | 
  return 0LL;                                                                      | 
}                                                                                  | 
47 row(s)
