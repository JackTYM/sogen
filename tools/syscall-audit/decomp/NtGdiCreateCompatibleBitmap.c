HBITMAP __fastcall NtGdiCreateCompatibleBitmap(HDC a1, int a2, int a3)                                       | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  OPTAPIDCOBJ::OPTAPIDCOBJ((OPTAPIDCOBJ *)v7, a1);                                                           | 
  CompatibleBitmap = GrepCreateCompatibleBitmap((struct OPTAPIDCOBJ *)v7, a2, a3 & 0xF0FFFFFF, 0, 0LL, 0LL); | 
  OPTAPIDCOBJ::~OPTAPIDCOBJ((OPTAPIDCOBJ *)v7);                                                              | 
  return CompatibleBitmap;                                                                                   | 
}                                                                                                            | 
9 row(s)
