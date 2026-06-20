__int64 __fastcall NtGdiGetOutlineTextMetricsInternalW(HDC a1, size_t Size, char *a3, unsigned __int64 a4)     | 
{                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                | 
                                                                                                               | 
  v6 = (unsigned int)Size;                                                                                     | 
  v8 = 0LL;                                                                                                    | 
  v9 = 0LL;                                                                                                    | 
  v14 = 0LL;                                                                                                   | 
  if ( (_DWORD)Size && a3 )                                                                                    | 
  {                                                                                                            | 
    if ( (unsigned int)Size <= 0x2710000 )                                                                     | 
    {                                                                                                          | 
      v8 = (void *)AllocFreeTmpBuffer((unsigned int)Size);                                                     | 
    }                                                                                                          | 
    if ( !v8 )                                                                                                 | 
    {                                                                                                          | 
      return 0LL;                                                                                              | 
    }                                                                                                          | 
    memset_0(v8, 0, v6);                                                                                       | 
  }                                                                                                            | 
  else                                                                                                         | 
  {                                                                                                            | 
    v6 = 0LL;                                                                                                  | 
  }                                                                                                            | 
  SessionState = W32GetSessionState();                                                                         | 
  EUDCCountRegion::EUDCCountRegion(                                                                            | 
    (EUDCCountRegion *)v15,                                                                                    | 
    (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));                                   | 
  if ( !EUDCCountRegion::IsReady((EUDCCountRegion *)v15) )                                                     | 
  {                                                                                                            | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v15);                                                 | 
    return 0LL;                                                                                                | 
  }                                                                                                            | 
  UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v16, a1);                                                                  | 
  if ( v16[0] )                                                                                                | 
  {                                                                                                            | 
    OutlineTextMetrics = GrepGetOutlineTextMetrics(v16, (unsigned int)v6, v8, &v14);                           | 
    v9 = v14;                                                                                                  | 
  }                                                                                                            | 
  else                                                                                                         | 
  {                                                                                                            | 
    EngSetLastError(6u);                                                                                       | 
    OutlineTextMetrics = 0;                                                                                    | 
  }                                                                                                            | 
  v13 = 0;                                                                                                     | 
  if ( OutlineTextMetrics != -1 )                                                                              | 
  {                                                                                                            | 
    v13 = OutlineTextMetrics;                                                                                  | 
  }                                                                                                            | 
  if ( v13 )                                                                                                   | 
  {                                                                                                            | 
    if ( a4 + 8 > MmUserProbeAddress || a4 + 8 <= a4 || (a4 & 3) != 0 )                                        | 
    {                                                                                                          | 
      *(_BYTE *)MmUserProbeAddress = 0;                                                                        | 
    }                                                                                                          | 
    *(_QWORD *)a4 = v9;                                                                                        | 
    if ( (_DWORD)v6 )                                                                                          | 
    {                                                                                                          | 
      if ( (unsigned __int64)&a3[v6] > MmUserProbeAddress || &a3[v6] <= a3 || ((unsigned __int8)a3 & 3) != 0 ) | 
      {                                                                                                        | 
        *(_BYTE *)MmUserProbeAddress = 0;                                                                      | 
      }                                                                                                        | 
      memmove(a3, v8, (unsigned int)v6);                                                                       | 
    }                                                                                                          | 
  }                                                                                                            | 
  if ( v8 )                                                                                                    | 
  {                                                                                                            | 
    FreeTmpBuffer(v8);                                                                                         | 
  }                                                                                                            | 
  UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v16);                                                                     | 
  EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v15);                                                   | 
  return v13;                                                                                                  | 
}                                                                                                              | 
73 row(s)
