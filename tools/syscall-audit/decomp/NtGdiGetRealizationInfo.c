__int64 __fastcall NtGdiGetRealizationInfo(HDC a1, unsigned int *a2)         | 
{                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]              | 
                                                                             | 
  memset_0(Length, 0, 0x32CuLL);                                             | 
  v4 = a2;                                                                   | 
  if ( (unsigned __int64)a2 >= MmUserProbeAddress )                          | 
  {                                                                          | 
    v4 = (unsigned int *)MmUserProbeAddress;                                 | 
  }                                                                          | 
  Length[0] = *v4;                                                           | 
  v5 = Length[0];                                                            | 
  if ( Length[0] != 16 && Length[0] != 24 && Length[0] != 812 )              | 
  {                                                                          | 
    return 0LL;                                                              | 
  }                                                                          | 
  SessionState = W32GetSessionState();                                       | 
  EUDCCountRegion::EUDCCountRegion(                                          | 
    (EUDCCountRegion *)v10,                                                  | 
    (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL)); | 
  if ( !EUDCCountRegion::IsReady((EUDCCountRegion *)v10) )                   | 
  {                                                                          | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v10);               | 
    return 0LL;                                                              | 
  }                                                                          | 
  UAPIDCOBJ::UAPIDCOBJ((UAPIDCOBJ *)v11, a1);                                | 
  if ( v11[0] )                                                              | 
  {                                                                          | 
    v8 = *(_DWORD *)GrepGetRealizationInfo(v9, v11, Length);                 | 
    v5 = Length[0];                                                          | 
  }                                                                          | 
  else                                                                       | 
  {                                                                          | 
    v8 = 0;                                                                  | 
  }                                                                          | 
  if ( v8 )                                                                  | 
  {                                                                          | 
    ProbeForWrite(a2, v5, 4u);                                               | 
    memmove(a2, Length, v5);                                                 | 
  }                                                                          | 
  UAPIDCOBJ::~UAPIDCOBJ((UAPIDCOBJ *)v11);                                   | 
  EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v10);                 | 
  return v8;                                                                 | 
}                                                                            | 
44 row(s)
