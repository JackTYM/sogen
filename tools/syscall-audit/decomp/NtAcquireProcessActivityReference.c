__int64 __fastcall NtAcquireProcessActivityReference(__int64 a1, ULONG_PTR a2, int a3)       | 
{                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                              | 
                                                                                             | 
  v4 = (_QWORD *)a1;                                                                         | 
  v9[0] = 0LL;                                                                               | 
  Object = 0LL;                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                         | 
  if ( PreviousMode )                                                                        | 
  {                                                                                          | 
    a1 = 0x7FFFFFFF0000LL;                                                                   | 
    if ( (unsigned __int64)v4 < 0x7FFFFFFF0000LL )                                           | 
    {                                                                                        | 
      a1 = (__int64)v4;                                                                      | 
    }                                                                                        | 
    *(_QWORD *)a1 = *(_QWORD *)a1;                                                           | 
  }                                                                                          | 
  if ( a3 )                                                                                  | 
  {                                                                                          | 
    return 3221225713LL;                                                                     | 
  }                                                                                          | 
  LOBYTE(a1) = PreviousMode;                                                                 | 
  result = ExCpuSetResourceManagerAccessCheck(a1);                                           | 
  if ( (int)result >= 0 )                                                                    | 
  {                                                                                          | 
    result = ObpReferenceObjectByHandleWithTag(a2, 0x63417350u, (__int64)&Object, 0LL, 0LL); | 
    if ( (int)result >= 0 )                                                                  | 
    {                                                                                        | 
      v7 = Object;                                                                           | 
      ActivityReference = PspCreateActivityReference(Object, v9);                            | 
      if ( ActivityReference >= 0 )                                                          | 
      {                                                                                      | 
        *v4 = v9[0];                                                                         | 
      }                                                                                      | 
      ObfDereferenceObjectWithTag(v7, 0x63417350u);                                          | 
      return (unsigned int)ActivityReference;                                                | 
    }                                                                                        | 
  }                                                                                          | 
  return result;                                                                             | 
}                                                                                            | 
40 row(s)
