__int64 __fastcall NtUserGetThreadState(int a1)                                                                     | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  if ( a1 == 14 )                                                                                                   | 
  {                                                                                                                 | 
    return 1LL;                                                                                                     | 
  }                                                                                                                 | 
  TID = 0LL;                                                                                                        | 
  v4 = EnterSharedCrit(0LL, 1LL);                                                                                   | 
  v5 = v4;                                                                                                          | 
  if ( a1 <= 8 )                                                                                                    | 
  {                                                                                                                 | 
    if ( a1 == 8 )                                                                                                  | 
    {                                                                                                               | 
      v14 = *(_QWORD *)(v4 + 552);                                                                                  | 
      if ( v14 )                                                                                                    | 
      {                                                                                                             | 
        if ( *(_QWORD *)(v14 + 32) )                                                                                | 
        {                                                                                                           | 
          v15 = 1LL;                                                                                                | 
        }                                                                                                           | 
        else                                                                                                        | 
        {                                                                                                           | 
          v15 = 4LL;                                                                                                | 
          if ( (*(_DWORD *)(v14 + 84) & 0x300) == 0 )                                                               | 
          {                                                                                                         | 
            v15 = 2LL;                                                                                              | 
          }                                                                                                         | 
        }                                                                                                           | 
        TID = v15 | 8;                                                                                              | 
        if ( (*(_DWORD *)(v14 + 84) & 1) == 0 )                                                                     | 
        {                                                                                                           | 
          TID = v15;                                                                                                | 
        }                                                                                                           | 
      }                                                                                                             | 
      goto LABEL_54;                                                                                                | 
    }                                                                                                               | 
    if ( a1 )                                                                                                       | 
    {                                                                                                               | 
      v6 = a1 - 1;                                                                                                  | 
      if ( v6 )                                                                                                     | 
      {                                                                                                             | 
        v7 = v6 - 1;                                                                                                | 
        if ( v7 )                                                                                                   | 
        {                                                                                                           | 
          v8 = v7 - 1;                                                                                              | 
          if ( v8 )                                                                                                 | 
          {                                                                                                         | 
            v9 = v8 - 1;                                                                                            | 
            if ( v9 )                                                                                               | 
            {                                                                                                       | 
              v10 = v9 - 1;                                                                                         | 
              if ( v10 )                                                                                            | 
              {                                                                                                     | 
                v11 = v10 - 1;                                                                                      | 
                if ( v11 )                                                                                          | 
                {                                                                                                   | 
                  if ( v11 == 1 )                                                                                   | 
                  {                                                                                                 | 
                    TID = *(_QWORD *)(*(_QWORD *)(v4 + 472) + 520LL);                                               | 
                  }                                                                                                 | 
                }                                                                                                   | 
                else                                                                                                | 
                {                                                                                                   | 
                  MicrosoftTelemetryAssertTriggeredArgsKM("IXPTellMeIf", 0x20000LL, 8604LL);                        | 
                  TID = *(int *)(*(_QWORD *)(v5 + 488) + 4LL);                                                      | 
                }                                                                                                   | 
              }                                                                                                     | 
              else                                                                                                  | 
              {                                                                                                     | 
                LOBYTE(TID) = (GetQueueStatus(7295LL) & 5) != 0;                                                    | 
              }                                                                                                     | 
              goto LABEL_54;                                                                                        | 
            }                                                                                                       | 
            v12 = *(__int64 **)(v4 + 832);                                                                          | 
          }                                                                                                         | 
          else                                                                                                      | 
          {                                                                                                         | 
            v12 = *(__int64 **)(v4 + 824);                                                                          | 
          }                                                                                                         | 
          if ( v12 )                                                                                                | 
          {                                                                                                         | 
            TID = *v12;                                                                                             | 
          }                                                                                                         | 
          goto LABEL_54;                                                                                            | 
        }                                                                                                           | 
        v13 = *(__int64 **)(*(_QWORD *)(v4 + 472) + 112LL);                                                         | 
      }                                                                                                             | 
      else                                                                                                          | 
      {                                                                                                             | 
        v13 = *(__int64 **)(*(_QWORD *)(v4 + 472) + 128LL);                                                         | 
      }                                                                                                             | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v13 = *(__int64 **)(*(_QWORD *)(v4 + 472) + 120LL);                                                           | 
    }                                                                                                               | 
    if ( v13 )                                                                                                      | 
    {                                                                                                               | 
      TID = *v13;                                                                                                   | 
    }                                                                                                               | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v16 = a1 - 9;                                                                                                     | 
  if ( !v16 )                                                                                                       | 
  {                                                                                                                 | 
    TID = *(int *)(v4 + 608);                                                                                       | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v17 = v16 - 1;                                                                                                    | 
  if ( !v17 )                                                                                                       | 
  {                                                                                                                 | 
    LOBYTE(TID) = *(_QWORD *)(v4 + 472) == *(_QWORD *)(W32GetUserSessionState() + 19024);                           | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v18 = v17 - 1;                                                                                                    | 
  if ( !v18 )                                                                                                       | 
  {                                                                                                                 | 
    TID = *(unsigned int *)(*(_QWORD *)(v4 + 464) + 756LL);                                                         | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v19 = v18 - 1;                                                                                                    | 
  if ( !v19 )                                                                                                       | 
  {                                                                                                                 | 
    TID = *(_QWORD *)(v4 + 840);                                                                                    | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v20 = v19 - 1;                                                                                                    | 
  if ( !v20 )                                                                                                       | 
  {                                                                                                                 | 
    v25 = *(HANDLE *)(W32GetUserSessionState() + 63680);                                                            | 
    if ( PsGetCurrentProcessId() == v25 || *(_QWORD *)(v5 + 496) == *(_QWORD *)(W32GetUserSessionState() + 63112) ) | 
    {                                                                                                               | 
      TID = 1LL;                                                                                                    | 
    }                                                                                                               | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  v21 = v20 - 2;                                                                                                    | 
  if ( !v21 )                                                                                                       | 
  {                                                                                                                 | 
    TID = (*(_DWORD *)(v4 + 1360) >> 29) & 1;                                                                       | 
    goto LABEL_54;                                                                                                  | 
  }                                                                                                                 | 
  if ( v21 == 1 )                                                                                                   | 
  {                                                                                                                 | 
    v22 = *(_QWORD *)(v4 + 552);                                                                                    | 
    if ( v22 )                                                                                                      | 
    {                                                                                                               | 
      v23 = *(tagTHREADINFO **)(v22 + 32);                                                                          | 
      if ( v23 )                                                                                                    | 
      {                                                                                                             | 
        TID = tagTHREADINFO::GetTID(v23);                                                                           | 
        goto LABEL_54;                                                                                              | 
      }                                                                                                             | 
      v24 = 50LL;                                                                                                   | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v24 = 5023LL;                                                                                                 | 
    }                                                                                                               | 
    UserSetLastError(v24);                                                                                          | 
  }                                                                                                                 | 
LABEL_54:                                                                                                           | 
  UserSessionSwitchLeaveCrit();                                                                                     | 
  return TID;                                                                                                       | 
}                                                                                                                   | 
167 row(s)
