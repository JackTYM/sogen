__int64 __fastcall NtQueryIntervalProfile(__int64 a1, _DWORD *a2) | 
{                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]   | 
                                                                  | 
  if ( KeGetCurrentThread()->PreviousMode )                       | 
  {                                                               | 
    v2 = 0x7FFFFFFF0000LL;                                        | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                | 
    {                                                             | 
      v2 = (__int64)a2;                                           | 
    }                                                             | 
    *(_DWORD *)v2 = *(_DWORD *)v2;                                | 
  }                                                               | 
  *a2 = KeQueryIntervalProfile();                                 | 
  return 0LL;                                                     | 
}                                                                 | 
16 row(s)
