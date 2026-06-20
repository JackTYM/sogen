__int64 __fastcall NtConvertBetweenAuxiliaryCounterAndPerformanceCounter( | 
        __int64 a1,                                                       | 
        _QWORD *a2,                                                       | 
        _QWORD *a3,                                                       | 
        _QWORD *a4)                                                       | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  v10[0] = 0LL;                                                           | 
  v9 = 0LL;                                                               | 
  if ( KeGetCurrentThread()->PreviousMode )                               | 
  {                                                                       | 
    if ( ((unsigned __int8)a2 & 3) != 0 )                                 | 
    {                                                                     | 
      ExRaiseDatatypeMisalignment();                                      | 
    }                                                                     | 
    v6 = *a2;                                                             | 
    v10[1] = *a2;                                                         | 
    ProbeForWrite(a3, 8uLL, 4u);                                          | 
    if ( a4 )                                                             | 
    {                                                                     | 
      ProbeForWrite(a4, 8uLL, 4u);                                        | 
    }                                                                     | 
    v7 = guard_dispatch_icall_no_overrides(v6, &v9, v10);                 | 
    if ( v7 >= 0 )                                                        | 
    {                                                                     | 
      *a3 = v9;                                                           | 
      if ( a4 )                                                           | 
      {                                                                   | 
        *a4 = v10[0];                                                     | 
      }                                                                   | 
    }                                                                     | 
  }                                                                       | 
  else                                                                    | 
  {                                                                       | 
    return (unsigned int)guard_dispatch_icall_no_overrides(*a2, a3, a4);  | 
  }                                                                       | 
  return (unsigned int)v7;                                                | 
}                                                                         | 
39 row(s)
