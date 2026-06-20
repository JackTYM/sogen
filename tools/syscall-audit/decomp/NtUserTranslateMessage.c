__int64 __fastcall NtUserTranslateMessage(__int128 *a1, unsigned int a2) | 
{                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]          | 
                                                                         | 
  v6 = 0LL;                                                              | 
  v7 = 0LL;                                                              | 
  v8 = 0LL;                                                              | 
  EnterCrit(0LL, 0LL);                                                   | 
  if ( (unsigned __int64)a1 >= MmUserProbeAddress )                      | 
  {                                                                      | 
    a1 = (__int128 *)MmUserProbeAddress;                                 | 
  }                                                                      | 
  v6 = *a1;                                                              | 
  v7 = a1[1];                                                            | 
  v8 = a1[2];                                                            | 
  v4 = 0;                                                                | 
  if ( ValidateHwnd(v6) )                                                | 
  {                                                                      | 
    v4 = xxxTranslateMessage(&v6, a2);                                   | 
  }                                                                      | 
  UserSessionSwitchLeaveCrit();                                          | 
  return v4;                                                             | 
}                                                                        | 
23 row(s)
