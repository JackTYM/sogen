__int64 __fastcall NtUserDrainThreadCoreMessagingCompletions2(__int64 a1) | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  v2 = EnterCrit(0LL, 0LL);                                               | 
  v3 = ValidateReceivingHwnd(a1, 0LL);                                    | 
  v4 = 0LL;                                                               | 
  v5 = v3;                                                                | 
  if ( v3 )                                                               | 
  {                                                                       | 
    BugCheckParameter3[0] = *(_QWORD *)(v2 + 456);                        | 
    *(_QWORD *)(v2 + 456) = BugCheckParameter3;                           | 
    BugCheckParameter3[1] = v3;                                           | 
    HMLockObject(v3);                                                     | 
    v6 = DrainThreadCoreMessagingCompletions2(v5);                        | 
    CurrentThreadNonPaged = (ULONG_PTR *)W32GetCurrentThreadNonPaged();   | 
    if ( CurrentThreadNonPaged )                                          | 
    {                                                                     | 
      v4 = *CurrentThreadNonPaged;                                        | 
    }                                                                     | 
    Win32HM_UnlockFromThread<0>(v4, (ULONG_PTR)BugCheckParameter3);       | 
  }                                                                       | 
  else                                                                    | 
  {                                                                       | 
    v6 = 0;                                                               | 
  }                                                                       | 
  UserSessionSwitchLeaveCrit();                                           | 
  return v6;                                                              | 
}                                                                         | 
29 row(s)
