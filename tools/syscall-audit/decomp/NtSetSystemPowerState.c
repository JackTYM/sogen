__int64 __fastcall NtSetSystemPowerState(int a1, int a2, int a3)                                     | 
{                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                      | 
                                                                                                     | 
  v21 = 0LL;                                                                                         | 
  v16[2] = 0;                                                                                        | 
  v17 = 0LL;                                                                                         | 
  *(_OWORD *)P = 0LL;                                                                                | 
  memset(Event, 0, sizeof(Event));                                                                   | 
  v20 = 0LL;                                                                                         | 
  if ( (unsigned int)(a2 - 1) > 5                                                                    | 
    || (unsigned int)(a1 - 1) > 6                                                                    | 
    || (a3 & 0xCFFFFC0) != 0                                                                         | 
    || a1 < 4 && dword_140F0B054 >= 17 )                                                             | 
  {                                                                                                  | 
    v8 = -1073741811;                                                                                | 
  }                                                                                                  | 
  else                                                                                               | 
  {                                                                                                  | 
    CurrentThread = KeGetCurrentThread();                                                            | 
    PreviousMode = CurrentThread->PreviousMode;                                                      | 
    if ( !PreviousMode || SeSinglePrivilegeCheck(SeShutdownPrivilege, CurrentThread->PreviousMode) ) | 
    {                                                                                                | 
      CurrentServerSilo = PsGetCurrentServerSilo();                                                  | 
      if ( (unsigned __int8)PsIsHostSilo(CurrentServerSilo) )                                        | 
      {                                                                                              | 
        v16[0] = a1;                                                                                 | 
        LOBYTE(v10) = PreviousMode;                                                                  | 
        v16[1] = a3;                                                                                 | 
        *(_QWORD *)&v17 = 0xA000000005LL;                                                            | 
        v12 = PoCaptureReasonContext(0, v10, 0, 0, 0LL, (__int64)P);                                 | 
        v13 = P[0];                                                                                  | 
        if ( v12 < 0 )                                                                               | 
        {                                                                                            | 
          v13 = 0LL;                                                                                 | 
        }                                                                                            | 
        P[0] = v13;                                                                                  | 
        KeInitializeEvent((PRKEVENT)Event, NotificationEvent, 0);                                    | 
        v21 = &v17;                                                                                  | 
        *((_QWORD *)&v17 + 1) = Event;                                                               | 
        PopAcquirePolicyLock();                                                                      | 
        PopExecutePowerAction((unsigned int)&v17, 0, (unsigned int)v16, a2, 1);                      | 
        PopReleasePolicyLock();                                                                      | 
        if ( (_QWORD)v20 )                                                                           | 
        {                                                                                            | 
          KeWaitForSingleObject(Event, Executive, 0, 0, 0LL);                                        | 
          PopAcquirePolicyLock();                                                                    | 
          v14 = v20;                                                                                 | 
          if ( *(__int128 **)(v20 + 8) != &v20 || **((__int128 ***)&v20 + 1) != &v20 )               | 
          {                                                                                          | 
            __fastfail(3u);                                                                          | 
          }                                                                                          | 
          **((_QWORD **)&v20 + 1) = v20;                                                             | 
          *(_QWORD *)(v14 + 8) = *((_QWORD *)&v14 + 1);                                              | 
          PopReleasePolicyLock();                                                                    | 
        }                                                                                            | 
        v8 = *(_DWORD *)&Event[24];                                                                  | 
      }                                                                                              | 
      else if ( (unsigned int)(a1 - 4) > 2 )                                                         | 
      {                                                                                              | 
        v8 = -1073741637;                                                                            | 
      }                                                                                              | 
      else                                                                                           | 
      {                                                                                              | 
        v11 = 2147483697LL;                                                                          | 
        if ( a1 != 5 )                                                                               | 
        {                                                                                            | 
          v11 = 3221226219LL;                                                                        | 
        }                                                                                            | 
        PsTerminateServerSilo(CurrentServerSilo, v11);                                               | 
        v8 = 0;                                                                                      | 
      }                                                                                              | 
    }                                                                                                | 
    else                                                                                             | 
    {                                                                                                | 
      v8 = -1073741727;                                                                              | 
    }                                                                                                | 
  }                                                                                                  | 
  if ( P[0] )                                                                                        | 
  {                                                                                                  | 
    PoDestroyReasonContext(P[0]);                                                                    | 
  }                                                                                                  | 
  return v8;                                                                                         | 
}                                                                                                    | 
84 row(s)
