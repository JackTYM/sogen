__int64 __fastcall NtUserDestroyMenu(__int64 a1)                                | 
{                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                 | 
                                                                                | 
  EnterCrit(0LL, 0LL);                                                          | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v7);       | 
  SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v8);                     | 
  v2 = ValidateHmenu(a1, 32LL);                                                 | 
  v3 = 0;                                                                       | 
  v9 = 0LL;                                                                     | 
  v4 = v2;                                                                      | 
  if ( v2 != *(_QWORD *)v8[0] )                                                 | 
  {                                                                             | 
    SmartObjStackRefBase<tagMENU>::DecrementCountAndTryFree(v8);                | 
    if ( v4 )                                                                   | 
    {                                                                           | 
      v8[0] = *(_QWORD *)(v4 + 152);                                            | 
      ++*(_DWORD *)(v8[0] + 8LL);                                               | 
    }                                                                           | 
    else                                                                        | 
    {                                                                           | 
      v8[0] = gSmartObjNullRef;                                                 | 
    }                                                                           | 
  }                                                                             | 
  if ( !(unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v8)              | 
    && (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v8[0] + 40LL) + 40LL) & 0x40) == 0 ) | 
  {                                                                             | 
    v5 = v9;                                                                    | 
    if ( !v9 )                                                                  | 
    {                                                                           | 
      v5 = *(struct _HEAD **)v8[0];                                             | 
    }                                                                           | 
    v3 = DestroyMenu(v5);                                                       | 
  }                                                                             | 
  SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v8);                    | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v7);                     | 
  UserSessionSwitchLeaveCrit();                                                 | 
  return v3;                                                                    | 
}                                                                               | 
39 row(s)
