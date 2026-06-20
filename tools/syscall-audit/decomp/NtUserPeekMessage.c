__int64 __fastcall NtUserPeekMessage(_OWORD *a1, __int64 a2, __int64 a3, __int64 a4, int a5, char a6) | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  if ( (a6 & 1) != 0 )                                                                                | 
  {                                                                                                   | 
    CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged();                                 | 
    Message = 0;                                                                                      | 
    if ( CurrentThreadNonPaged )                                                                      | 
    {                                                                                                 | 
      v9 = *CurrentThreadNonPaged;                                                                    | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      v9 = 0LL;                                                                                       | 
    }                                                                                                 | 
    v14.QuadPart = 0LL;                                                                               | 
    if ( KeWaitForSingleObject(*(PVOID *)(v9 + 1608), WrUserRequest, 1, 0, &v14) == 258 )             | 
    {                                                                                                 | 
      return 0LL;                                                                                     | 
    }                                                                                                 | 
    if ( (a6 & 2) != 0 )                                                                              | 
    {                                                                                                 | 
      --*(_QWORD *)(*(_QWORD *)(v9 + 520) + 8LL);                                                     | 
    }                                                                                                 | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    Message = 0;                                                                                      | 
  }                                                                                                   | 
  v15 = 0LL;                                                                                          | 
  v16 = 0LL;                                                                                          | 
  v17 = 0LL;                                                                                          | 
  EnterCrit(0LL, 0LL);                                                                                | 
  if ( (a5 & 0xE300FFFC) != 0 )                                                                       | 
  {                                                                                                   | 
    UserSetLastError(1004LL);                                                                         | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    LODWORD(Timeout) = a5;                                                                            | 
    Message = xxxInternalGetMessage((__int64)&v15, (unsigned __int64)Timeout, 0);                     | 
    if ( Message )                                                                                    | 
    {                                                                                                 | 
      HIDWORD(v15) = 0;                                                                               | 
      HIDWORD(v17) = 0;                                                                               | 
      v11 = v16;                                                                                      | 
      v12 = v17;                                                                                      | 
      if ( (unsigned __int64)a1 >= MmUserProbeAddress )                                               | 
      {                                                                                               | 
        a1 = (_OWORD *)MmUserProbeAddress;                                                            | 
      }                                                                                               | 
      *a1 = v15;                                                                                      | 
      a1[1] = v11;                                                                                    | 
      a1[2] = v12;                                                                                    | 
    }                                                                                                 | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return Message;                                                                                     | 
}                                                                                                     | 
60 row(s)
