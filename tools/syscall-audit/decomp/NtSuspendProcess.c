__int64 __fastcall NtSuspendProcess(ULONG_PTR a1)                                      | 
{                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                        | 
                                                                                       | 
  Object = 0LL;                                                                        | 
  v1 = ObpReferenceObjectByHandleWithTag(a1, 0x75537350u, (__int64)&Object, 0LL, 0LL); | 
  if ( v1 >= 0 )                                                                       | 
  {                                                                                    | 
    v1 = PsSuspendProcess(Object);                                                     | 
    ObfDereferenceObjectWithTag(Object, 0x75537350u);                                  | 
  }                                                                                    | 
  return (unsigned int)v1;                                                             | 
}                                                                                      | 
13 row(s)
