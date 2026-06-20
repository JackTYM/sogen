__int64 __fastcall NtUserSetProp2(__int64 a1, struct _UNICODE_STRING *a2, __int64 a3)                             | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  v6 = 0;                                                                                                         | 
  EnterLeaveCritShared::EnterLeaveCritShared(v15, 0LL);                                                           | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v15);                                        | 
  v8 = ValidatePropWindow(a1, 3LL);                                                                               | 
  if ( v8 && CaptureAtomName(a2, v16, v7) )                                                                       | 
  {                                                                                                               | 
    v9 = UserGlobalAtomTableCallout();                                                                            | 
    v10 = (struct _RTL_ATOM_TABLE *)v9;                                                                           | 
    if ( !v9 )                                                                                                    | 
    {                                                                                                             | 
      v12 = 5LL;                                                                                                  | 
      goto LABEL_9;                                                                                               | 
    }                                                                                                             | 
    v14 = 0;                                                                                                      | 
    v11 = RtlAddAtomToAtomTableEx(v9, v16, &v14, 0LL);                                                            | 
    if ( v11 < 0                                                                                                  | 
      || (v6 = InternalSetProp(v8, v14, a3, 2LL)) == 0 && (v11 = RtlDeleteAtomFromAtomTable(v10, v14), v11 < 0) ) | 
    {                                                                                                             | 
      v12 = RtlNtStatusToDosError(v11);                                                                           | 
LABEL_9:                                                                                                          | 
      UserSetLastError(v12);                                                                                      | 
    }                                                                                                             | 
  }                                                                                                               | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v15);                                                      | 
  UserSessionSwitchLeaveCrit();                                                                                   | 
  return v6;                                                                                                      | 
}                                                                                                                 | 
31 row(s)
