NTSTATUS __fastcall NtMakeTemporaryObject(HANDLE Handle)                                                       | 
{                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                | 
                                                                                                               | 
  v5 = 0LL;                                                                                                    | 
  Object = 0LL;                                                                                                | 
  result = ObReferenceObjectByHandle(Handle, 0x10000u, 0LL, KeGetCurrentThread()->PreviousMode, &Object, &v5); | 
  v3 = result;                                                                                                 | 
  if ( result >= 0 )                                                                                           | 
  {                                                                                                            | 
    ObMakeTemporaryObject(Object);                                                                             | 
    if ( (v5.HandleAttributes & 4) != 0 )                                                                      | 
    {                                                                                                          | 
      SeDeleteObjectAuditAlarmWithTransaction(Object, Handle, 0LL);                                            | 
    }                                                                                                          | 
    ObfDereferenceObject(Object);                                                                              | 
    return v3;                                                                                                 | 
  }                                                                                                            | 
  return result;                                                                                               | 
}                                                                                                              | 
20 row(s)
