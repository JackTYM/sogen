__int64 __fastcall NtGdiQueryFontAssocInfo(HDC a1)                                                     | 
{                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                        | 
                                                                                                       | 
  v2 = 0;                                                                                              | 
  v3 = *(_QWORD *)(W32GetSessionState() + 96);                                                         | 
  if ( !a1 )                                                                                           | 
  {                                                                                                    | 
    return *(unsigned int *)(v3 + 18944);                                                              | 
  }                                                                                                    | 
  UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v8, a1);                                                           | 
  if ( !v8[0] )                                                                                        | 
  {                                                                                                    | 
    EngSetLastError(6u);                                                                               | 
    DCOBJ::~DCOBJ((DCOBJ *)v8);                                                                        | 
    return 0LL;                                                                                        | 
  }                                                                                                    | 
  SessionState = W32GetSessionState();                                                                 | 
  EUDCCountRegion::EUDCCountRegion(                                                                    | 
    (EUDCCountRegion *)v9,                                                                             | 
    (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));                           | 
  if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v9) )                                               | 
  {                                                                                                    | 
    v11 = 4;                                                                                           | 
    v12 = *(unsigned __int16 *)(v8[0] + 12LL);                                                         | 
    v10 = 0LL;                                                                                         | 
    RFONTOBJ::vInit((RFONTOBJ *)&v10, (struct UDCOBJ *)v8, 0, 2u, (const struct RFONTOBJ::Tag *)&v11); | 
    if ( !v10 )                                                                                        | 
    {                                                                                                  | 
      goto LABEL_22;                                                                                   | 
    }                                                                                                  | 
    v6 = *(_BYTE *)(*(_QWORD *)(*(_QWORD *)(v10 + 120) + 32LL) + 44LL);                                | 
    if ( ((v6 + 0x80) & 0xF6) == 0 && v6 != -119 || v6 == -122 )                                       | 
    {                                                                                                  | 
      RFONTOBJ::~RFONTOBJ((RFONTOBJ *)&v10);                                                           | 
      v2 = 2;                                                                                          | 
      goto LABEL_23;                                                                                   | 
    }                                                                                                  | 
    if ( v6 )                                                                                          | 
    {                                                                                                  | 
      if ( v6 == -1 )                                                                                  | 
      {                                                                                                | 
        if ( (*(_DWORD *)(v3 + 18944) & 1) == 0 )                                                      | 
        {                                                                                              | 
          goto LABEL_22;                                                                               | 
        }                                                                                              | 
        goto LABEL_15;                                                                                 | 
      }                                                                                                | 
    }                                                                                                  | 
    else if ( (*(_DWORD *)(v3 + 18944) & 2) != 0 )                                                     | 
    {                                                                                                  | 
LABEL_15:                                                                                              | 
      LFONTOBJ::LFONTOBJ((LFONTOBJ *)v7, *(struct HLFONT__ **)(v8[0] + 1744LL), 0LL);                  | 
      if ( v7[0] && (*(_BYTE *)(v7[0] + 277LL) & 0x40) == 0 )                                          | 
      {                                                                                                | 
        LFONTOBJ::~LFONTOBJ((LFONTOBJ *)v7);                                                           | 
        RFONTOBJ::~RFONTOBJ((RFONTOBJ *)&v10);                                                         | 
        v2 = 1;                                                                                        | 
        goto LABEL_23;                                                                                 | 
      }                                                                                                | 
      LFONTOBJ::~LFONTOBJ((LFONTOBJ *)v7);                                                             | 
LABEL_22:                                                                                              | 
      RFONTOBJ::~RFONTOBJ((RFONTOBJ *)&v10);                                                           | 
      goto LABEL_23;                                                                                   | 
    }                                                                                                  | 
    if ( v6 != 2 || (*(_DWORD *)(v3 + 18944) & 4) == 0 )                                               | 
    {                                                                                                  | 
      goto LABEL_22;                                                                                   | 
    }                                                                                                  | 
    goto LABEL_15;                                                                                     | 
  }                                                                                                    | 
LABEL_23:                                                                                              | 
  EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v9);                                            | 
  DCOBJ::~DCOBJ((DCOBJ *)v8);                                                                          | 
  return v2;                                                                                           | 
}                                                                                                      | 
76 row(s)
