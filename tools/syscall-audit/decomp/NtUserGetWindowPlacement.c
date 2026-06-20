__int64 __fastcall NtUserGetWindowPlacement(__int64 a1, _DWORD *a2) | 
{                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]     | 
                                                                    | 
  memset(Src, 0, sizeof(Src));                                      | 
  v11 = 0LL;                                                        | 
  v12 = 0;                                                          | 
  EnterCrit(0LL, 1LL);                                              | 
  v6 = ValidateHwnd(a1);                                            | 
  if ( v6 )                                                         | 
  {                                                                 | 
    v8 = a2;                                                        | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )               | 
    {                                                               | 
      v8 = (_BYTE *)MmUserProbeAddress;                             | 
    }                                                               | 
    *v8 = *v8;                                                      | 
    v8[43] = v8[43];                                                | 
    LODWORD(Src[0]) = *a2;                                          | 
    WindowPlacement = GetWindowPlacement(v6, Src);                  | 
    if ( WindowPlacement )                                          | 
    {                                                               | 
      RtlCopyVolatileMemory(a2, Src, 0x2CuLL);                      | 
    }                                                               | 
  }                                                                 | 
  else                                                              | 
  {                                                                 | 
    WindowPlacement = 0;                                            | 
  }                                                                 | 
  UserSessionSwitchLeaveCrit(v5, v4, v6);                           | 
  return WindowPlacement;                                           | 
}                                                                   | 
32 row(s)
