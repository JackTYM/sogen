__int64 __fastcall NtAlertThread(ULONG_PTR a1)                                             | 
{                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                            | 
                                                                                           | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                       | 
  Object = 0LL;                                                                            | 
  result = ObpReferenceObjectByHandleWithTag(a1, 0x75537350u, (__int64)&Object, 0LL, 0LL); | 
  if ( (int)result >= 0 )                                                                  | 
  {                                                                                        | 
    LOBYTE(v3) = PreviousMode;                                                             | 
    KeAlertThread(Object, v3);                                                             | 
    ObfDereferenceObjectWithTag(Object, 0x75537350u);                                      | 
    return 0LL;                                                                            | 
  }                                                                                        | 
  return result;                                                                           | 
}                                                                                          | 
16 row(s)
