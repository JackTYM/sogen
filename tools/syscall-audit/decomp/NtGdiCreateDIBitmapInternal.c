__int64 __fastcall NtGdiCreateDIBitmapInternal(                                                            | 
        HDC a1,                                                                                            | 
        int a2,                                                                                            | 
        int a3,                                                                                            | 
        char a4,                                                                                           | 
        __int64 a5,                                                                                        | 
        char *Src,                                                                                         | 
        unsigned int a7,                                                                                   | 
        size_t Size,                                                                                       | 
        unsigned int a9,                                                                                   | 
        int a10,                                                                                           | 
        __int64 a11)                                                                                       | 
{                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                            | 
                                                                                                           | 
  v13 = 0LL;                                                                                               | 
  v14 = 1LL;                                                                                               | 
  memset(v25, 0, sizeof(v25));                                                                             | 
  v26 = 0;                                                                                                 | 
  v21 = 0LL;                                                                                               | 
  v22 = 0LL;                                                                                               | 
  v23 = 0LL;                                                                                               | 
  v24 = 0;                                                                                                 | 
  if ( !Src )                                                                                              | 
  {                                                                                                        | 
    v15 = Size;                                                                                            | 
    goto LABEL_19;                                                                                         | 
  }                                                                                                        | 
  v15 = Size;                                                                                              | 
  if ( !(_DWORD)Size )                                                                                     | 
  {                                                                                                        | 
    goto LABEL_19;                                                                                         | 
  }                                                                                                        | 
  if ( (unsigned int)Size < 4 )                                                                            | 
  {                                                                                                        | 
    goto LABEL_14;                                                                                         | 
  }                                                                                                        | 
  if ( (unsigned int)Size > 0x2710000 )                                                                    | 
  {                                                                                                        | 
    goto LABEL_14;                                                                                         | 
  }                                                                                                        | 
  v13 = (struct tagBITMAPINFO *)AllocThreadBufferWithTag((unsigned int)Size, 1886221383LL, 0LL);           | 
  if ( !v13 )                                                                                              | 
  {                                                                                                        | 
    goto LABEL_14;                                                                                         | 
  }                                                                                                        | 
  if ( &Src[(unsigned int)Size] < Src || (unsigned __int64)&Src[(unsigned int)Size] > MmUserProbeAddress ) | 
  {                                                                                                        | 
    *(_BYTE *)MmUserProbeAddress = 0;                                                                      | 
  }                                                                                                        | 
  memmove(v13, Src, (unsigned int)Size);                                                                   | 
  biSize = v13->bmiHeader.biSize;                                                                          | 
  if ( v13->bmiHeader.biSize < 0x28                                                                        | 
    || (unsigned int)Size < biSize                                                                         | 
    || (_DWORD)Size != GreGetBitmapSizeInternal(v13, a7, biSize) )                                         | 
  {                                                                                                        | 
    FreeThreadBufferWithTag(v13);                                                                          | 
    v13 = 0LL;                                                                                             | 
LABEL_14:                                                                                                  | 
    v17 = 0;                                                                                               | 
    goto LABEL_15;                                                                                         | 
  }                                                                                                        | 
  v17 = 1;                                                                                                 | 
LABEL_15:                                                                                                  | 
  v14 = -(__int64)(v17 != 0) & 1;                                                                          | 
  if ( v14 && a5 )                                                                                         | 
  {                                                                                                        | 
    inited = umptr_r<unsigned char>::init_probe(v25, a5, a9, 4LL);                                         | 
    v21 = *(_QWORD *)inited;                                                                               | 
    v22 = *(_QWORD *)(inited + 8);                                                                         | 
    v23 = *(_QWORD *)(inited + 16);                                                                        | 
    v24 = *(_WORD *)(inited + 24);                                                                         | 
    v14 &= -(__int64)(v21 != 0);                                                                           | 
  }                                                                                                        | 
LABEL_19:                                                                                                  | 
  if ( v14 == 1 )                                                                                          | 
  {                                                                                                        | 
    OPTAPIDCOBJ::OPTAPIDCOBJ((OPTAPIDCOBJ *)v27, a1);                                                      | 
    if ( (a4 & 2) != 0 )                                                                                   | 
    {                                                                                                      | 
      DIBitmapReal = GrepCreateDIBitmapReal((OPTAPIDCOBJ *)v27, a7, v15, 0LL, 0, 0LL, 0, 0LL, 0LL);        | 
    }                                                                                                      | 
    else                                                                                                   | 
    {                                                                                                      | 
      DIBitmapReal = GrepCreateDIBitmapComp(                                                               | 
                       (struct OPTAPIDCOBJ *)v27,                                                          | 
                       a2,                                                                                 | 
                       a3,                                                                                 | 
                       (__int64)&v21,                                                                      | 
                       (__int64)v13,                                                                       | 
                       a7,                                                                                 | 
                       v15,                                                                                | 
                       a11);                                                                               | 
    }                                                                                                      | 
    v14 = DIBitmapReal;                                                                                    | 
    OPTAPIDCOBJ::~OPTAPIDCOBJ((OPTAPIDCOBJ *)v27);                                                         | 
  }                                                                                                        | 
  if ( v13 )                                                                                               | 
  {                                                                                                        | 
    FreeThreadBufferWithTag(v13);                                                                          | 
  }                                                                                                        | 
  return v14;                                                                                              | 
}                                                                                                          | 
103 row(s)
