__int64 __fastcall NtGdiGetDCObject(__int64 a1, unsigned int a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  CurrentProcessSessionId = GetCurrentProcessSessionId();
  v5 = *(__int64 (__fastcall **)(__int64, _QWORD))(*(_QWORD *)(*(_QWORD *)(W32GetSessionStateForSession(CurrentProcessSessionId)
                                                                         + 136)
                                                             + 312LL)
                                                 + 1160LL);
  if ( v5 )
  {
    return v5(a1, a2);
  }
  else
  {
    return -1073741796LL;
  }
}
18 row(s)
