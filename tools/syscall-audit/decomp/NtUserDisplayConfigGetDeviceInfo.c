__int64 __fastcall NtUserDisplayConfigGetDeviceInfo(__int64 a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  CurrentProcessSessionId = GetCurrentProcessSessionId();
  v3 = *(__int64 (__fastcall **)(__int64))(*(_QWORD *)(*(_QWORD *)(W32GetSessionStateForSession(CurrentProcessSessionId)
                                                                 + 136)
                                                     + 648LL)
                                         + 8LL);
  if ( v3 )
  {
    return v3(a1);
  }
  else
  {
    return 3221225500LL;
  }
}
18 row(s)
