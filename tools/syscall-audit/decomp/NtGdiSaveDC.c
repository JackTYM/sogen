__int64 __fastcall NtGdiSaveDC(HDC a1)                          | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v3, a1);                       | 
  v1 = 0;                                                       | 
  if ( v3[0] )                                                  | 
  {                                                             | 
    v1 = GrepSaveDC((struct XDCOBJ *)v3, 0);                    | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    EngSetLastError(6u);                                        | 
  }                                                             | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v3);                          | 
  return v1;                                                    | 
}                                                               | 
17 row(s)
