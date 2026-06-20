__int64 __fastcall NtDuplicateObject(                                                                        | 
        ULONG_PTR BugCheckParameter1,                                                                        | 
        int a2,                                                                                              | 
        ULONG_PTR a3,                                                                                        | 
        _QWORD *a4,                                                                                          | 
        int a5,                                                                                              | 
        int a6,                                                                                              | 
        int a7)                                                                                              | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v11 = 0LL;                                                                                                 | 
  v22 = 0LL;                                                                                                 | 
  Object = 0LL;                                                                                              | 
  v20 = 0LL;                                                                                                 | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                         | 
  if ( a4 && PreviousMode )                                                                                  | 
  {                                                                                                          | 
    v13 = 0x7FFFFFFF0000LL;                                                                                  | 
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                        | 
      v13 = (__int64)a4;                                                                                     | 
    }                                                                                                        | 
    *(_QWORD *)v13 = *(_QWORD *)v13;                                                                         | 
    *a4 = 0LL;                                                                                               | 
  }                                                                                                          | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x7544624Fu, (__int64)&Object, 0LL, 0LL);   | 
  if ( (int)result >= 0 )                                                                                    | 
  {                                                                                                          | 
    if ( a3 )                                                                                                | 
    {                                                                                                        | 
      v15 = ObpReferenceObjectByHandleWithTag(a3, 0x7544624Fu, (__int64)&v20, 0LL, 0LL);                     | 
      if ( v15 < 0 )                                                                                         | 
      {                                                                                                      | 
        v20 = 0LL;                                                                                           | 
        goto LABEL_10;                                                                                       | 
      }                                                                                                      | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      v15 = 0;                                                                                               | 
    }                                                                                                        | 
    v11 = v20;                                                                                               | 
LABEL_10:                                                                                                    | 
    v16 = v15;                                                                                               | 
    v17 = a2;                                                                                                | 
    v18 = Object;                                                                                            | 
    v19 = ObDuplicateObject((_DWORD)Object, v17, (_DWORD)v11, (unsigned int)&v22, a5, a6, a7, PreviousMode); | 
    if ( a4 )                                                                                                | 
    {                                                                                                        | 
      *a4 = v22;                                                                                             | 
    }                                                                                                        | 
    ObfDereferenceObjectWithTag(v18, 0x7544624Fu);                                                           | 
    if ( v11 )                                                                                               | 
    {                                                                                                        | 
      ObfDereferenceObjectWithTag(v11, 0x7544624Fu);                                                         | 
    }                                                                                                        | 
    if ( v15 >= 0 )                                                                                          | 
    {                                                                                                        | 
      return v19;                                                                                            | 
    }                                                                                                        | 
    return v16;                                                                                              | 
  }                                                                                                          | 
  return result;                                                                                             | 
}                                                                                                            | 
