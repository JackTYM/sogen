__int64 __fastcall NtGdiSelectBitmap(HDC a1, __int64 a2)        | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v6, a1);                       | 
  v3 = 0LL;                                                     | 
  if ( v6[0] )                                                  | 
  {                                                             | 
    v3 = *(_QWORD *)GrepSelectBitmap(v5, v6, a2, 0LL);          | 
  }                                                             | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v6);                          | 
  return v3;                                                    | 
}                                                               | 
13 row(s)
