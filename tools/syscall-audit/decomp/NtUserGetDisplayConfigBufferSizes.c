__int64 __fastcall NtUserGetDisplayConfigBufferSizes(unsigned int a1, __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  CurrentProcessSessionId = GetCurrentProcessSessionId();
  v5 = *(__int64 (__fastcall **)(_QWORD, __int64))(*(_QWORD *)(*(_QWORD *)(W32GetSessionStateForSession(CurrentProcessSessionId)
                                                                         + 136)
                                                             + 648LL)
                                                 + 56LL);
  if ( v5 )
  {
    return v5(a1, a2);
  }
  else
  {
    return 3221225500LL;
  }
}
18 row(s)
