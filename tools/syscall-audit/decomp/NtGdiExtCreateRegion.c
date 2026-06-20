__int64 __fastcall NtGdiExtCreateRegion(struct _XFORML *a1, size_t Size, char *Src) | 
{                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                     | 
                                                                                    | 
  v4 = (unsigned int)Size;                                                          | 
  memset(&v10, 0, sizeof(v10));                                                     | 
  Region = 0LL;                                                                     | 
  if ( (unsigned int)(Size - 32) <= 0x270FFE0 )                                     | 
  {                                                                                 | 
    v7 = (void *)AllocFreeTmpBuffer((unsigned int)Size);                            | 
    if ( v7 )                                                                       | 
    {                                                                               | 
      v8 = 1;                                                                       | 
      if ( a1 )                                                                     | 
      {                                                                             | 
        v8 = ProbeAndConvertXFORM(a1, &v10);                                        | 
        a1 = &v10;                                                                  | 
      }                                                                             | 
      if ( v8 )                                                                     | 
      {                                                                             | 
        if ( &Src[v4] < Src || (unsigned __int64)&Src[v4] > MmUserProbeAddress )    | 
        {                                                                           | 
          *(_BYTE *)MmUserProbeAddress = 0;                                         | 
        }                                                                           | 
        memmove(v7, Src, v4);                                                       | 
        Region = 1LL;                                                               | 
      }                                                                             | 
      if ( Region )                                                                 | 
      {                                                                             | 
        Region = GreExtCreateRegion(a1, (unsigned int)v4, v7);                      | 
      }                                                                             | 
      FreeTmpBuffer(v7);                                                            | 
    }                                                                               | 
  }                                                                                 | 
  return Region;                                                                    | 
}                                                                                   | 
36 row(s)
