__int64 __fastcall NtGdiLineTo(HDC a1, int a2, int a3)                                                                  | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v4 = 0;                                                                                                               | 
  v6 = 0;                                                                                                               | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v10, a1);                                                                              | 
  if ( v10[0] )                                                                                                         | 
  {                                                                                                                     | 
    v12 = v10;                                                                                                          | 
    if ( *(_DWORD *)(v10[0] + 492LL)                                                                                    | 
      && (*(_DWORD *)(v10[0] + 36LL) & 0x4000) != 0                                                                     | 
      && !(unsigned int)DWMSCREENREADMODIFYWRITEASSIST::bInPathBracket((DWMSCREENREADMODIFYWRITEASSIST *)v11) )         | 
    {                                                                                                                   | 
      DWMSCREENREADMODIFYWRITEASSIST::vSaveAccumBoundsAndDisableSpriteUpdates((DWMSCREENREADMODIFYWRITEASSIST *)v11);   | 
      if ( (unsigned int)GrepLineTo((struct XDCOBJ *)v10, a2, a3) )                                                     | 
      {                                                                                                                 | 
        v6 = DWMSCREENREADMODIFYWRITEASSIST::bReadFromAccumulatedBounds((DWMSCREENREADMODIFYWRITEASSIST *)v11);         | 
      }                                                                                                                 | 
      DWMSCREENREADMODIFYWRITEASSIST::vRestoreAccumBoundsAndEnableSpriteUpdates((DWMSCREENREADMODIFYWRITEASSIST *)v11); | 
    }                                                                                                                   | 
    v7 = GrepLineTo((struct XDCOBJ *)v10, a2, a3);                                                                      | 
    if ( v6 )                                                                                                           | 
    {                                                                                                                   | 
      v9 = UserReferenceDwmApiPort();                                                                                   | 
      DwmSyncFlushAndWaitForBatch(v9);                                                                                  | 
    }                                                                                                                   | 
    v4 = v7;                                                                                                            | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    EngSetLastError(6u);                                                                                                | 
  }                                                                                                                     | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v10);                                                                                 | 
  return v4;                                                                                                            | 
}                                                                                                                       | 
36 row(s)
