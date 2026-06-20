__int64 __fastcall NtTerminateEnclave(__int64 a1, unsigned int a2)                                          | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v8 = 0;                                                                                                   | 
  if ( (a2 & 0xFFFFFFFA) != 0 )                                                                             | 
  {                                                                                                         | 
    return 3221225712LL;                                                                                    | 
  }                                                                                                         | 
  v5 = MiObtainReferencedVadEx(a1, 0LL, &v8);                                                               | 
  v6 = (void *)v5;                                                                                          | 
  if ( !v5 )                                                                                                | 
  {                                                                                                         | 
    return v8;                                                                                              | 
  }                                                                                                         | 
  if ( (*(_DWORD *)(v5 + 48) & 0xC200000) == 0x8200000                                                      | 
    && (*(unsigned int *)(v5 + 24) | ((unsigned __int64)*(unsigned __int8 *)(v5 + 32) << 32)) << 12 == a1 ) | 
  {                                                                                                         | 
    v7 = MiTerminateEnclave(KeGetCurrentThread()->ApcState.Process, v5, a2);                                | 
  }                                                                                                         | 
  else                                                                                                      | 
  {                                                                                                         | 
    v7 = -1073741800;                                                                                       | 
  }                                                                                                         | 
  MiUnlockAndDereferenceVad(v6);                                                                            | 
  return v7;                                                                                                | 
}                                                                                                           | 
27 row(s)
