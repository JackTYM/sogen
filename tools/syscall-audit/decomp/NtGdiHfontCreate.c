__int64 __fastcall NtGdiHfontCreate(char *Src, size_t Size)                | 
{                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]            | 
                                                                           | 
  v2 = (unsigned int)Size;                                                 | 
  if ( !Src || !(_DWORD)Size || (unsigned int)Size > 0x1A4 )               | 
  {                                                                        | 
    return 0LL;                                                            | 
  }                                                                        | 
  memset_0(Srca, 0, 0x1A4uLL);                                             | 
  if ( &Src[v2] < Src || (unsigned __int64)&Src[v2] > MmUserProbeAddress ) | 
  {                                                                        | 
    *(_BYTE *)MmUserProbeAddress = 0;                                      | 
  }                                                                        | 
  memmove(Srca, Src, (unsigned int)v2);                                    | 
  v7 = 0;                                                                  | 
  return hfontCreate(Srca, 0);                                             | 
}                                                                          | 
18 row(s)
