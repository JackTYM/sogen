__int64 __fastcall NtCreateNamedPipeFile(
        int a1,
        int a2,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11,
        int a12,
        int a13,
        __int64 *a14)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v16 = 0LL;
  v18 = 0;
  v19 = 0;
  v20 = 0;
  if ( a14 )
  {
    v17 = 1;
    if ( KeGetCurrentThread()->PreviousMode )
    {
      if ( ((unsigned __int8)a14 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v16 = *a14;
    }
    else
    {
      v16 = *a14;
    }
  }
  else
  {
    v17 = 0;
  }
  v15[0] = a8;
  v15[1] = a9;
  v15[2] = a10;
  v15[3] = a11;
  v15[4] = a12;
  v15[5] = a13;
  return IopCreateFile(a1, a2, a3, a4, 0LL, 0, a5, a6, a7, 0LL, 0, 1, (__int64)v15, 0, 0, 0LL);
}
50 row(s)
