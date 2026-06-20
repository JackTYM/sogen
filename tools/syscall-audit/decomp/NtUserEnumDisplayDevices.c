__int64 __fastcall NtUserEnumDisplayDevices(__int64 a1, unsigned int a2, __int64 a3, unsigned int a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  CurrentProcessSessionId = GetCurrentProcessSessionId();
  v9 = *(__int64 (__fastcall **)(__int64, _QWORD, __int64, _QWORD))(*(_QWORD *)(*(_QWORD *)(W32GetSessionStateForSession(CurrentProcessSessionId)
                                                                                          + 136)
                                                                              + 648LL)
                                                                  + 24LL);
  if ( v9 )
  {
    return v9(a1, a2, a3, a4);
  }
  else
  {
    return 3221225500LL;
  }
}
18 row(s)
