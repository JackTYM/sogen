__int64 __fastcall NtAddAtomEx(_WORD *Src, size_t Size, _WORD *a3, unsigned int a4) | 
{                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                     | 
                                                                                    | 
  v6 = (unsigned int)Size;                                                          | 
  v15 = 0;                                                                          | 
  if ( (a4 & 0xFFFFFFFD) != 0 )                                                     | 
  {                                                                                 | 
    return 3221225485LL;                                                            | 
  }                                                                                 | 
  v17 = 0LL;                                                                        | 
  v18 = 0LL;                                                                        | 
  v20 = 0;                                                                          | 
  v19 = 0LL;                                                                        | 
  if ( (unsigned int)PspUpdateCalloutParameters(2LL, &v17, 0LL, 0LL) )              | 
  {                                                                                 | 
    Win32Callouts = PsSessionGetWin32Callouts();                                    | 
    ExCallCallBack(Win32Callouts, v9, &v17);                                        | 
  }                                                                                 | 
  v10 = v18;                                                                        | 
  if ( !v18 )                                                                       | 
  {                                                                                 | 
    return 3221225506LL;                                                            | 
  }                                                                                 | 
  if ( (unsigned int)v6 > 0x1FE )                                                   | 
  {                                                                                 | 
    return 3221225485LL;                                                            | 
  }                                                                                 | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                | 
  v12 = Src;                                                                        | 
  v16 = Src;                                                                        | 
  if ( PreviousMode )                                                               | 
  {                                                                                 | 
    if ( a3 )                                                                       | 
    {                                                                               | 
      v13 = 0x7FFFFFFF0000LL;                                                       | 
      if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )                                | 
      {                                                                             | 
        v13 = (__int64)a3;                                                          | 
      }                                                                             | 
      *(_WORD *)v13 = *(_WORD *)v13;                                                | 
      v12 = v16;                                                                    | 
    }                                                                               | 
    if ( Src )                                                                      | 
    {                                                                               | 
      if ( (_DWORD)v6 && ((unsigned __int8)Src & 1) != 0 )                          | 
      {                                                                             | 
        ExRaiseDatatypeMisalignment();                                              | 
      }                                                                             | 
      v12 = v21;                                                                    | 
      v16 = v21;                                                                    | 
      memmove(v21, Src, v6);                                                        | 
      v21[v6 >> 1] = 0;                                                             | 
    }                                                                               | 
  }                                                                                 | 
  LODWORD(result) = RtlAddAtomToAtomTableEx(v10, v12, &v15, a4);                    | 
  if ( a3 )                                                                         | 
  {                                                                                 | 
    if ( (int)result >= 0 )                                                         | 
    {                                                                               | 
      *a3 = v15;                                                                    | 
    }                                                                               | 
  }                                                                                 | 
  return (unsigned int)result;                                                      | 
}                                                                                   | 
65 row(s)
