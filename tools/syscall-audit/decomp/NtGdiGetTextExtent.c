__int64 __fastcall NtGdiGetTextExtent(HDC a1, char *Src, int a3, void *a4, int a5)            | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  v5 = a4;                                                                                    | 
  v6 = a3;                                                                                    | 
  v8 = a1;                                                                                    | 
  v18 = a1;                                                                                   | 
  v19 = a1;                                                                                   | 
  v20 = a4;                                                                                   | 
  Srca = 0LL;                                                                                 | 
  v10 = 0LL;                                                                                  | 
  TextExtentW = 0;                                                                            | 
  v15 = 0;                                                                                    | 
  if ( a3 < 0 )                                                                               | 
  {                                                                                           | 
    return TextExtentW;                                                                       | 
  }                                                                                           | 
  if ( !a3 )                                                                                  | 
  {                                                                                           | 
    TextExtentW = 1;                                                                          | 
LABEL_27:                                                                                     | 
    if ( (unsigned __int64)v5 >= MmUserProbeAddress )                                         | 
    {                                                                                         | 
      v5 = (void *)MmUserProbeAddress;                                                        | 
    }                                                                                         | 
    RtlCopyVolatileMemory(v5, &Srca, 8uLL);                                                   | 
    return TextExtentW;                                                                       | 
  }                                                                                           | 
  if ( Src )                                                                                  | 
  {                                                                                           | 
    if ( a3 > 16 )                                                                            | 
    {                                                                                         | 
      if ( (unsigned __int64)a3 <= 0x1388000 )                                                | 
      {                                                                                       | 
        v10 = (char *)AllocFreeTmpBuffer((unsigned int)(2 * a3));                             | 
      }                                                                                       | 
    }                                                                                         | 
    else                                                                                      | 
    {                                                                                         | 
      v10 = &v23;                                                                             | 
    }                                                                                         | 
    if ( v10 )                                                                                | 
    {                                                                                         | 
      v12 = &Src[2 * v6];                                                                     | 
      if ( v12 < Src || (unsigned __int64)v12 > MmUserProbeAddress )                          | 
      {                                                                                       | 
        *(_BYTE *)MmUserProbeAddress = 0;                                                     | 
      }                                                                                       | 
      memmove(v10, Src, 2 * v6);                                                              | 
      TextExtentW = 1;                                                                        | 
      v15 = 1;                                                                                | 
    }                                                                                         | 
    v8 = v18;                                                                                 | 
  }                                                                                           | 
  if ( TextExtentW )                                                                          | 
  {                                                                                           | 
    SessionState = W32GetSessionState();                                                      | 
    EUDCCountRegion::EUDCCountRegion(                                                         | 
      (EUDCCountRegion *)v22,                                                                 | 
      (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));                | 
    if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v22) )                                   | 
    {                                                                                         | 
      UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v21, v8);                                             | 
      if ( v21[0] )                                                                           | 
      {                                                                                       | 
        TextExtentW = GrepGetTextExtentW(v21, v10, (unsigned int)a3, &Srca, a5, 10, v15, a3); | 
      }                                                                                       | 
      else                                                                                    | 
      {                                                                                       | 
        TextExtentW = 0;                                                                      | 
      }                                                                                       | 
      UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v21);                                                | 
    }                                                                                         | 
    else                                                                                      | 
    {                                                                                         | 
      TextExtentW = 0;                                                                        | 
    }                                                                                         | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v22);                                | 
  }                                                                                           | 
  if ( (int)v6 > 16 && v10 )                                                                  | 
  {                                                                                           | 
    FreeTmpBuffer(v10);                                                                       | 
  }                                                                                           | 
  if ( TextExtentW )                                                                          | 
  {                                                                                           | 
    goto LABEL_27;                                                                            | 
  }                                                                                           | 
  return TextExtentW;                                                                         | 
}                                                                                             | 
90 row(s)
