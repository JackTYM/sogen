__int64 __fastcall NtGdiStretchDIBitsInternal(                                                                   | 
        HDC a1,                                                                                                  | 
        __int64 a2,                                                                                              | 
        __int64 a3,                                                                                              | 
        __int64 a4,                                                                                              | 
        int a5,                                                                                                  | 
        int a6,                                                                                                  | 
        int a7,                                                                                                  | 
        int a8,                                                                                                  | 
        int a9,                                                                                                  | 
        __int64 a10,                                                                                             | 
        char *Src,                                                                                               | 
        unsigned int a12,                                                                                        | 
        int a13,                                                                                                 | 
        size_t Size,                                                                                             | 
        unsigned int a15,                                                                                        | 
        __int64 a16)                                                                                             | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v29 = a1;                                                                                                      | 
  v16 = 0LL;                                                                                                     | 
  v17 = 1;                                                                                                       | 
  memset(v27, 0, sizeof(v27));                                                                                   | 
  v28 = 0;                                                                                                       | 
  v23 = 0LL;                                                                                                     | 
  v24 = 0LL;                                                                                                     | 
  v25 = 0LL;                                                                                                     | 
  v26 = 0;                                                                                                       | 
  if ( !a10 || !Src )                                                                                            | 
  {                                                                                                              | 
    v18 = Size;                                                                                                  | 
    goto LABEL_21;                                                                                               | 
  }                                                                                                              | 
  v18 = Size;                                                                                                    | 
  if ( !(_DWORD)Size )                                                                                           | 
  {                                                                                                              | 
    goto LABEL_21;                                                                                               | 
  }                                                                                                              | 
  if ( (unsigned int)Size >= 4 && (unsigned int)Size <= 0x2710000 )                                              | 
  {                                                                                                              | 
    v16 = (struct tagBITMAPINFO *)AllocThreadBufferWithTag((unsigned int)Size, 1886221383LL, 0LL);               | 
    if ( v16 )                                                                                                   | 
    {                                                                                                            | 
      if ( &Src[(unsigned int)Size] < Src || (unsigned __int64)&Src[(unsigned int)Size] > MmUserProbeAddress )   | 
      {                                                                                                          | 
        *(_BYTE *)MmUserProbeAddress = 0;                                                                        | 
      }                                                                                                          | 
      memmove(v16, Src, (unsigned int)Size);                                                                     | 
      biSize = v16->bmiHeader.biSize;                                                                            | 
      if ( v16->bmiHeader.biSize >= 0x28                                                                         | 
        && (unsigned int)Size >= biSize                                                                          | 
        && (_DWORD)Size == GreGetBitmapSizeInternal(v16, a12, biSize) )                                          | 
      {                                                                                                          | 
        v20 = 1;                                                                                                 | 
        goto LABEL_16;                                                                                           | 
      }                                                                                                          | 
      FreeThreadBufferWithTag(v16);                                                                              | 
      v16 = 0LL;                                                                                                 | 
    }                                                                                                            | 
  }                                                                                                              | 
  v20 = 0;                                                                                                       | 
LABEL_16:                                                                                                        | 
  v17 = v20 != 0;                                                                                                | 
  if ( v17 )                                                                                                     | 
  {                                                                                                              | 
    inited = umptr_r<unsigned char>::init_probe(v27, a10, a15, 4LL);                                             | 
    v23 = *(_QWORD *)inited;                                                                                     | 
    v24 = *(_QWORD *)(inited + 8);                                                                               | 
    v25 = *(_QWORD *)(inited + 16);                                                                              | 
    v26 = *(_WORD *)(inited + 24);                                                                               | 
    a1 = v29;                                                                                                    | 
    if ( !v23 )                                                                                                  | 
    {                                                                                                            | 
      v17 = 0;                                                                                                   | 
    }                                                                                                            | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    a1 = v29;                                                                                                    | 
  }                                                                                                              | 
LABEL_21:                                                                                                        | 
  if ( v17 )                                                                                                     | 
  {                                                                                                              | 
    v17 = GreStretchDIBitsInternalImpl(a1, a5, a6, a7, a8, a9, (__int64)&v23, (__int64)v16, a12, a13, v18, a16); | 
  }                                                                                                              | 
  if ( v16 )                                                                                                     | 
  {                                                                                                              | 
    FreeThreadBufferWithTag(v16);                                                                                | 
  }                                                                                                              | 
  return v17;                                                                                                    | 
}                                                                                                                | 
92 row(s)
