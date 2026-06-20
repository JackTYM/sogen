__int64 __fastcall NtGdiGetDCforBitmap(HSURF a1)                                    | 
{                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                     | 
                                                                                    | 
  v1 = 0LL;                                                                         | 
  SURFREF::SURFREF((SURFREF *)v3, a1);                                              | 
  if ( v4 )                                                                         | 
  {                                                                                 | 
    v1 = *(_QWORD *)(v4 + 160);                                                     | 
  }                                                                                 | 
  SFMALTLOGICALSURFACEREF::~SFMALTLOGICALSURFACEREF((SFMALTLOGICALSURFACEREF *)v3); | 
  return v1;                                                                        | 
}                                                                                   | 
13 row(s)
