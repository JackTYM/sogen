__int64 __fastcall NtWaitForMultipleObjects(unsigned int a1, char *a2, unsigned int a3, char a4, unsigned __int64 a5) | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v8 = a1;                                                                                                            | 
  v14 = (__int64 *)a5;                                                                                                | 
  memset_0(v16, 0, sizeof(v16));                                                                                      | 
  v15 = 0LL;                                                                                                          | 
  if ( (unsigned int)(v8 - 1) > 0x3F )                                                                                | 
  {                                                                                                                   | 
    return 3221225711LL;                                                                                              | 
  }                                                                                                                   | 
  if ( a3 > 1 )                                                                                                       | 
  {                                                                                                                   | 
    return 3221225713LL;                                                                                              | 
  }                                                                                                                   | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                  | 
  v10 = (__int64 *)a5;                                                                                                | 
  if ( PreviousMode )                                                                                                 | 
  {                                                                                                                   | 
    if ( a5 )                                                                                                         | 
    {                                                                                                                 | 
      v11 = 0x7FFFFFFF0000LL;                                                                                         | 
      if ( a5 < 0x7FFFFFFF0000LL )                                                                                    | 
      {                                                                                                               | 
        v11 = a5;                                                                                                     | 
      }                                                                                                               | 
      v15 = *(_QWORD *)v11;                                                                                           | 
      v10 = &v15;                                                                                                     | 
      v14 = &v15;                                                                                                     | 
    }                                                                                                                 | 
    v12 = 8 * v8;                                                                                                     | 
    if ( 8 * v8 && (&a2[v12] < a2 || (unsigned __int64)&a2[v12] > 0x7FFFFFFF0000LL) )                                 | 
    {                                                                                                                 | 
      v10 = v14;                                                                                                      | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  memmove(v16, a2, 8 * v8);                                                                                           | 
  return ObWaitForMultipleObjects(v8, (unsigned int)v16, PreviousMode, a3, PreviousMode, a4, (__int64)v10);           | 
}                                                                                                                     | 
