__int64 __fastcall NtCreateThreadStateChange(HANDLE *a1, __int64 a2, int a3, ULONG_PTR a4, int a5)          | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  Object = 0LL;                                                                                             | 
  v17[0] = 0LL;                                                                                             | 
  Handle = 0LL;                                                                                             | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                        | 
  if ( PreviousMode )                                                                                       | 
  {                                                                                                         | 
    v8 = 0x7FFFFFFF0000LL;                                                                                  | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                          | 
    {                                                                                                       | 
      v8 = (__int64)a1;                                                                                     | 
    }                                                                                                       | 
    *(_QWORD *)v8 = *(_QWORD *)v8;                                                                          | 
  }                                                                                                         | 
  if ( a5 )                                                                                                 | 
  {                                                                                                         | 
    inserted = -1073741811;                                                                                 | 
  }                                                                                                         | 
  else                                                                                                      | 
  {                                                                                                         | 
    inserted = ObpReferenceObjectByHandleWithTag(a4, 0x63547350u, (__int64)&Object, 0LL, 0LL);              | 
    if ( inserted >= 0 )                                                                                    | 
    {                                                                                                       | 
      LOBYTE(v11) = PreviousMode;                                                                           | 
      LOBYTE(v9) = PreviousMode;                                                                            | 
      inserted = ObCreateObjectEx(v9, PspThreadStateChangeType, a3, v11, Tag, 24, 0, 0, (__int64)v17, 0LL); | 
      if ( inserted >= 0 )                                                                                  | 
      {                                                                                                     | 
        v13 = (PVOID *)v17[0];                                                                              | 
        *(_OWORD *)v17[0] = 0LL;                                                                            | 
        v13[2] = 0LL;                                                                                       | 
        v13[1] = 0LL;                                                                                       | 
        *v13 = Object;                                                                                      | 
        *((_DWORD *)v13 + 4) = 0;                                                                           | 
        Object = 0LL;                                                                                       | 
        inserted = ObInsertObjectEx(v13, 0, 0LL, (__int64)&Handle);                                         | 
        if ( inserted >= 0 )                                                                                | 
        {                                                                                                   | 
          *a1 = Handle;                                                                                     | 
          Handle = 0LL;                                                                                     | 
        }                                                                                                   | 
      }                                                                                                     | 
    }                                                                                                       | 
  }                                                                                                         | 
  if ( Object )                                                                                             | 
  {                                                                                                         | 
    ObfDereferenceObjectWithTag(Object, 0x63547350u);                                                       | 
  }                                                                                                         | 
  if ( Handle )                                                                                             | 
  {                                                                                                         | 
    NtClose(Handle);                                                                                        | 
  }                                                                                                         | 
  return (unsigned int)inserted;                                                                            | 
}                                                                                                           | 
57 row(s)
