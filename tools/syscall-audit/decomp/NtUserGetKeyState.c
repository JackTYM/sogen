__int64 __fastcall NtUserGetKeyState(unsigned int a1)                                           | 
{                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                 | 
                                                                                                | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v24, 1LL);                                        | 
  v2 = v24;                                                                                     | 
  v3 = v24 + 472;                                                                               | 
  if ( (*(_DWORD *)(*(_QWORD *)(v24 + 472) + 508LL) & 1) != 0 )                                 | 
  {                                                                                             | 
    UserSessionState = W32GetUserSessionState();                                                | 
    CurrentThreadWin32Thread = (_DWORD *)PsGetCurrentThreadWin32Thread();                       | 
    if ( (CurrentThreadWin32Thread[6] & 0xC) == 8 )                                             | 
    {                                                                                           | 
      v6 = *(void (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 48) + 3944LL);        | 
      if ( v6 )                                                                                 | 
      {                                                                                         | 
        v6();                                                                                   | 
      }                                                                                         | 
      *(_DWORD *)(UserSessionState + 19932) = 0;                                                | 
      *(_QWORD *)(UserSessionState + 19912) = 0LL;                                              | 
      DestroyExclusiveUserCritDeferredUnlockList();                                             | 
      *(_QWORD *)(UserSessionState + 16) = 0LL;                                                 | 
    }                                                                                           | 
    if ( *(_QWORD *)CurrentThreadWin32Thread && (CurrentThreadWin32Thread[6] & 2) == 0 )        | 
    {                                                                                           | 
      *(_BYTE *)(*(_QWORD *)CurrentThreadWin32Thread + 1708LL) = 0;                             | 
    }                                                                                           | 
    v7 = PsGetCurrentThreadWin32Thread();                                                       | 
    if ( v7 )                                                                                   | 
    {                                                                                           | 
      v8 = *(_DWORD *)(v7 + 28);                                                                | 
      if ( v8 )                                                                                 | 
      {                                                                                         | 
        KeBugCheckEx(0x160u, v8, 0LL, 0LL, 0LL);                                                | 
      }                                                                                         | 
    }                                                                                           | 
    EtwTraceReleaseUserCrit();                                                                  | 
    v9 = CurrentThreadWin32Thread[6] & 0xFFFFFFF1;                                              | 
    CurrentThreadWin32Thread[6] = v9;                                                           | 
    if ( (v9 & 0x10) != 0 )                                                                     | 
    {                                                                                           | 
      PsSetThreadWin32Thread(KeGetCurrentThread(), 0LL, CurrentThreadWin32Thread);              | 
      v2 = v24;                                                                                 | 
    }                                                                                           | 
    ExReleaseFastResource(*(_QWORD *)UserSessionState, CurrentThreadWin32Thread + 8);           | 
    KeLeaveCriticalRegion();                                                                    | 
    v10 = W32GetUserSessionState();                                                             | 
    v11 = PsGetCurrentThreadWin32Thread();                                                      | 
    KeEnterCriticalRegion();                                                                    | 
    _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_(                          | 
      *(struct _FAST_ERESOURCE **)v10,                                                          | 
      (struct _W32THREADNONPAGED *)v11);                                                        | 
    if ( *(_QWORD *)v11 )                                                                       | 
    {                                                                                           | 
      if ( (unsigned int)IsThreadCrossSessionAttached() )                                       | 
      {                                                                                         | 
        *(_DWORD *)(v11 + 24) |= 2u;                                                            | 
        v12 = 0LL;                                                                              | 
      }                                                                                         | 
      else                                                                                      | 
      {                                                                                         | 
        v12 = *(_QWORD *)v11;                                                                   | 
        *(_BYTE *)(*(_QWORD *)v11 + 1708LL) = 1;                                                | 
      }                                                                                         | 
    }                                                                                           | 
    else                                                                                        | 
    {                                                                                           | 
      v12 = 0LL;                                                                                | 
    }                                                                                           | 
    *(_QWORD *)(v10 + 16) = v12;                                                                | 
    if ( v12 && (unsigned __int8)UserCritInternal::_anonymous_namespace_::IsValidGuiContext() ) | 
    {                                                                                           | 
      DestroySharedUserCritDeferredUnlockList(v10 + 19832);                                     | 
      DestroyDeferredUnlockObjectAssignmentList(v10 + 19888);                                   | 
      DestroyDeferredUnlockObjectAssignmentList(v10 + 19872);                                   | 
    }                                                                                           | 
    if ( (*(_DWORD *)(*(_QWORD *)v3 + 508LL) & 1) != 0 )                                        | 
    {                                                                                           | 
      PostUpdateKeyStateEvent();                                                                | 
    }                                                                                           | 
  }                                                                                             | 
  if ( (unsigned __int8)a1 >= 0x20u )                                                           | 
  {                                                                                             | 
    v13 = 0;                                                                                    | 
    v14 = &unk_14025DC58;                                                                       | 
    while ( *v14 != (_BYTE)a1 )                                                                 | 
    {                                                                                           | 
      ++v13;                                                                                    | 
      ++v14;                                                                                    | 
      if ( v13 >= 0xE )                                                                         | 
      {                                                                                         | 
        switch ( (char)a1 )                                                                     | 
        {                                                                                       | 
          case 91:                                                                              | 
          case 92:                                                                              | 
          case -96:                                                                             | 
          case -95:                                                                             | 
          case -94:                                                                             | 
          case -93:                                                                             | 
          case -92:                                                                             | 
          case -91:                                                                             | 
            goto LABEL_33;                                                                      | 
          default:                                                                              | 
            if ( (unsigned int)ApiSetEditionIsGetKeyStateBlocked() )                            | 
            {                                                                                   | 
              KeyState = 0;                                                                     | 
              goto LABEL_37;                                                                    | 
            }                                                                                   | 
            if ( (unsigned int)ApiSetEditionIsGpqForegroundAccessibleCurrent(1LL) )             | 
            {                                                                                   | 
              goto LABEL_33;                                                                    | 
            }                                                                                   | 
            v16 = *(_QWORD *)(W32GetUserSessionState() + 19024);                                | 
            v25 = *(_QWORD *)(v16 + 544);                                                       | 
            v17 = *(_DWORD *)(v16 + 552);                                                       | 
            v18 = *(_QWORD *)(W32GetUserSessionState() + 19024);                                | 
            v22 = v25;                                                                          | 
            v23 = v17;                                                                          | 
            EtwTraceUIPIInputError(v2, 0, v18, (unsigned int)&v22, 3);                          | 
            KeyState = 0;                                                                       | 
            break;                                                                              | 
        }                                                                                       | 
        goto LABEL_37;                                                                          | 
      }                                                                                         | 
    }                                                                                           | 
  }                                                                                             | 
LABEL_33:                                                                                       | 
  if ( (*(_DWORD *)(*(_QWORD *)(W32GetUserSessionState() + 19744) + 8LL) & 1) != 0              | 
    && (ThreadProcessId = (unsigned int)PsGetThreadProcessId(*(PETHREAD *)v2),                  | 
        (ThreadProcessId & 0xFFFFFFFC) == **(_DWORD **)(W32GetUserSessionState() + 19744)) )    | 
  {                                                                                             | 
    KeyState = 0;                                                                               | 
  }                                                                                             | 
  else                                                                                          | 
  {                                                                                             | 
    KeyState = GetKeyState(a1);                                                                 | 
  }                                                                                             | 
LABEL_37:                                                                                       | 
  if ( (*(_DWORD *)(*(_QWORD *)(W32GetUserSessionState() + 19744) + 8LL) & 1) != 0              | 
    && (v20 = (unsigned int)PsGetThreadProcessId(*(PETHREAD *)v2),                              | 
        (v20 & 0xFFFFFFFC) == **(_DWORD **)(W32GetUserSessionState() + 19744)) )                | 
  {                                                                                             | 
    *(_DWORD *)(*(_QWORD *)(v2 + 520) + 112LL) = 0;                                             | 
    *(_QWORD *)(*(_QWORD *)(v24 + 520) + 116LL) = 0LL;                                          | 
  }                                                                                             | 
  else                                                                                          | 
  {                                                                                             | 
    v26 = *(_QWORD *)(W32GetUserSessionState() + 20016);                                        | 
    *(_DWORD *)(*(_QWORD *)(v2 + 520) + 112LL) = *(_DWORD *)(v26 + 6984);                       | 
    *(_QWORD *)(*(_QWORD *)(v24 + 520) + 116LL) = *(_QWORD *)(*(_QWORD *)(v24 + 472) + 352LL);  | 
  }                                                                                             | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                     | 
  return KeyState;                                                                              | 
}                                                                                               | 
154 row(s)
