__int64 __fastcall NtGdiRemoveFontMemResourceEx(void *a1)       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  if ( a1 )                                                     | 
  {                                                             | 
    return GrepRemoveFontMemResource(a1);                       | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    return 0LL;                                                 | 
  }                                                             | 
}                                                               | 
13 row(s)
