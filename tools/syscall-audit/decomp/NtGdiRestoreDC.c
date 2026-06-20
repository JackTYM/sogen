__int64 __fastcall NtGdiRestoreDC(HDC a1, int a2)               | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v5, a1);                       | 
  if ( v5[0] && XDCOBJ::W32PidDCOwnerFast((XDCOBJ *)v5) )       | 
  {                                                             | 
    v3 = GrepRestoreDCOBJ((struct XDCOBJ *)v5, a2);             | 
    APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v5);                        | 
    return v3;                                                  | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    EngSetLastError(6u);                                        | 
    APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v5);                        | 
    return 0LL;                                                 | 
  }                                                             | 
}                                                               | 
18 row(s)
