__int64 __fastcall NtQuerySemaphore(HANDLE Handle, int a2, unsigned __int64 a3, int a4, unsigned __int64 a5)    | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                            | 
  if ( PreviousMode )                                                                                           | 
  {                                                                                                             | 
    if ( (a3 & 3) != 0 )                                                                                        | 
    {                                                                                                           | 
      ExRaiseDatatypeMisalignment();                                                                            | 
    }                                                                                                           | 
    v9 = 0x7FFFFFFF0000LL;                                                                                      | 
    v10 = 0x7FFFFFFF0000LL;                                                                                     | 
    if ( a3 < 0x7FFFFFFF0000LL )                                                                                | 
    {                                                                                                           | 
      v10 = a3;                                                                                                 | 
    }                                                                                                           | 
    *(_BYTE *)v10 = *(_BYTE *)v10;                                                                              | 
    *(_BYTE *)(v10 + 7) = *(_BYTE *)(v10 + 7);                                                                  | 
    v11 = (_DWORD *)a5;                                                                                         | 
    if ( a5 )                                                                                                   | 
    {                                                                                                           | 
      if ( a5 < 0x7FFFFFFF0000LL )                                                                              | 
      {                                                                                                         | 
        v9 = a5;                                                                                                | 
      }                                                                                                         | 
      *(_DWORD *)v9 = *(_DWORD *)v9;                                                                            | 
    }                                                                                                           | 
  }                                                                                                             | 
  else                                                                                                          | 
  {                                                                                                             | 
    v11 = (_DWORD *)a5;                                                                                         | 
  }                                                                                                             | 
  if ( a2 )                                                                                                     | 
  {                                                                                                             | 
    return 3221225475LL;                                                                                        | 
  }                                                                                                             | 
  if ( a4 != 8 )                                                                                                | 
  {                                                                                                             | 
    return 3221225476LL;                                                                                        | 
  }                                                                                                             | 
  Object = 0LL;                                                                                                 | 
  v12 = ObReferenceObjectByHandle(Handle, 1u, (POBJECT_TYPE)ExSemaphoreObjectType, PreviousMode, &Object, 0LL); | 
  if ( v12 >= 0 )                                                                                               | 
  {                                                                                                             | 
    v13 = *((_DWORD *)Object + 1);                                                                              | 
    v14 = *((_DWORD *)Object + 6);                                                                              | 
    ObfDereferenceObject(Object);                                                                               | 
    if ( PreviousMode )                                                                                         | 
    {                                                                                                           | 
      *(_DWORD *)a3 = v13;                                                                                      | 
      *(_DWORD *)(a3 + 4) = v14;                                                                                | 
      if ( v11 )                                                                                                | 
      {                                                                                                         | 
        *v11 = 8;                                                                                               | 
      }                                                                                                         | 
    }                                                                                                           | 
    else                                                                                                        | 
    {                                                                                                           | 
      *(_DWORD *)a3 = v13;                                                                                      | 
      *(_DWORD *)(a3 + 4) = v14;                                                                                | 
      if ( v11 )                                                                                                | 
      {                                                                                                         | 
        *v11 = 8;                                                                                               | 
      }                                                                                                         | 
    }                                                                                                           | 
  }                                                                                                             | 
  return (unsigned int)v12;                                                                                     | 
}                                                                                                               | 
69 row(s)
