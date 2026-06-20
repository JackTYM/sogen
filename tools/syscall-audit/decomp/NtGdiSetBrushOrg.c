__int64 __fastcall NtGdiSetBrushOrg(HDC a1, __int64 a2, __int64 a3, void *a4) | 
{                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]               | 
                                                                              | 
  Src[0] = 0LL;                                                               | 
  v5 = GreSetBrushOrg(a1);                                                    | 
  if ( v5 && a4 )                                                             | 
  {                                                                           | 
    if ( (unsigned __int64)a4 >= MmUserProbeAddress )                         | 
    {                                                                         | 
      a4 = (void *)MmUserProbeAddress;                                        | 
    }                                                                         | 
    RtlCopyVolatileMemory(a4, Src, 8uLL);                                     | 
  }                                                                           | 
  return v5;                                                                  | 
}                                                                             | 
16 row(s)
