__int64 __fastcall NtUnmapViewOfSection(ULONG_PTR a1, unsigned __int64 a2)                 | 
{                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                            | 
                                                                                           | 
  Object = 0LL;                                                                            | 
  if ( KeGetCurrentThread()->PreviousMode == 1 && a2 > 0x7FFFFFFEFFFFLL )                  | 
  {                                                                                        | 
    return 3221225497LL;                                                                   | 
  }                                                                                        | 
  result = ObpReferenceObjectByHandleWithTag(a1, 0x77566D4Du, (__int64)&Object, 0LL, 0LL); | 
  if ( (int)result >= 0 )                                                                  | 
  {                                                                                        | 
    v3 = MiUnmapViewOfSection((PRKPROCESS)Object);                                         | 
    ObfDereferenceObjectWithTag(Object, 0x77566D4Du);                                      | 
    return v3;                                                                             | 
  }                                                                                        | 
  return result;                                                                           | 
}                                                                                          | 
