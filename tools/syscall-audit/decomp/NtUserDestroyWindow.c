__int64 __fastcall NtUserDestroyWindow(__int64 a1)              | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v2 = (struct tagWND *)ValidateReceivingHwnd(a1, 1LL);         | 
  v3 = 0;                                                       | 
  if ( v2 )                                                     | 
  {                                                             | 
    v3 = xxxDestroyWindow(v2);                                  | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v3;                                                    | 
}                                                               | 
14 row(s)
