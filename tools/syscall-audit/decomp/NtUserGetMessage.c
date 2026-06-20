__int64 __fastcall NtUserGetMessage(_OWORD *a1, __int64 a2, int a3, int a4) | 
{                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]             | 
                                                                            | 
  v12 = 0LL;                                                                | 
  v13 = 0LL;                                                                | 
  v14 = 0LL;                                                                | 
  EnterCrit(0LL, 0LL);                                                      | 
  if ( ((a4 | a3) & 0xFFFE0000) != 0 )                                      | 
  {                                                                         | 
    Message = 0;                                                            | 
    UserSetLastError(87LL);                                                 | 
  }                                                                         | 
  else                                                                      | 
  {                                                                         | 
    v11 = 1;                                                                | 
    Message = xxxInternalGetMessage((__int64)&v12, v11, 1);                 | 
    HIDWORD(v12) = 0;                                                       | 
    HIDWORD(v14) = 0;                                                       | 
    v8 = v13;                                                               | 
    v9 = v14;                                                               | 
    if ( (unsigned __int64)a1 >= MmUserProbeAddress )                       | 
    {                                                                       | 
      a1 = (_OWORD *)MmUserProbeAddress;                                    | 
    }                                                                       | 
    *a1 = v12;                                                              | 
    a1[1] = v8;                                                             | 
    a1[2] = v9;                                                             | 
  }                                                                         | 
  UserSessionSwitchLeaveCrit();                                             | 
  return Message;                                                           | 
}                                                                           | 
32 row(s)
