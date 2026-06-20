__int64 __fastcall NtGdiBitBlt(                                           | 
        HDC a1,                                                           | 
        int a2,                                                           | 
        int a3,                                                           | 
        int a4,                                                           | 
        int a5,                                                           | 
        HDC a6,                                                           | 
        int a7,                                                           | 
        int a8,                                                           | 
        unsigned int a9,                                                  | 
        unsigned int a10,                                                 | 
        int a11)                                                          | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v16, a1);                                | 
  OPTAPIDCOBJ::OPTAPIDCOBJ((OPTAPIDCOBJ *)v17, a6, (struct XDCOBJ *)v16); | 
  v14 = 0;                                                                | 
  if ( v16[0] )                                                           | 
  {                                                                       | 
    v14 = GrepBitBltImpl(                                                 | 
            (struct XDCOBJ *)v16,                                         | 
            a2,                                                           | 
            a3,                                                           | 
            a4,                                                           | 
            a5,                                                           | 
            (struct OPTAPIDCOBJ *)v17,                                    | 
            a7,                                                           | 
            a8,                                                           | 
            a9,                                                           | 
            a10,                                                          | 
            a11 & 0xFFFFFFFD);                                            | 
  }                                                                       | 
  else                                                                    | 
  {                                                                       | 
    EngSetLastError(6u);                                                  | 
  }                                                                       | 
  OPTAPIDCOBJ::~OPTAPIDCOBJ((OPTAPIDCOBJ *)v17);                          | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v16);                                   | 
  return v14;                                                             | 
}                                                                         | 
41 row(s)
