__int64 __fastcall NtGdiCreatePen(unsigned int a1, int a2, int a3, __int64 a4) | 
{                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                | 
                                                                               | 
  v4 = 0LL;                                                                    | 
  if ( a1 <= 4 || a1 - 5 <= 1 )                                                | 
  {                                                                            | 
    return GreExtCreatePen(a1, a2, 0, a3, 0LL, 0LL, 0, 0LL, 0, 1, a4);         | 
  }                                                                            | 
  EngSetLastError(0x57u);                                                      | 
  return v4;                                                                   | 
}                                                                              | 
12 row(s)
