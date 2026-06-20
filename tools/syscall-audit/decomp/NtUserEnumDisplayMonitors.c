__int64 __fastcall NtUserEnumDisplayMonitors(HDC a1, __int128 *a2) | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  v2 = a2;                                                         | 
  v3 = a1;                                                         | 
  v7 = 0LL;                                                        | 
  LOBYTE(a2) = a2 != 0LL;                                          | 
  LOBYTE(a1) = a1 != 0LL;                                          | 
  EtwTraceEnumDisplayMonitors(a1, a2);                             | 
  EnterLeaveCritShared::EnterLeaveCritShared(v6, 1LL);             | 
  if ( *(_DWORD *)(W32GetUserGdiSessionState() + 32) )             | 
  {                                                                | 
    if ( v2 )                                                      | 
    {                                                              | 
      if ( (unsigned __int64)v2 >= MmUserProbeAddress )            | 
      {                                                            | 
        v2 = (__int128 *)MmUserProbeAddress;                       | 
      }                                                            | 
      v7 = *v2;                                                    | 
    }                                                              | 
    v4 = (int)xxxEnumDisplayMonitors(v3, 0);                       | 
    UserSessionSwitchLeaveCritWithNonPaged();                      | 
    return v4;                                                     | 
  }                                                                | 
  else                                                             | 
  {                                                                | 
    UserSessionSwitchLeaveCritWithNonPaged();                      | 
    return 0LL;                                                    | 
  }                                                                | 
}                                                                  | 
31 row(s)
