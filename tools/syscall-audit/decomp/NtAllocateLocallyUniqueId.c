__int64 __fastcall NtAllocateLocallyUniqueId(unsigned __int64 a1)                           | 
{                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                             | 
                                                                                            | 
  if ( KeGetCurrentThread()->PreviousMode )                                                 | 
  {                                                                                         | 
    if ( (a1 & 3) != 0 )                                                                    | 
    {                                                                                       | 
      ExRaiseDatatypeMisalignment();                                                        | 
    }                                                                                       | 
    v2 = 0x7FFFFFFF0000LL;                                                                  | 
    if ( a1 < 0x7FFFFFFF0000LL )                                                            | 
    {                                                                                       | 
      v2 = a1;                                                                              | 
    }                                                                                       | 
    *(_BYTE *)v2 = *(_BYTE *)v2;                                                            | 
    *(_BYTE *)(v2 + 7) = *(_BYTE *)(v2 + 7);                                                | 
  }                                                                                         | 
  *(_QWORD *)a1 = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement); | 
  return 0LL;                                                                               | 
}                                                                                           | 
21 row(s)
