__int64 __fastcall NtGdiSetLayout(__int64 a1, unsigned int a2, unsigned int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v6 = *(__int64 (__fastcall **)(__int64, _QWORD, _QWORD))(*(_QWORD *)(*(_QWORD *)(W32GetSessionState() + 136) + 312LL)
                                                         + 2096LL);
  if ( v6 )
  {
    return v6(a1, a2, a3);
  }
  else
  {
    return 3221225500LL;
  }
}
15 row(s)
