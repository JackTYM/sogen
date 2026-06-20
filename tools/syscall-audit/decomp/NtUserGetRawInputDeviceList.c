__int64 __fastcall NtUserGetRawInputDeviceList(volatile void *a1, unsigned int *a2, int a3)                                | 
{                                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                            | 
                                                                                                                           | 
  v5 = 0;                                                                                                                  | 
  v6 = -1;                                                                                                                 | 
  EnterLeaveCritShared::EnterLeaveCritShared(v16, 1LL);                                                                    | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v18);                                                 | 
  if ( a3 == 16 )                                                                                                          | 
  {                                                                                                                        | 
    UserSessionState = W32GetUserSessionState();                                                                           | 
    Lock = CBaseInput::TmpGetLock(*(CBaseInput **)(UserSessionState + 16840));                                             | 
    W32AcquirePushLockSharedEx(Lock, 0);                                                                                   | 
    v9 = W32GetUserSessionState();                                                                                         | 
    for ( i = CBaseInput::TmpGetDeviceList(*(CBaseInput **)(v9 + 16840)); i; i = (struct DEVICEINFO *)*((_QWORD *)i + 7) ) | 
    {                                                                                                                      | 
      ++v5;                                                                                                                | 
    }                                                                                                                      | 
    if ( a1 )                                                                                                              | 
    {                                                                                                                      | 
      v11 = 0;                                                                                                             | 
      v12 = a2;                                                                                                            | 
      if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                                                    | 
      {                                                                                                                    | 
        v12 = (unsigned int *)MmUserProbeAddress;                                                                          | 
      }                                                                                                                    | 
      if ( *v12 < v5 )                                                                                                     | 
      {                                                                                                                    | 
        if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                                                  | 
        {                                                                                                                  | 
          a2 = (unsigned int *)MmUserProbeAddress;                                                                         | 
        }                                                                                                                  | 
        *a2 = v5;                                                                                                          | 
        UserSetLastError(122LL);                                                                                           | 
      }                                                                                                                    | 
      else                                                                                                                 | 
      {                                                                                                                    | 
        ProbeForWrite(a1, 16LL * v5, 8u);                                                                                  | 
        v17 = *(CBaseInput **)(W32GetUserSessionState() + 16840);                                                          | 
        for ( j = CBaseInput::TmpGetDeviceList(v17); j; j = (struct DEVICEINFO *)*((_QWORD *)j + 7) )                      | 
        {                                                                                                                  | 
          v14 = 2LL * v11;                                                                                                 | 
          *((_QWORD *)a1 + v14) = *(_QWORD *)j;                                                                            | 
          *((_DWORD *)a1 + 2 * v14 + 2) = *((_DWORD *)j + 12);                                                             | 
          if ( ++v11 >= v5 )                                                                                               | 
          {                                                                                                                | 
            break;                                                                                                         | 
          }                                                                                                                | 
        }                                                                                                                  | 
        v6 = v11;                                                                                                          | 
      }                                                                                                                    | 
    }                                                                                                                      | 
    else                                                                                                                   | 
    {                                                                                                                      | 
      if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                                                    | 
      {                                                                                                                    | 
        a2 = (unsigned int *)MmUserProbeAddress;                                                                           | 
      }                                                                                                                    | 
      *a2 = v5;                                                                                                            | 
      v6 = 0;                                                                                                              | 
    }                                                                                                                      | 
    W32ReleasePushLockSharedEx(Lock, 0);                                                                                   | 
  }                                                                                                                        | 
  else                                                                                                                     | 
  {                                                                                                                        | 
    UserSetLastError(87LL);                                                                                                | 
  }                                                                                                                        | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v18);                                                               | 
  UserSessionSwitchLeaveCrit();                                                                                            | 
  return v6;                                                                                                               | 
}                                                                                                                          | 
71 row(s)
