__int64 NtUserGetQueueStatusReadonly()                              | 
{                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]     | 
                                                                    | 
  CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged(); | 
  v1 = 0LL;                                                         | 
  if ( CurrentThreadNonPaged )                                      | 
  {                                                                 | 
    v1 = *CurrentThreadNonPaged;                                    | 
  }                                                                 | 
  INTERLOCKEDINT::operator int(*(_QWORD *)(v1 + 488) + 8LL);        | 
  v3 = INTERLOCKEDINT::operator int(*(_QWORD *)(v2 + 488) + 4LL);   | 
  return (v5 | (v5 << 16)) & (v4 | (unsigned int)v3);               | 
}                                                                   | 
14 row(s)
