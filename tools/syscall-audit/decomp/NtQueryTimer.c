__int64 __fastcall NtQueryTimer(HANDLE Handle, int a2, unsigned __int64 a3, int a4, unsigned __int64 a5) | 
{                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                          | 
                                                                                                         | 
  if ( a2 )                                                                                              | 
  {                                                                                                      | 
    return 3221225475LL;                                                                                 | 
  }                                                                                                      | 
  if ( a4 != 16 )                                                                                        | 
  {                                                                                                      | 
    return 3221225476LL;                                                                                 | 
  }                                                                                                      | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                     | 
  if ( PreviousMode )                                                                                    | 
  {                                                                                                      | 
    if ( (a3 & 3) != 0 )                                                                                 | 
    {                                                                                                    | 
      ExRaiseDatatypeMisalignment();                                                                     | 
    }                                                                                                    | 
    v8 = 0x7FFFFFFF0000LL;                                                                               | 
    v9 = 0x7FFFFFFF0000LL;                                                                               | 
    if ( a3 < 0x7FFFFFFF0000LL )                                                                         | 
    {                                                                                                    | 
      v9 = a3;                                                                                           | 
    }                                                                                                    | 
    *(_BYTE *)v9 = *(_BYTE *)v9;                                                                         | 
    *(_BYTE *)(v9 + 15) = *(_BYTE *)(v9 + 15);                                                           | 
    v10 = (_DWORD *)a5;                                                                                  | 
    if ( a5 )                                                                                            | 
    {                                                                                                    | 
      if ( a5 < 0x7FFFFFFF0000LL )                                                                       | 
      {                                                                                                  | 
        v8 = a5;                                                                                         | 
      }                                                                                                  | 
      *(_DWORD *)v8 = *(_DWORD *)v8;                                                                     | 
    }                                                                                                    | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v10 = (_DWORD *)a5;                                                                                  | 
  }                                                                                                      | 
  Object = 0LL;                                                                                          | 
  v11 = ObReferenceObjectByHandle(Handle, 1u, ExTimerObjectType, PreviousMode, &Object, 0LL);            | 
  if ( v11 >= 0 )                                                                                        | 
  {                                                                                                      | 
    v12 = *((_BYTE *)Object + 4);                                                                        | 
    v13 = MEMORY[0xFFFFF78000000008];                                                                    | 
    v14 = KeQueryTimerDueTime(Object) - v13;                                                             | 
    ObfDereferenceObject(Object);                                                                        | 
    if ( PreviousMode )                                                                                  | 
    {                                                                                                    | 
      *(_BYTE *)(a3 + 8) = v12;                                                                          | 
      *(_QWORD *)a3 = v14;                                                                               | 
      if ( v10 )                                                                                         | 
      {                                                                                                  | 
        *v10 = 16;                                                                                       | 
      }                                                                                                  | 
    }                                                                                                    | 
    else                                                                                                 | 
    {                                                                                                    | 
      *(_BYTE *)(a3 + 8) = v12;                                                                          | 
      *(_QWORD *)a3 = v14;                                                                               | 
      if ( v10 )                                                                                         | 
      {                                                                                                  | 
        *v10 = 16;                                                                                       | 
      }                                                                                                  | 
    }                                                                                                    | 
  }                                                                                                      | 
  return (unsigned int)v11;                                                                              | 
}                                                                                                        | 
70 row(s)
