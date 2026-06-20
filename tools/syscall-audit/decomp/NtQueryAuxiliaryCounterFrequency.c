__int64 __fastcall NtQueryAuxiliaryCounterFrequency(_QWORD *a1)     | 
{                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]     | 
                                                                    | 
  v3 = 0LL;                                                         | 
  if ( !KeGetCurrentThread()->PreviousMode )                        | 
  {                                                                 | 
    return ((__int64 (*)(void))KeQueryAuxiliaryCounterFrequency)(); | 
  }                                                                 | 
  ProbeForWrite(a1, 8uLL, 4u);                                      | 
  result = KeQueryAuxiliaryCounterFrequency(&v3);                   | 
  if ( (int)result >= 0 )                                           | 
  {                                                                 | 
    *a1 = v3;                                                       | 
  }                                                                 | 
  return result;                                                    | 
}                                                                   | 
17 row(s)
