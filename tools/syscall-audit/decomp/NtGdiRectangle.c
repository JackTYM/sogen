__int64 __fastcall NtGdiRectangle(HDC a1, int a2, int a3, int a4, int a5)                                               | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v6 = 0;                                                                                                               | 
  v9 = 0;                                                                                                               | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v13, a1);                                                                              | 
  if ( v13[0] )                                                                                                         | 
  {                                                                                                                     | 
    v15 = v13;                                                                                                          | 
    if ( *(_DWORD *)(v13[0] + 492LL)                                                                                    | 
      && (*(_DWORD *)(v13[0] + 36LL) & 0x4000) != 0                                                                     | 
      && !(unsigned int)DWMSCREENREADMODIFYWRITEASSIST::bInPathBracket((DWMSCREENREADMODIFYWRITEASSIST *)v14) )         | 
    {                                                                                                                   | 
      DWMSCREENREADMODIFYWRITEASSIST::vSaveAccumBoundsAndDisableSpriteUpdates((DWMSCREENREADMODIFYWRITEASSIST *)v14);   | 
      if ( (unsigned int)GrepRectangle((struct XDCOBJ *)v13, a2, a3, a4, a5) )                                          | 
      {                                                                                                                 | 
        v9 = DWMSCREENREADMODIFYWRITEASSIST::bReadFromAccumulatedBounds((DWMSCREENREADMODIFYWRITEASSIST *)v14);         | 
      }                                                                                                                 | 
      DWMSCREENREADMODIFYWRITEASSIST::vRestoreAccumBoundsAndEnableSpriteUpdates((DWMSCREENREADMODIFYWRITEASSIST *)v14); | 
    }                                                                                                                   | 
    v10 = GrepRectangle((struct XDCOBJ *)v13, a2, a3, a4, a5);                                                          | 
    if ( v9 )                                                                                                           | 
    {                                                                                                                   | 
      v12 = UserReferenceDwmApiPort();                                                                                  | 
      DwmSyncFlushAndWaitForBatch(v12);                                                                                 | 
    }                                                                                                                   | 
    v6 = v10;                                                                                                           | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    EngSetLastError(6u);                                                                                                | 
  }                                                                                                                     | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v13);                                                                                 | 
  return v6;                                                                                                            | 
}                                                                                                                       | 
36 row(s)
