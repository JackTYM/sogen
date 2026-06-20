__int64 __fastcall NtUserIsTouchWindow(__int64 a1, unsigned int *a2)          | 
{                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]               | 
                                                                              | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v9, 1LL);                       | 
  v3 = ValidateHwnd(a1);                                                      | 
  if ( v3 )                                                                   | 
  {                                                                           | 
    UserSessionState = W32GetUserSessionState();                              | 
    Prop = GetProp(v3, *(unsigned __int16 *)(UserSessionState + 41506), 1LL); | 
    LODWORD(v3) = (Prop & 0x10000) != 0;                                      | 
    if ( (Prop & 0x10000) != 0 )                                              | 
    {                                                                         | 
      if ( a2 )                                                               | 
      {                                                                       | 
        v6 = a2;                                                              | 
        if ( (unsigned __int64)a2 >= MmUserProbeAddress )                     | 
        {                                                                     | 
          v6 = (_DWORD *)MmUserProbeAddress;                                  | 
        }                                                                     | 
        *v6 = *v6;                                                            | 
        *a2 = Prop & 0xFFFEFFFF;                                              | 
      }                                                                       | 
    }                                                                         | 
  }                                                                           | 
  UserSessionSwitchLeaveCrit();                                               | 
  return (int)v3;                                                             | 
}                                                                             | 
28 row(s)
