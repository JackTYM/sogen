__int64 __fastcall NtGdiExtSelectClipRgn(HDC a1, HRGN a2, int a3)                | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v9, a1);                                        | 
  v5 = 0;                                                                        | 
  if ( !v9[0] )                                                                  | 
  {                                                                              | 
    EngSetLastError(6u);                                                         | 
    goto LABEL_12;                                                               | 
  }                                                                              | 
  v6 = 0;                                                                        | 
  if ( (unsigned int)(a3 - 1) > 4 )                                              | 
  {                                                                              | 
    v8 = 87;                                                                     | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    if ( (*(_DWORD *)(HmgPentryFromPobj() + 8) & 0xFFFFFFFE) != 0 )              | 
    {                                                                            | 
      if ( a2 )                                                                  | 
      {                                                                          | 
        RGNOBJAPI::RGNOBJAPI((RGNOBJAPI *)v18, a2, 1, 0);                        | 
        if ( v18[0] )                                                            | 
        {                                                                        | 
          v6 = DC::iSelect(v9[0], v18[0], a3);                                   | 
          if ( v6 )                                                              | 
          {                                                                      | 
            DLODCOBJ::DLODCOBJ((DLODCOBJ *)v13);                                 | 
            v11 = 0LL;                                                           | 
            v10 = 0LL;                                                           | 
            v12 = 0;                                                             | 
            v13[0] = 0LL;                                                        | 
            v6 = 0;                                                              | 
            memset(&v13[10], 0, 24);                                             | 
            if ( DEVLOCKOBJ::bLock((DEVLOCKOBJ *)&v10, (struct XDCOBJ *)v9, 1) ) | 
            {                                                                    | 
              v19 = XDCOBJ::prgnEffRao((XDCOBJ *)v9);                            | 
              v6 = RGNOBJ::iComplexity((RGNOBJ *)&v19);                          | 
            }                                                                    | 
            DEVLOCKOBJ::~DEVLOCKOBJ((DEVLOCKOBJ *)&v10);                         | 
          }                                                                      | 
        }                                                                        | 
        RGNOBJAPI::~RGNOBJAPI((RGNOBJAPI *)v18);                                 | 
      }                                                                          | 
      else if ( a3 == 5 )                                                        | 
      {                                                                          | 
        v6 = DC::iSelect(v9[0], 0LL, 5);                                         | 
        if ( v6 )                                                                | 
        {                                                                        | 
          DLODCOBJ::DLODCOBJ((DLODCOBJ *)v17);                                   | 
          v15 = 0LL;                                                             | 
          v14 = 0LL;                                                             | 
          v16 = 0;                                                               | 
          v17[0] = 0LL;                                                          | 
          v6 = 0;                                                                | 
          memset(&v17[10], 0, 24);                                               | 
          if ( DEVLOCKOBJ::bLock((DEVLOCKOBJ *)&v14, (struct XDCOBJ *)v9, 1) )   | 
          {                                                                      | 
            v19 = DC::prgnVisSnap(v9[0]);                                        | 
            v6 = RGNOBJ::iComplexity((RGNOBJ *)&v19);                            | 
          }                                                                      | 
          DEVLOCKOBJ::~DEVLOCKOBJ((DEVLOCKOBJ *)&v14);                           | 
        }                                                                        | 
      }                                                                          | 
      goto LABEL_11;                                                             | 
    }                                                                            | 
    v8 = 6;                                                                      | 
  }                                                                              | 
  EngSetLastError(v8);                                                           | 
LABEL_11:                                                                        | 
  v5 = v6;                                                                       | 
LABEL_12:                                                                        | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v9);                                           | 
  return v5;                                                                     | 
}                                                                                | 
76 row(s)
