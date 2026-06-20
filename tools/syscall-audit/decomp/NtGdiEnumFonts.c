__int64 __fastcall NtGdiEnumFonts(                                                                                 | 
        HDC a1,                                                                                                    | 
        unsigned int a2,                                                                                           | 
        unsigned int a3,                                                                                           | 
        unsigned int a4,                                                                                           | 
        char *Src,                                                                                                 | 
        unsigned int a6,                                                                                           | 
        ULONG64 a7,                                                                                                | 
        volatile void *Address)                                                                                    | 
{                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                    | 
                                                                                                                   | 
  v10 = 0LL;                                                                                                       | 
  v18 = 0LL;                                                                                                       | 
  v11 = 1;                                                                                                         | 
  v17 = 0;                                                                                                         | 
  if ( Src )                                                                                                       | 
  {                                                                                                                | 
    if ( a4 - 1 > 0x1F )                                                                                           | 
    {                                                                                                              | 
      return 0LL;                                                                                                  | 
    }                                                                                                              | 
    v10 = (void *)AllocFreeTmpBuffer(2 * a4);                                                                      | 
    v18 = v10;                                                                                                     | 
    if ( v10 )                                                                                                     | 
    {                                                                                                              | 
      v12 = 2LL * a4;                                                                                              | 
      if ( &Src[v12] < Src || (unsigned __int64)&Src[v12] > MmUserProbeAddress )                                   | 
      {                                                                                                            | 
        *(_BYTE *)MmUserProbeAddress = 0;                                                                          | 
      }                                                                                                            | 
      memmove(v10, Src, v12);                                                                                      | 
      *((_WORD *)v10 + a4 - 1) = 0;                                                                                | 
    }                                                                                                              | 
    else                                                                                                           | 
    {                                                                                                              | 
      v11 = 0;                                                                                                     | 
    }                                                                                                              | 
  }                                                                                                                | 
  if ( v11 )                                                                                                       | 
  {                                                                                                                | 
    v14 = (unsigned int *)a7;                                                                                      | 
    if ( a7 >= MmUserProbeAddress )                                                                                | 
    {                                                                                                              | 
      v14 = (unsigned int *)MmUserProbeAddress;                                                                    | 
    }                                                                                                              | 
    v15 = *v14;                                                                                                    | 
    v17 = *v14;                                                                                                    | 
    if ( Address )                                                                                                 | 
    {                                                                                                              | 
      ProbeForWrite(Address, v15, 4u);                                                                             | 
    }                                                                                                              | 
    if ( (unsigned int)GrepGetCurrentProcessBehaviorRestriction() == 1 && a1 == (HDC)-589410304LL )                | 
    {                                                                                                              | 
      a1 = *(HDC *)(*(_QWORD *)(W32GetUserSessionState() + 57104) + 64LL);                                         | 
    }                                                                                                              | 
    APIDCOBJ::APIDCOBJ((APIDCOBJ *)v19, a1);                                                                       | 
    if ( v19[0] )                                                                                                  | 
    {                                                                                                              | 
      v11 = GrepEnumFonts((struct XDCOBJ *)v19, a2, a3, (const unsigned __int16 *)v10, a6, &v17, (void *)Address); | 
    }                                                                                                              | 
    else                                                                                                           | 
    {                                                                                                              | 
      EngSetLastError(6u);                                                                                         | 
      v11 = 0;                                                                                                     | 
    }                                                                                                              | 
    v16 = (_DWORD *)a7;                                                                                            | 
    if ( a7 >= MmUserProbeAddress )                                                                                | 
    {                                                                                                              | 
      v16 = (_DWORD *)MmUserProbeAddress;                                                                          | 
    }                                                                                                              | 
    *v16 = v17;                                                                                                    | 
    APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v19);                                                                          | 
  }                                                                                                                | 
  if ( v10 )                                                                                                       | 
  {                                                                                                                | 
    FreeTmpBuffer(v10);                                                                                            | 
  }                                                                                                                | 
  return v11;                                                                                                      | 
}                                                                                                                  | 
80 row(s)
