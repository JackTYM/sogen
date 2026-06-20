__int64 __fastcall NtCreateProcessEx(unsigned __int64 a1, unsigned int a2, __int64 a3, __int64 a4) | 
{                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                    | 
                                                                                                   | 
  if ( !a4 )                                                                                       | 
  {                                                                                                | 
    return 3221225485LL;                                                                           | 
  }                                                                                                | 
  if ( KeGetCurrentThread()->PreviousMode )                                                        | 
  {                                                                                                | 
    v6 = 0x7FFFFFFF0000LL;                                                                         | 
    if ( a1 < 0x7FFFFFFF0000LL )                                                                   | 
    {                                                                                              | 
      v6 = a1;                                                                                     | 
    }                                                                                              | 
    *(_QWORD *)v6 = *(_QWORD *)v6;                                                                 | 
  }                                                                                                | 
  return PspCreateProcess(a1, a2);                                                                 | 
}                                                                                                  | 
19 row(s)
