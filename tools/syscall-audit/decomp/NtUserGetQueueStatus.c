__int64 __fastcall NtUserGetQueueStatus(__int16 a1)                                                      | 
{                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                          | 
                                                                                                         | 
  EnterLeaveCritShared::EnterLeaveCritShared(v27, 1LL);                                                  | 
  CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged();                                      | 
  v3 = 0;                                                                                                | 
  if ( CurrentThreadNonPaged )                                                                           | 
  {                                                                                                      | 
    v4 = *CurrentThreadNonPaged;                                                                         | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v4 = 0LL;                                                                                            | 
  }                                                                                                      | 
  for ( i = 0; i < 2; ++i )                                                                              | 
  {                                                                                                      | 
    if ( *(_QWORD *)(v4 + 8LL * i + 1656) )                                                              | 
    {                                                                                                    | 
      if ( (a1 & 8) != 0 )                                                                               | 
      {                                                                                                  | 
        v6 = (const struct tagTHREADINFO **)W32GetCurrentThreadNonPaged();                               | 
        if ( v6 )                                                                                        | 
        {                                                                                                | 
          v7 = *v6;                                                                                      | 
        }                                                                                                | 
        else                                                                                             | 
        {                                                                                                | 
          v7 = 0LL;                                                                                      | 
        }                                                                                                | 
        v24 = 0;                                                                                         | 
        memset(&v26, 0, sizeof(v26));                                                                    | 
        v25 = 0LL;                                                                                       | 
        v8 = 0;                                                                                          | 
        ZwSetIoCompletionEx(*((_QWORD *)v7 + 202), *((_QWORD *)v7 + 206), 0LL, -2147483647LL, 0, 0LL);   | 
        while ( 1 )                                                                                      | 
        {                                                                                                | 
          LOBYTE(v23) = 0;                                                                               | 
          if ( (unsigned int)ZwRemoveIoCompletionEx(*((_QWORD *)v7 + 202), &v26, 1LL, &v24, &v25, v23) ) | 
          {                                                                                              | 
            break;                                                                                       | 
          }                                                                                              | 
          if ( ((__int64)v26.ApcContext & 0x80000000) != 0 )                                             | 
          {                                                                                              | 
            if ( LODWORD(v26.ApcContext) != 0x80000000 )                                                 | 
            {                                                                                            | 
              break;                                                                                     | 
            }                                                                                            | 
            v8 = 1;                                                                                      | 
          }                                                                                              | 
          else                                                                                           | 
          {                                                                                              | 
            if ( v26.ApcContext >= (PVOID)2                                                              | 
              || (v22 = (struct tagWND *)*((_QWORD *)v7 + (__int64)v26.ApcContext + 207)) == 0LL )       | 
            {                                                                                            | 
              BroadcastCoreMessagingApcContextError(v7, (unsigned __int64)v26.ApcContext);               | 
              break;                                                                                     | 
            }                                                                                            | 
            xxxHandleCoreMessagingQueueCompletion(v22, &v26, 0x80000000uLL);                             | 
          }                                                                                              | 
        }                                                                                                | 
        if ( v8 )                                                                                        | 
        {                                                                                                | 
          ZwAssociateWaitCompletionPacket(                                                               | 
            *((_QWORD *)v7 + 205),                                                                       | 
            *((_QWORD *)v7 + 202),                                                                       | 
            *((_QWORD *)v7 + 204),                                                                       | 
            0LL,                                                                                         | 
            0xFFFFFFFF80000000uLL,                                                                       | 
            0,                                                                                           | 
            0LL,                                                                                         | 
            0LL);                                                                                        | 
        }                                                                                                | 
      }                                                                                                  | 
      break;                                                                                             | 
    }                                                                                                    | 
  }                                                                                                      | 
  v9 = a1 & 0x5DFF;                                                                                      | 
  DLT = DLT_THREADINFO::getDLT();                                                                        | 
  DomainLockRef = (tagDomLock *)GetDomainLockRef(DLT);                                                   | 
  v29 = 0;                                                                                               | 
  UserSessionState = W32GetUserSessionState();                                                           | 
  v31 = 0;                                                                                               | 
  v33 = 0;                                                                                               | 
  v30 = UserSessionState + 42520;                                                                        | 
  v32 = 0LL;                                                                                             | 
  v12 = W32GetUserSessionState();                                                                        | 
  v13 = 0LL;                                                                                             | 
  if ( v4 != v12 + 42184 )                                                                               | 
  {                                                                                                      | 
    v13 = v4 + 432;                                                                                      | 
  }                                                                                                      | 
  *(_QWORD *)&v32 = v13;                                                                                 | 
  if ( !v33 )                                                                                            | 
  {                                                                                                      | 
    v14 = 0;                                                                                             | 
    v15 = &v29;                                                                                          | 
    do                                                                                                   | 
    {                                                                                                    | 
      v16 = (tagDomLock *)*((_QWORD *)v15 - 1);                                                          | 
      if ( v16 )                                                                                         | 
      {                                                                                                  | 
        if ( *v15 )                                                                                      | 
        {                                                                                                | 
          tagDomLock::LockExclusive(v16);                                                                | 
        }                                                                                                | 
        else                                                                                             | 
        {                                                                                                | 
          tagDomLock::LockShared(v16);                                                                   | 
        }                                                                                                | 
      }                                                                                                  | 
      ++v14;                                                                                             | 
      v15 += 16;                                                                                         | 
    }                                                                                                    | 
    while ( !v14 );                                                                                      | 
    v17 = (tagObjLock **)&v32;                                                                           | 
    do                                                                                                   | 
    {                                                                                                    | 
      if ( *v17 )                                                                                        | 
      {                                                                                                  | 
        tagObjLock::LockExclusive(*v17);                                                                 | 
      }                                                                                                  | 
      ++v3;                                                                                              | 
      ++v17;                                                                                             | 
    }                                                                                                    | 
    while ( !v3 );                                                                                       | 
    v33 = 1;                                                                                             | 
  }                                                                                                      | 
  INTERLOCKEDINT::operator int(*(_QWORD *)(v4 + 488) + 4LL);                                             | 
  _InterlockedAnd((volatile signed __int32 *)(*(_QWORD *)(v4 + 488) + 4LL), ~v9);                        | 
  v18 = INTERLOCKEDINT::operator int(*(_QWORD *)(v4 + 488) + 8LL);                                       | 
  v20 = (unsigned __int16)(v9 & v19) | ((unsigned __int16)(v9 & v18) << 16);                             | 
  if ( v33 )                                                                                             | 
  {                                                                                                      | 
    if ( (_QWORD)v32 )                                                                                   | 
    {                                                                                                    | 
      tagObjLock::UnLockExclusive((tagObjLock *)v32);                                                    | 
    }                                                                                                    | 
    if ( DomainLockRef )                                                                                 | 
    {                                                                                                    | 
      if ( v29 )                                                                                         | 
      {                                                                                                  | 
        tagDomLock::UnLockExclusive(DomainLockRef);                                                      | 
      }                                                                                                  | 
      else                                                                                               | 
      {                                                                                                  | 
        tagDomLock::UnLockShared(DomainLockRef);                                                         | 
      }                                                                                                  | 
    }                                                                                                    | 
  }                                                                                                      | 
  UserSessionSwitchLeaveCrit();                                                                          | 
  return v20;                                                                                            | 
}                                                                                                        | 
153 row(s)
