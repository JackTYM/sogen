__int64 __fastcall NtGdiGetGlyphOutline(                                            | 
        HDC a1,                                                                     | 
        unsigned __int16 a2,                                                        | 
        unsigned int a3,                                                            | 
        void *a4,                                                                   | 
        size_t Size,                                                                | 
        char *a6,                                                                   | 
        const void *a7,                                                             | 
        int a8)                                                                     | 
{                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                     | 
                                                                                    | 
  v9 = a7;                                                                          | 
  GlyphOutline = -1;                                                                | 
  v23 = 0LL;                                                                        | 
  Src = 0LL;                                                                        | 
  v22 = 0;                                                                          | 
  if ( (unsigned int)(Size - 1) > 0x270FFFF )                                       | 
  {                                                                                 | 
    v11 = 0LL;                                                                      | 
  }                                                                                 | 
  else                                                                              | 
  {                                                                                 | 
    v11 = (void *)AllocFreeTmpBuffer((unsigned int)Size);                           | 
    v9 = a7;                                                                        | 
  }                                                                                 | 
  if ( v11 || !(_DWORD)Size )                                                       | 
  {                                                                                 | 
    if ( (unsigned __int64)v9 >= MmUserProbeAddress )                               | 
    {                                                                               | 
      v9 = (const void *)MmUserProbeAddress;                                        | 
    }                                                                               | 
    RtlCopyVolatileMemory(&v23, v9, 0x10uLL);                                       | 
    if ( v11 )                                                                      | 
    {                                                                               | 
      memset_0(v11, 0, (unsigned int)Size);                                         | 
    }                                                                               | 
    SessionState = W32GetSessionState();                                            | 
    EUDCCountRegion::EUDCCountRegion(                                               | 
      (EUDCCountRegion *)v20,                                                       | 
      (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));      | 
    if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v20) )                         | 
    {                                                                               | 
      UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v19, a1);                                   | 
      if ( v19[0] )                                                                 | 
      {                                                                             | 
        GlyphOutline = GrepGetGlyphOutline(v19, a2, a3, &Src, Size, v11, &v23, a8); | 
      }                                                                             | 
      else                                                                          | 
      {                                                                             | 
        GlyphOutline = -1;                                                          | 
      }                                                                             | 
      UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v19);                                      | 
    }                                                                               | 
    else                                                                            | 
    {                                                                               | 
      GlyphOutline = -1;                                                            | 
    }                                                                               | 
    if ( GlyphOutline != -1 )                                                       | 
    {                                                                               | 
      if ( v11 )                                                                    | 
      {                                                                             | 
        v13 = &a6[(unsigned int)Size];                                              | 
        v14 = (void **)MmUserProbeAddress;                                          | 
        if ( (unsigned __int64)v13 > MmUserProbeAddress || v13 <= a6 )              | 
        {                                                                           | 
          *(_BYTE *)MmUserProbeAddress = 0;                                         | 
        }                                                                           | 
        memmove(a6, v11, (unsigned int)Size);                                       | 
      }                                                                             | 
      else                                                                          | 
      {                                                                             | 
        v14 = (void **)MmUserProbeAddress;                                          | 
      }                                                                             | 
      if ( a4 >= *v14 )                                                             | 
      {                                                                             | 
        a4 = *v14;                                                                  | 
      }                                                                             | 
      RtlCopyVolatileMemory(a4, &Src, 0x14uLL);                                     | 
    }                                                                               | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v20);                      | 
    if ( v11 )                                                                      | 
    {                                                                               | 
      FreeTmpBuffer(v11);                                                           | 
    }                                                                               | 
  }                                                                                 | 
  return GlyphOutline;                                                              | 
}                                                                                   | 
88 row(s)
