__int64 __fastcall NtUserQueryDisplayConfig(unsigned int a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  CurrentProcessSessionId = GetCurrentProcessSessionId();
  v10 = *(__int64 (__fastcall **)(_QWORD, __int64, __int64, __int64, __int64))(*(_QWORD *)(*(_QWORD *)(W32GetSessionStateForSession(CurrentProcessSessionId) + 136)
                                                                                         + 648LL)
                                                                             + 72LL);
  if ( v10 )
  {
    return v10(a1, a2, a3, a4, a5);
  }
  else
  {
    return 3221225500LL;
  }
}
17 row(s)
