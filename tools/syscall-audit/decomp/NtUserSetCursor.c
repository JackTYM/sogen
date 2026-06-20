__int64 __fastcall NtUserSetCursor(__int64 a1)                                                                        | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  EnterCrit(0LL, 0LL);                                                                                                | 
  v2 = 0LL;                                                                                                           | 
  v3 = 0LL;                                                                                                           | 
  if ( a1 )                                                                                                           | 
  {                                                                                                                   | 
    W32GetCurrentThreadNonPaged();                                                                                    | 
    if ( (unsigned __int64)(unsigned __int16)a1 >= *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 20016) + 8LL) ) | 
    {                                                                                                                 | 
      goto LABEL_22;                                                                                                  | 
    }                                                                                                                 | 
    UserSessionState = W32GetUserSessionState();                                                                      | 
    v5 = *(_DWORD *)(W32GetUserSessionState() + 20040) * (unsigned int)(unsigned __int16)a1                           | 
       + *(_QWORD *)(UserSessionState + 20032);                                                                       | 
    v7 = HMPkheFromPhe(v5);                                                                                           | 
    v8 = WORD1(a1) & 0x7FFF;                                                                                          | 
    if ( (WORD1(a1) & 0x7FFF) != *(_WORD *)(v5 + 26) && v8 != 0x7FFF && (v8 || !PsGetCurrentProcessWow64Process()) )  | 
    {                                                                                                                 | 
      goto LABEL_22;                                                                                                  | 
    }                                                                                                                 | 
    v9 = *(_BYTE *)(v5 + 25);                                                                                         | 
    if ( (v9 & 1) == 0 && *(_BYTE *)(v5 + 24) == 3 )                                                                  | 
    {                                                                                                                 | 
      v3 = *(_QWORD *)v7;                                                                                             | 
      if ( (v9 & 4) != 0 )                                                                                            | 
      {                                                                                                               | 
        LOBYTE(v6) = 3;                                                                                               | 
        v3 &= -(__int64)((unsigned int)HMSDCheck(a1, v6, 0LL) != 0);                                                  | 
      }                                                                                                               | 
    }                                                                                                                 | 
    if ( (*(_BYTE *)(v5 + 25) & 4) == 0 )                                                                             | 
    {                                                                                                                 | 
      CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged();                                               | 
      if ( CurrentThreadNonPaged )                                                                                    | 
      {                                                                                                               | 
        v11 = *CurrentThreadNonPaged;                                                                                 | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        v11 = 0LL;                                                                                                    | 
      }                                                                                                               | 
      v12 = 3LL;                                                                                                      | 
      if ( (_InterlockedCompareExchange((volatile signed __int32 *)(v11 + 528), 0, 0) & 0x20000000) == 0 )            | 
      {                                                                                                               | 
        v12 = 2LL;                                                                                                    | 
      }                                                                                                               | 
      v3 &= -(__int64)((unsigned int)ValidateHandleSecure(a1, v12) != 0);                                             | 
      if ( *(char *)(v5 + 25) < 0 )                                                                                   | 
      {                                                                                                               | 
        CurrentProcessWin32Process = (_QWORD *)PsGetCurrentProcessWin32Process();                                     | 
        v16 = CurrentProcessWin32Process;                                                                             | 
        if ( CurrentProcessWin32Process )                                                                             | 
        {                                                                                                             | 
          if ( *CurrentProcessWin32Process )                                                                          | 
          {                                                                                                           | 
            v17 = gahti[12 * *(unsigned __int8 *)(v5 + 24) + 6];                                                      | 
            if ( (v17 & 2) != 0 )                                                                                     | 
            {                                                                                                         | 
              v18 = *(_QWORD **)(v7 + 8);                                                                             | 
LABEL_30:                                                                                                             | 
              if ( v18 && v18 != v16 )                                                                                | 
              {                                                                                                       | 
                goto LABEL_22;                                                                                        | 
              }                                                                                                       | 
              goto LABEL_14;                                                                                          | 
            }                                                                                                         | 
            if ( (v17 & 1) != 0 )                                                                                     | 
            {                                                                                                         | 
              v19 = *(_QWORD *)(v7 + 8);                                                                              | 
              if ( v19 )                                                                                              | 
              {                                                                                                       | 
                v18 = *(_QWORD **)(v19 + 464);                                                                        | 
                goto LABEL_30;                                                                                        | 
              }                                                                                                       | 
            }                                                                                                         | 
          }                                                                                                           | 
        }                                                                                                             | 
      }                                                                                                               | 
    }                                                                                                                 | 
LABEL_14:                                                                                                             | 
    if ( v3 )                                                                                                         | 
    {                                                                                                                 | 
      goto LABEL_15;                                                                                                  | 
    }                                                                                                                 | 
LABEL_22:                                                                                                             | 
    UserSetLastError(1402LL);                                                                                         | 
    goto LABEL_17;                                                                                                    | 
  }                                                                                                                   | 
LABEL_15:                                                                                                             | 
  v13 = zzzSetCursor((struct tagCURSOR *)v3);                                                                         | 
  if ( v13 )                                                                                                          | 
  {                                                                                                                   | 
    v2 = *(_QWORD *)v13;                                                                                              | 
  }                                                                                                                   | 
LABEL_17:                                                                                                             | 
  UserSessionSwitchLeaveCrit();                                                                                       | 
  return v2;                                                                                                          | 
}                                                                                                                     | 
101 row(s)
