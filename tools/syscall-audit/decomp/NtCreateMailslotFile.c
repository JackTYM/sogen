__int64 __fastcall NtCreateMailslotFile(
        int a1,
        int a2,
        int a3,
        int a4,
        int a5,
        unsigned int a6,
        unsigned int a7,
        __int64 a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  *(_OWORD *)v9 = 0LL;
  v10 = 0LL;
  if ( a8 )
  {
    LOBYTE(v10) = 1;
    if ( KeGetCurrentThread()->PreviousMode )
    {
      if ( (a8 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v9[1] = *(_QWORD *)a8;
    }
    else
    {
      v9[1] = *(_QWORD *)a8;
    }
  }
  v9[0] = __PAIR64__(a7, a6);
  return IopCreateFile(a1, a2, a3, a4, 0LL, 0, 3, 2, a5, 0LL, 0, 2, (__int64)v9, 0, 0, 0LL);
}
33 row(s)
