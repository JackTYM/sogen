__int64 __fastcall NtCreateProfile(int a1, int a2, int a3, int a4, int a5, __int64 a6, SIZE_T a7, int a8, __int64 a9) | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  *(_OWORD *)v16 = 0LL;                                                                                               | 
  PrimaryGroupThread = KeQueryPrimaryGroupThread(KeGetCurrentThread());                                               | 
  LOWORD(v16[1]) = PrimaryGroupThread;                                                                                | 
  if ( a9 == -1 )                                                                                                     | 
  {                                                                                                                   | 
    v16[0] = KeQueryGroupAffinity(PrimaryGroupThread);                                                                | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v16[0] = a9;                                                                                                      | 
  }                                                                                                                   | 
  LODWORD(Length) = a7;                                                                                               | 
  return ExpProfileCreate(a1, a2, a3, a4, a5, a6, Length, a8, 1, (__int64)v16, 1);                                    | 
}                                                                                                                     | 
18 row(s)
