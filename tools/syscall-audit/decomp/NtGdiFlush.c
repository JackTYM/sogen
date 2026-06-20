__int64 NtGdiFlush()                                                                     | 
{                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                          | 
                                                                                         | 
  v0 = *(int (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 24) + 2280LL);      | 
  if ( v0 )                                                                              | 
  {                                                                                      | 
    if ( v0() >= 0 )                                                                     | 
    {                                                                                    | 
      v1 = *(void (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 24) + 2288LL); | 
      if ( v1 )                                                                          | 
      {                                                                                  | 
        v1();                                                                            | 
      }                                                                                  | 
    }                                                                                    | 
  }                                                                                      | 
  return 0LL;                                                                            | 
}                                                                                        | 
18 row(s)
