__int64 __fastcall NtGdiGetTextMetricsW(HDC a1, unsigned __int64 a2, unsigned int a3) | 
{                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                       | 
                                                                                      | 
  TextMetricsW = 0;                                                                   | 
  if ( a3 >= 0x44 )                                                                   | 
  {                                                                                   | 
    memset_0(v8, 0, 0x44uLL);                                                         | 
    SessionState = W32GetSessionState();                                              | 
    EUDCCountRegion::EUDCCountRegion(                                                 | 
      (EUDCCountRegion *)v11,                                                         | 
      (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));        | 
    if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v11) )                           | 
    {                                                                                 | 
      UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v10, a1);                                     | 
      if ( v10[0] )                                                                   | 
      {                                                                               | 
        TextMetricsW = GrepGetTextMetricsW(v10, v8, 20LL);                            | 
      }                                                                               | 
      UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v10);                                        | 
    }                                                                                 | 
    if ( TextMetricsW )                                                               | 
    {                                                                                 | 
      if ( a2 + 68 > MmUserProbeAddress || a2 + 68 <= a2 || (a2 & 3) != 0 )           | 
      {                                                                               | 
        *(_BYTE *)MmUserProbeAddress = 0;                                             | 
      }                                                                               | 
      *(_OWORD *)a2 = v8[0];                                                          | 
      *(_OWORD *)(a2 + 16) = v8[1];                                                   | 
      *(_OWORD *)(a2 + 32) = v8[2];                                                   | 
      *(_OWORD *)(a2 + 48) = v8[3];                                                   | 
      *(_DWORD *)(a2 + 64) = v9;                                                      | 
    }                                                                                 | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v11);                        | 
  }                                                                                   | 
  return TextMetricsW;                                                                | 
}                                                                                     | 
37 row(s)
