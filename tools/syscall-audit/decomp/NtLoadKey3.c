line
__int64 __fastcall NtLoadKey3(
        int a1,
        int a2,
        int a3,
        __int64 a4,
        int a5,
        ACCESS_MASK DesiredAccess,
        __int64 a7,
        int a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v19 = 0LL;
  v18 = 0LL;
  v16 = 0;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v11 = 0LL;
  v17 = 0LL;
  v13 = 0;
  while ( a5 )
  {
    if ( PreviousMode == 1 )
    {
      if ( (a4 & 7) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v13 = v16;
      v11 = v17;
    }
    v14 = *(_OWORD *)a4;
    if ( (unsigned __int8)*(_OWORD *)a4 == 1LL )
    {
      if ( (v13 & 2) != 0 )
      {
        return 3221225714LL;
      }
      v11 = *(_QWORD *)(a4 + 8);
      v17 = v11;
      v13 |= 2u;
    }
    else if ( (unsigned __int8)v14 == 2LL )
    {
      if ( (v13 & 4) != 0 )
      {
        return 3221225714LL;
      }
      v19 = *(_QWORD *)(a4 + 8);
      v13 |= 4u;
    }
    else
    {
      if ( (unsigned __int8)v14 != 3LL )
      {
        return 3221225714LL;
      }
      if ( (v13 & 8) != 0 )
      {
        return 3221225714LL;
      }
      v18 = *(_QWORD *)(a4 + 8);
      v13 |= 8u;
    }
    v16 = v13;
    a4 += 16LL;
    --a5;
  }
  return CmLoadDifferencingKey(a1, a2, a3 | 0x8000u, 0, v11, v19, DesiredAccess, a7, a8, 0LL, 0, v18, PreviousMode);
}
