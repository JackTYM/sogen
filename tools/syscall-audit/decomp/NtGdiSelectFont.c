HFONT __fastcall NtGdiSelectFont(HDC a1, HFONT a2)              | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v5, a1);                       | 
  v3 = 0LL;                                                     | 
  if ( v5[0] )                                                  | 
  {                                                             | 
    v3 = GrepSelectFont((struct XDCOBJ *)v5, a2, 0);            | 
  }                                                             | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v5);                          | 
  return v3;                                                    | 
}                                                               | 
13 row(s)
