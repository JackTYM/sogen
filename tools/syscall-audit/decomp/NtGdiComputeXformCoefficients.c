_BOOL8 __fastcall NtGdiComputeXformCoefficients(HDC a1)         | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  v1 = 0;                                                       | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v3, a1);                       | 
  if ( v3[0] )                                                  | 
  {                                                             | 
    DC::QuickInitXform(v3[0], &v4, 516LL);                      | 
    v1 = (*(_BYTE *)(v4 + 32) & 1) != 0;                        | 
  }                                                             | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v3);                          | 
  return v1;                                                    | 
}                                                               | 
14 row(s)
