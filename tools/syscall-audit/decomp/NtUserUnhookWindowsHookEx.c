__int64 __fastcall NtUserUnhookWindowsHookEx(__int64 a1)                      | 
{                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]               | 
                                                                              | 
  EnterLeaveCritShared::EnterLeaveCritShared(v7, 1LL);                        | 
  DomainShared<>::DomainExclusive<DLT_HOOK>::ObjectLock<>::ObjectLock<>(&v8); | 
  LOBYTE(v2) = 5;                                                             | 
  v3 = (struct tagHOOK *)HMValidateHandleWithDescriptor(a1, v2);              | 
  v4 = 0LL;                                                                   | 
  if ( v3 )                                                                   | 
  {                                                                           | 
    v5 = (int)zzzUnhookWindowsHookEx(v3);                                     | 
    if ( v10 )                                                                | 
    {                                                                         | 
      if ( v8 )                                                               | 
      {                                                                       | 
        if ( v9 )                                                             | 
        {                                                                     | 
          tagDomLock::UnLockExclusive(v8);                                    | 
        }                                                                     | 
        else                                                                  | 
        {                                                                     | 
          tagDomLock::UnLockShared(v8);                                       | 
        }                                                                     | 
      }                                                                       | 
      v10 = 0;                                                                | 
    }                                                                         | 
    v4 = v5;                                                                  | 
  }                                                                           | 
  else if ( v10 )                                                             | 
  {                                                                           | 
    if ( v8 )                                                                 | 
    {                                                                         | 
      if ( v9 )                                                               | 
      {                                                                       | 
        tagDomLock::UnLockExclusive(v8);                                      | 
      }                                                                       | 
      else                                                                    | 
      {                                                                       | 
        tagDomLock::UnLockShared(v8);                                         | 
      }                                                                       | 
    }                                                                         | 
    v10 = 0;                                                                  | 
  }                                                                           | 
  UserSessionSwitchLeaveCrit();                                               | 
  return v4;                                                                  | 
}                                                                             | 
47 row(s)
