__int64 __fastcall NtGdiTransparentBlt(
        __int64 a1,
        unsigned int a2,
        unsigned int a3,
        unsigned int a4,
        int a5,
        __int64 a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v15 = *(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, int, __int64, int, int, int, int, int))(*(_QWORD *)(*(_QWORD *)(W32GetSessionState() + 136) + 312LL) + 2304LL);
  if ( v15 )
  {
    return v15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
  }
  else
  {
    return 3221225500LL;
  }
}
25 row(s)
