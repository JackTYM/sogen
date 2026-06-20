void *__fastcall NtGdiAddFontMemResourceEx(void *a1, unsigned int a2, _BYTE *a3, unsigned int a4, ULONG64 a5) | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  v5 = a4;                                                                                                    | 
  v6 = a3;                                                                                                    | 
  v7 = a2;                                                                                                    | 
  v13 = a2;                                                                                                   | 
  v19 = a1;                                                                                                   | 
  v15 = a2;                                                                                                   | 
  v17 = a3;                                                                                                   | 
  v16 = a4;                                                                                                   | 
  v9 = 1;                                                                                                     | 
  v14 = 1;                                                                                                    | 
  v10 = 0LL;                                                                                                  | 
  memset_0(v20, 0, 0x48uLL);                                                                                  | 
  if ( (int)UmfdHostLifeTimeManager::WaitForSessionRasterizerInitialization() < 0 )                           | 
  {                                                                                                           | 
    return 0LL;                                                                                               | 
  }                                                                                                           | 
  if ( !v7 )                                                                                                  | 
  {                                                                                                           | 
    return 0LL;                                                                                               | 
  }                                                                                                           | 
  if ( !a1 )                                                                                                  | 
  {                                                                                                           | 
    return 0LL;                                                                                               | 
  }                                                                                                           | 
  v11 = (_DWORD *)a5;                                                                                         | 
  if ( !a5 )                                                                                                  | 
  {                                                                                                           | 
    return 0LL;                                                                                               | 
  }                                                                                                           | 
  if ( (_DWORD)v5 )                                                                                           | 
  {                                                                                                           | 
    if ( (unsigned int)v5 > 0x48 )                                                                            | 
    {                                                                                                         | 
      v9 = 0;                                                                                                 | 
      v14 = 0;                                                                                                | 
    }                                                                                                         | 
    else                                                                                                      | 
    {                                                                                                         | 
      if ( (unsigned __int64)v6 >= MmUserProbeAddress )                                                       | 
      {                                                                                                       | 
        v6 = (_BYTE *)MmUserProbeAddress;                                                                     | 
      }                                                                                                       | 
      RtlCopyVolatileMemory(v20, v6, v5);                                                                     | 
      v6 = v20;                                                                                               | 
      v17 = v20;                                                                                              | 
      if ( v5 != 4LL * v21 + 8 )                                                                              | 
      {                                                                                                       | 
        v9 = 0;                                                                                               | 
        v14 = 0;                                                                                              | 
      }                                                                                                       | 
      v7 = v13;                                                                                               | 
    }                                                                                                         | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    v6 = 0LL;                                                                                                 | 
    v17 = 0LL;                                                                                                | 
  }                                                                                                           | 
  if ( v9 )                                                                                                   | 
  {                                                                                                           | 
    v13 = 0;                                                                                                  | 
    v10 = GrepAddFontMemResource(a1, v7, (struct tagDESIGNVECTOR *)v6, v5, &v13);                             | 
    v18 = v10;                                                                                                | 
    if ( v10 )                                                                                                | 
    {                                                                                                         | 
      if ( a5 >= MmUserProbeAddress )                                                                         | 
      {                                                                                                       | 
        v11 = (_DWORD *)MmUserProbeAddress;                                                                   | 
      }                                                                                                       | 
      *v11 = v13;                                                                                             | 
    }                                                                                                         | 
  }                                                                                                           | 
  return v10;                                                                                                 | 
}                                                                                                             | 
78 row(s)
