__int64 __fastcall NtUserBitBltSysBmp(HDC a1, __int64 a2, __int64 a3, unsigned int a4, int a5)                               | 
{                                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                              | 
                                                                                                                             | 
  v7 = 0;                                                                                                                    | 
  EnterCrit(0LL, 0LL);                                                                                                       | 
  if ( a4 < 0x5D )                                                                                                           | 
  {                                                                                                                          | 
    v8 = PrepareHDCBITSBitmap(a1);                                                                                           | 
    if ( v8 )                                                                                                                | 
    {                                                                                                                        | 
      OemBitmapInfoForCurrentThread = (__int16 *)GetOemBitmapInfoForCurrentThread(a4);                                       | 
      if ( (W32GetCurrentThreadDpiAwarenessContext() & 0xF) != 0                                                             | 
        || (DCDpiScaleValue = GreGetDCDpiScaleValue(a1), v11 = (unsigned int)DCDpiScaleValue, DCDpiScaleValue <= 1)          | 
        || (LOWORD(v11) = 96 * DCDpiScaleValue, (OemBitmapInfoForDpi = (__int16 *)GetOemBitmapInfoForDpi(a4, v11)) == 0LL) ) | 
      {                                                                                                                      | 
        v13 = GreBitBltInternal(                                                                                             | 
                a1,                                                                                                          | 
                OemBitmapInfoForCurrentThread[3],                                                                            | 
                v8,                                                                                                          | 
                *OemBitmapInfoForCurrentThread,                                                                              | 
                OemBitmapInfoForCurrentThread[1],                                                                            | 
                a5,                                                                                                          | 
                0,                                                                                                           | 
                0);                                                                                                          | 
      }                                                                                                                      | 
      else                                                                                                                   | 
      {                                                                                                                      | 
        v13 = NtGdiStretchBlt(                                                                                               | 
                a1,                                                                                                          | 
                OemBitmapInfoForCurrentThread[3],                                                                            | 
                v8,                                                                                                          | 
                *OemBitmapInfoForDpi,                                                                                        | 
                OemBitmapInfoForDpi[1],                                                                                      | 
                OemBitmapInfoForDpi[2],                                                                                      | 
                OemBitmapInfoForDpi[3],                                                                                      | 
                a5,                                                                                                          | 
                0);                                                                                                          | 
      }                                                                                                                      | 
      v7 = v13;                                                                                                              | 
    }                                                                                                                        | 
  }                                                                                                                          | 
  else                                                                                                                       | 
  {                                                                                                                          | 
    UserSetLastError(87LL);                                                                                                  | 
  }                                                                                                                          | 
  UserSessionSwitchLeaveCrit();                                                                                              | 
  return v7;                                                                                                                 | 
}                                                                                                                            | 
49 row(s)
