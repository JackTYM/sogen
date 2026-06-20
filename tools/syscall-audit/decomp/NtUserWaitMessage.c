__int64 NtUserWaitMessage()                                        | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  EnterCrit(0LL, 0LL);                                             | 
  CurrentThreadNonPaged = (_QWORD *)W32GetCurrentThreadNonPaged(); | 
  if ( CurrentThreadNonPaged )                                     | 
  {                                                                | 
    CurrentThreadNonPaged = (_QWORD *)*CurrentThreadNonPaged;      | 
  }                                                                | 
  if ( *(int *)(CurrentThreadNonPaged[61] + 28LL) <= 0 )           | 
  {                                                                | 
    v1 = xxxSleepThread2(0x3CFFu, 0, 1, 0, 0LL);                   | 
  }                                                                | 
  else                                                             | 
  {                                                                | 
    v1 = ClientWaitMessageExMPH(15615LL);                          | 
  }                                                                | 
  v2 = v1;                                                         | 
  UserSessionSwitchLeaveCrit();                                    | 
  return v2;                                                       | 
}                                                                  | 
22 row(s)
