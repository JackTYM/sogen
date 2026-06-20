__int64 __fastcall NtGdiGetDIBitsInternal(                                                                    | 
        HDC a1,                                                                                               | 
        HSURF a2,                                                                                             | 
        __int64 a3,                                                                                           | 
        int a4,                                                                                               | 
        __int64 a5,                                                                                           | 
        unsigned int *Address,                                                                                | 
        unsigned int a7,                                                                                      | 
        unsigned int a8)                                                                                      | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  v34 = a2;                                                                                                   | 
  v35 = a2;                                                                                                   | 
  v36 = Address;                                                                                              | 
  BitmapBitsSize = a8;                                                                                        | 
  v28 = 1;                                                                                                    | 
  memset(v37, 0, sizeof(v37));                                                                                | 
  v38 = 0;                                                                                                    | 
  v29 = 0LL;                                                                                                  | 
  v30 = 0LL;                                                                                                  | 
  v31 = 0LL;                                                                                                  | 
  v32 = 0;                                                                                                    | 
  memset(v40, 0, sizeof(v40));                                                                                | 
  v41 = 0LL;                                                                                                  | 
  v8 = 2;                                                                                                     | 
  if ( a7 > 2 || !Address || !a2 )                                                                            | 
  {                                                                                                           | 
    return 0LL;                                                                                               | 
  }                                                                                                           | 
  v9 = a5 & -(__int64)(a4 != 0);                                                                              | 
  v33 = v9;                                                                                                   | 
  v10 = *Address;                                                                                             | 
  ProbeForWrite(Address, *Address, 1u);                                                                       | 
  if ( v9 )                                                                                                   | 
  {                                                                                                           | 
    v11 = 0;                                                                                                  | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    if ( v10 == 12 && !*((_WORD *)Address + 5) )                                                              | 
    {                                                                                                         | 
      v11 = 12;                                                                                               | 
LABEL_14:                                                                                                     | 
      memmove(v40, Address, v11);                                                                             | 
      v40[0] = v11;                                                                                           | 
LABEL_15:                                                                                                     | 
      v12 = (struct tagBITMAPINFO *)v40;                                                                      | 
      goto LABEL_16;                                                                                          | 
    }                                                                                                         | 
    v11 = 0;                                                                                                  | 
    if ( v10 >= 0x28 && !*((_WORD *)Address + 7) )                                                            | 
    {                                                                                                         | 
      v11 = 40;                                                                                               | 
    }                                                                                                         | 
  }                                                                                                           | 
  if ( v11 )                                                                                                  | 
  {                                                                                                           | 
    goto LABEL_14;                                                                                            | 
  }                                                                                                           | 
  if ( v10 == 40 )                                                                                            | 
  {                                                                                                           | 
    Address[8] = 0;                                                                                           | 
  }                                                                                                           | 
  v13 = a7;                                                                                                   | 
  if ( v10 == 12 )                                                                                            | 
  {                                                                                                           | 
    v14 = 3;                                                                                                  | 
    v15 = 12;                                                                                                 | 
    v16 = 3;                                                                                                  | 
    v17 = *((_WORD *)Address + 5);                                                                            | 
    v18 = 0;                                                                                                  | 
    v19 = 0;                                                                                                  | 
    goto LABEL_29;                                                                                            | 
  }                                                                                                           | 
  if ( v10 >= 0x28 )                                                                                          | 
  {                                                                                                           | 
    v15 = v10;                                                                                                | 
    v16 = 4;                                                                                                  | 
    v17 = *((_WORD *)Address + 7);                                                                            | 
    v18 = Address[8];                                                                                         | 
    v19 = Address[4];                                                                                         | 
    v14 = 3;                                                                                                  | 
LABEL_29:                                                                                                     | 
    switch ( v19 )                                                                                            | 
    {                                                                                                         | 
      case 3u:                                                                                                | 
        if ( a7 == 1 )                                                                                        | 
        {                                                                                                     | 
          v13 = 0;                                                                                            | 
        }                                                                                                     | 
        if ( ((v17 - 16) & 0xFFEF) != 0 )                                                                     | 
        {                                                                                                     | 
          goto LABEL_33;                                                                                      | 
        }                                                                                                     | 
        v18 = 0;                                                                                              | 
        if ( v10 > 0x28 )                                                                                     | 
        {                                                                                                     | 
          v14 = 0;                                                                                            | 
        }                                                                                                     | 
        else                                                                                                  | 
        {                                                                                                     | 
          v18 = 3;                                                                                            | 
        }                                                                                                     | 
LABEL_63:                                                                                                     | 
        if ( v18 && v18 <= v14 )                                                                              | 
        {                                                                                                     | 
          v14 = v18;                                                                                          | 
        }                                                                                                     | 
        if ( v13 != 1 )                                                                                       | 
        {                                                                                                     | 
          if ( v13 == 2 )                                                                                     | 
          {                                                                                                   | 
            v16 = 0;                                                                                          | 
          }                                                                                                   | 
          v8 = v16;                                                                                           | 
        }                                                                                                     | 
        v11 = (v15 + v8 * v14 + 3) & 0xFFFFFFFC;                                                              | 
        if ( v11 >= v15 )                                                                                     | 
        {                                                                                                     | 
          v9 = v33;                                                                                           | 
          goto LABEL_73;                                                                                      | 
        }                                                                                                     | 
LABEL_33:                                                                                                     | 
        v11 = 0;                                                                                              | 
        v12 = (struct tagBITMAPINFO *)v40;                                                                    | 
        goto LABEL_96;                                                                                        | 
      case 0u:                                                                                                | 
        if ( v17 != 1 )                                                                                       | 
        {                                                                                                     | 
          if ( v17 != 4 )                                                                                     | 
          {                                                                                                   | 
            if ( v17 != 8 )                                                                                   | 
            {                                                                                                 | 
              if ( a7 == 1 )                                                                                  | 
              {                                                                                               | 
                v13 = 0;                                                                                      | 
              }                                                                                               | 
              v14 = 0;                                                                                        | 
              if ( ((v17 - 16) & 0xFFE7) != 0 || v17 == 40 )                                                  | 
              {                                                                                               | 
                goto LABEL_33;                                                                                | 
              }                                                                                               | 
              goto LABEL_63;                                                                                  | 
            }                                                                                                 | 
LABEL_60:                                                                                                     | 
            v14 = 256;                                                                                        | 
            goto LABEL_63;                                                                                    | 
          }                                                                                                   | 
LABEL_62:                                                                                                     | 
          v14 = 16;                                                                                           | 
          goto LABEL_63;                                                                                      | 
        }                                                                                                     | 
LABEL_52:                                                                                                     | 
        v14 = 2;                                                                                              | 
        goto LABEL_63;                                                                                        | 
      case 0xAu:                                                                                              | 
        switch ( v17 )                                                                                        | 
        {                                                                                                     | 
          case 1:                                                                                             | 
            goto LABEL_52;                                                                                    | 
          case 4:                                                                                             | 
            goto LABEL_62;                                                                                    | 
          case 8:                                                                                             | 
            goto LABEL_60;                                                                                    | 
        }                                                                                                     | 
        if ( v17 != 32 )                                                                                      | 
        {                                                                                                     | 
          goto LABEL_33;                                                                                      | 
        }                                                                                                     | 
        break;                                                                                                | 
      case 2u:                                                                                                | 
      case 0xCu:                                                                                              | 
        if ( v17 != 4 )                                                                                       | 
        {                                                                                                     | 
          goto LABEL_33;                                                                                      | 
        }                                                                                                     | 
        goto LABEL_62;                                                                                        | 
      case 1u:                                                                                                | 
      case 0xBu:                                                                                              | 
        if ( v17 != 8 )                                                                                       | 
        {                                                                                                     | 
          goto LABEL_33;                                                                                      | 
        }                                                                                                     | 
        goto LABEL_60;                                                                                        | 
      default:                                                                                                | 
        if ( v19 - 4 > 1 )                                                                                    | 
        {                                                                                                     | 
          goto LABEL_33;                                                                                      | 
        }                                                                                                     | 
        break;                                                                                                | 
    }                                                                                                         | 
    v14 = 0;                                                                                                  | 
    goto LABEL_63;                                                                                            | 
  }                                                                                                           | 
  v11 = 0;                                                                                                    | 
LABEL_73:                                                                                                     | 
  if ( !v11 )                                                                                                 | 
  {                                                                                                           | 
    goto LABEL_15;                                                                                            | 
  }                                                                                                           | 
  v12 = (struct tagBITMAPINFO *)Win32AllocPoolZInit(v11, 1886221383LL);                                       | 
  if ( v12 )                                                                                                  | 
  {                                                                                                           | 
    v20 = (char *)Address + v11;                                                                              | 
    if ( v20 < (char *)Address || (unsigned __int64)v20 > MmUserProbeAddress )                                | 
    {                                                                                                         | 
      *(_BYTE *)MmUserProbeAddress = 0;                                                                       | 
    }                                                                                                         | 
    memmove(v12, Address, v11);                                                                               | 
    v12->bmiHeader.biSize = v10;                                                                              | 
    if ( GreGetBitmapSizeInternal(v12, a7, v10) != v11 )                                                      | 
    {                                                                                                         | 
LABEL_22:                                                                                                     | 
      v11 = 0;                                                                                                | 
      goto LABEL_96;                                                                                          | 
    }                                                                                                         | 
    if ( v10 >= 0x28 )                                                                                        | 
    {                                                                                                         | 
      v12->bmiHeader.biClrUsed = 0;                                                                           | 
    }                                                                                                         | 
    if ( a4 )                                                                                                 | 
    {                                                                                                         | 
      if ( v10 < 0x28 )                                                                                       | 
      {                                                                                                       | 
        if ( !LOWORD(v12->bmiHeader.biWidth) || !LOWORD(v12->bmiHeader.biHeight) )                            | 
        {                                                                                                     | 
          goto LABEL_87;                                                                                      | 
        }                                                                                                     | 
        v21 = HIWORD(v12->bmiHeader.biHeight) == 0;                                                           | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        if ( !v12->bmiHeader.biWidth || !v12->bmiHeader.biPlanes )                                            | 
        {                                                                                                     | 
          goto LABEL_87;                                                                                      | 
        }                                                                                                     | 
        v21 = v12->bmiHeader.biBitCount == 0;                                                                 | 
      }                                                                                                       | 
      v22 = 0;                                                                                                | 
      if ( !v21 )                                                                                             | 
      {                                                                                                       | 
LABEL_88:                                                                                                     | 
        v28 = v22;                                                                                            | 
        goto LABEL_16;                                                                                        | 
      }                                                                                                       | 
LABEL_87:                                                                                                     | 
      v22 = 1;                                                                                                | 
      goto LABEL_88;                                                                                          | 
    }                                                                                                         | 
  }                                                                                                           | 
LABEL_16:                                                                                                     | 
  if ( v11 && v9 && v12 )                                                                                     | 
  {                                                                                                           | 
    if ( v12->bmiHeader.biSize < 0x28 || v12->bmiHeader.biCompression - 1 > 1 || v12->bmiHeader.biSizeImage ) | 
    {                                                                                                         | 
      if ( a8 || (BitmapBitsSize = GreGetBitmapBitsSize(v12)) != 0 )                                          | 
      {                                                                                                       | 
        inited = umptr_w<unsigned char>::init_probe(v37, v9, BitmapBitsSize, 4LL);                            | 
        v29 = *(_QWORD *)inited;                                                                              | 
        v30 = *(_QWORD *)(inited + 8);                                                                        | 
        v31 = *(_QWORD *)(inited + 16);                                                                       | 
        v32 = *(_WORD *)(inited + 24);                                                                        | 
      }                                                                                                       | 
      v11 &= -(v29 != 0);                                                                                     | 
      goto LABEL_96;                                                                                          | 
    }                                                                                                         | 
    goto LABEL_22;                                                                                            | 
  }                                                                                                           | 
LABEL_96:                                                                                                     | 
  if ( v29 && v28 || !v11 || !v12 )                                                                           | 
  {                                                                                                           | 
    DIBits = 0;                                                                                               | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    OPTAPIDCOBJ::OPTAPIDCOBJ((OPTAPIDCOBJ *)v39, a1);                                                         | 
    DIBits = GrepGetDIBits((OPTAPIDCOBJ *)v39, v34, (__int64)&v29, v12, a7, BitmapBitsSize, v11);             | 
    if ( DIBits )                                                                                             | 
    {                                                                                                         | 
      memmove(Address, v12, v11);                                                                             | 
    }                                                                                                         | 
    OPTAPIDCOBJ::~OPTAPIDCOBJ((OPTAPIDCOBJ *)v39);                                                            | 
  }                                                                                                           | 
  if ( v12 )                                                                                                  | 
  {                                                                                                           | 
    if ( v12 != (struct tagBITMAPINFO *)v40 )                                                                 | 
    {                                                                                                         | 
      Win32FreePool(v12);                                                                                     | 
    }                                                                                                         | 
  }                                                                                                           | 
  return DIBits;                                                                                              | 
}                                                                                                             | 
293 row(s)
