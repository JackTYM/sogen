__int64 __fastcall NtFindAtom(_WORD *Src, size_t Size, _WORD *a3)         | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  v4 = (unsigned int)Size;                                                | 
  v13 = 0;                                                                | 
  v15 = 0LL;                                                              | 
  v16 = 0LL;                                                              | 
  v18 = 0;                                                                | 
  v17 = 0LL;                                                              | 
  if ( (unsigned int)PspUpdateCalloutParameters(2LL, &v15, 0LL, 0LL) )    | 
  {                                                                       | 
    Win32Callouts = PsSessionGetWin32Callouts();                          | 
    v7 = ExReferenceCallBackBlock(Win32Callouts);                         | 
    if ( v7 )                                                             | 
    {                                                                     | 
      guard_dispatch_icall_no_overrides(*(_QWORD *)(v7 + 16), 2LL, &v15); | 
      ExDereferenceCallBackBlock(Win32Callouts, v7);                      | 
    }                                                                     | 
  }                                                                       | 
  v8 = v16;                                                               | 
  if ( !v16 )                                                             | 
  {                                                                       | 
    return 3221225506LL;                                                  | 
  }                                                                       | 
  if ( (unsigned int)v4 > 0x1FE )                                         | 
  {                                                                       | 
    return 3221225485LL;                                                  | 
  }                                                                       | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                      | 
  v10 = Src;                                                              | 
  v14 = Src;                                                              | 
  if ( PreviousMode )                                                     | 
  {                                                                       | 
    if ( a3 )                                                             | 
    {                                                                     | 
      v11 = 0x7FFFFFFF0000LL;                                             | 
      if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )                      | 
      {                                                                   | 
        v11 = (__int64)a3;                                                | 
      }                                                                   | 
      *(_WORD *)v11 = *(_WORD *)v11;                                      | 
      v10 = v14;                                                          | 
    }                                                                     | 
    if ( Src )                                                            | 
    {                                                                     | 
      if ( (_DWORD)v4 && ((unsigned __int8)Src & 1) != 0 )                | 
      {                                                                   | 
        ExRaiseDatatypeMisalignment();                                    | 
      }                                                                   | 
      v10 = v19;                                                          | 
      v14 = v19;                                                          | 
      memmove(v19, Src, v4);                                              | 
      v19[v4 >> 1] = 0;                                                   | 
    }                                                                     | 
  }                                                                       | 
  LODWORD(result) = RtlLookupAtomInAtomTable(v8, v10, &v13);              | 
  if ( a3 )                                                               | 
  {                                                                       | 
    if ( (int)result >= 0 )                                               | 
    {                                                                     | 
      *a3 = v13;                                                          | 
    }                                                                     | 
  }                                                                       | 
  return (unsigned int)result;                                            | 
}                                                                         | 
66 row(s)
