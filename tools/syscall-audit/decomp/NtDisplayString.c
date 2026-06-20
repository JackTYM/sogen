__int64 __fastcall NtDisplayString(unsigned __int64 a1)                      | 
{                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]              | 
                                                                             | 
  if ( !a1 )                                                                 | 
  {                                                                          | 
    return 3221225485LL;                                                     | 
  }                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                         | 
  if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )               | 
  {                                                                          | 
    return 3221225569LL;                                                     | 
  }                                                                          | 
  if ( PreviousMode )                                                        | 
  {                                                                          | 
    v4 = 0x7FFFFFFF0000LL;                                                   | 
    if ( a1 < 0x7FFFFFFF0000LL )                                             | 
    {                                                                        | 
      v4 = a1;                                                               | 
    }                                                                        | 
    P = *(_DWORD *)v4;                                                       | 
    v5 = *(_WORD **)(v4 + 8);                                                | 
    if ( !v5 || !HIWORD(*(_DWORD *)v4) )                                     | 
    {                                                                        | 
      return 0LL;                                                            | 
    }                                                                        | 
    if ( !*v5 )                                                              | 
    {                                                                        | 
      return 0LL;                                                            | 
    }                                                                        | 
    Pool2 = (_WORD *)ExAllocatePool2(0x40uLL, HIWORD(P) + 2LL, 0x67727453u); | 
    v7 = Pool2;                                                              | 
    if ( Pool2 )                                                             | 
    {                                                                        | 
      memmove(Pool2, v5, HIWORD(P));                                         | 
      v7[(unsigned __int64)HIWORD(P) >> 1] = 0;                              | 
      goto LABEL_21;                                                         | 
    }                                                                        | 
    return 3221225495LL;                                                     | 
  }                                                                          | 
  v8 = *(_WORD **)(a1 + 8);                                                  | 
  if ( !v8 )                                                                 | 
  {                                                                          | 
    return 0LL;                                                              | 
  }                                                                          | 
  v9 = *(unsigned __int16 *)(a1 + 2);                                        | 
  if ( !(_WORD)v9 || !*v8 )                                                  | 
  {                                                                          | 
    return 0LL;                                                              | 
  }                                                                          | 
  v10 = (_WORD *)ExAllocatePool2(0x40uLL, v9 + 2, 0x67727453u);              | 
  v7 = v10;                                                                  | 
  if ( !v10 )                                                                | 
  {                                                                          | 
    return 3221225495LL;                                                     | 
  }                                                                          | 
  memmove(v10, *(const void **)(a1 + 8), *(unsigned __int16 *)(a1 + 2));     | 
  v7[(unsigned __int64)*(unsigned __int16 *)(a1 + 2) >> 1] = 0;              | 
LABEL_21:                                                                    | 
  v11 = BgkDisplayStringEx(v7);                                              | 
  ExFreePoolWithTag(v7, 0);                                                  | 
  if ( !v11 )                                                                | 
  {                                                                          | 
    return 3221225473LL;                                                     | 
  }                                                                          | 
  return 0LL;                                                                | 
}                                                                            | 
67 row(s)
