__int64 __fastcall NtGdiOpenDCW(                                                 | 
        unsigned __int16 *a1,                                                    | 
        struct _devicemodeW *a2,                                                 | 
        __int64 a3,                                                              | 
        unsigned int a4,                                                         | 
        __int64 a5,                                                              | 
        int a6,                                                                  | 
        void *a7,                                                                | 
        struct _DRIVER_INFO_2W *a8,                                              | 
        __int64 a9)                                                              | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  v11 = 0LL;                                                                     | 
  v19 = 0LL;                                                                     | 
  v20 = 0LL;                                                                     | 
  if ( !a1 )                                                                     | 
  {                                                                              | 
    return UserGetDesktopDC(a4, 0LL, 1LL);                                       | 
  }                                                                              | 
  v12 = *a1;                                                                     | 
  v13 = (char *)*((_QWORD *)a1 + 1);                                             | 
  if ( *a1 )                                                                     | 
  {                                                                              | 
    v14 = v12;                                                                   | 
    v15 = (void *)AllocFreeTmpBuffer(v12 + 2);                                   | 
    if ( v15 )                                                                   | 
    {                                                                            | 
      if ( &v13[v14] < v13 || (unsigned __int64)&v13[v14] > MmUserProbeAddress ) | 
      {                                                                          | 
        *(_BYTE *)MmUserProbeAddress = 0;                                        | 
      }                                                                          | 
      memmove(v15, v13, v14);                                                    | 
      *((_WORD *)v15 + (v14 >> 1)) = 0;                                          | 
    }                                                                            | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    v15 = 0LL;                                                                   | 
  }                                                                              | 
  if ( !a2 || (v19 = CaptureDEVMODEW(a2)) != 0LL )                               | 
  {                                                                              | 
    v16 = 1;                                                                     | 
    v17 = 1;                                                                     | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    v17 = 0;                                                                     | 
    v16 = 1;                                                                     | 
  }                                                                              | 
  if ( !v17 || a8 && (v20 = CaptureDriverInfo2W(a8)) == 0LL )                    | 
  {                                                                              | 
    v16 = 0;                                                                     | 
  }                                                                              | 
  if ( v16 )                                                                     | 
  {                                                                              | 
    v11 = hdcOpenDCW((PCWSTR)v15, a7, (HDC)v20, a9, a6);                         | 
  }                                                                              | 
  if ( v15 )                                                                     | 
  {                                                                              | 
    FreeTmpBuffer(v15);                                                          | 
  }                                                                              | 
  if ( v19 )                                                                     | 
  {                                                                              | 
    FreeThreadBufferWithTag(v19);                                                | 
  }                                                                              | 
  vFreeDriverInfo2(v20);                                                         | 
  return v11;                                                                    | 
}                                                                                | 
69 row(s)
