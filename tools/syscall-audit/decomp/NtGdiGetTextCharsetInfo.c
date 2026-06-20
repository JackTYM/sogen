__int64 __fastcall NtGdiGetTextCharsetInfo(HDC a1, void *a2)                                                   | 
{                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                | 
                                                                                                               | 
  v11 = 0LL;                                                                                                   | 
  Src = 0LL;                                                                                                   | 
  SessionState = W32GetSessionState();                                                                         | 
  EUDCCountRegion::EUDCCountRegion(                                                                            | 
    (EUDCCountRegion *)v9,                                                                                     | 
    (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));                                   | 
  if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v9) )                                                       | 
  {                                                                                                            | 
    UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v8, a1);                                                                 | 
    if ( v8[0] )                                                                                               | 
    {                                                                                                          | 
      TextCharsetInfo = GrepGetTextCharsetInfo(                                                                | 
                          v8,                                                                                  | 
                          (unsigned __int64)&Src & ((unsigned __int128)-(__int128)(unsigned __int64)a2 >> 64), | 
                          v5,                                                                                  | 
                          14LL);                                                                               | 
    }                                                                                                          | 
    else                                                                                                       | 
    {                                                                                                          | 
      EngSetLastError(6u);                                                                                     | 
      TextCharsetInfo = 1;                                                                                     | 
    }                                                                                                          | 
    UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v8);                                                                    | 
  }                                                                                                            | 
  else                                                                                                         | 
  {                                                                                                            | 
    EngSetLastError(0x15u);                                                                                    | 
    TextCharsetInfo = 1;                                                                                       | 
  }                                                                                                            | 
  if ( TextCharsetInfo != -1 && a2 )                                                                           | 
  {                                                                                                            | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                                          | 
    {                                                                                                          | 
      a2 = (void *)MmUserProbeAddress;                                                                         | 
    }                                                                                                          | 
    RtlCopyVolatileMemory(a2, &Src, 0x18uLL);                                                                  | 
  }                                                                                                            | 
  EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v9);                                                    | 
  return TextCharsetInfo;                                                                                      | 
}                                                                                                              | 
44 row(s)
