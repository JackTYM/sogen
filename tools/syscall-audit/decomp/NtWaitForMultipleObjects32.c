__int64 __fastcall NtWaitForMultipleObjects32(                                                              | 
        unsigned int a1,                                                                                    | 
        unsigned __int64 a2,                                                                                | 
        unsigned int a3,                                                                                    | 
        char a4,                                                                                            | 
        unsigned __int64 a5)                                                                                | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v8 = a1;                                                                                                  | 
  v15 = (_QWORD *)a5;                                                                                       | 
  memset_0(v17, 0, sizeof(v17));                                                                            | 
  v9 = 0;                                                                                                   | 
  v16[0] = 0LL;                                                                                             | 
  if ( (unsigned int)(v8 - 1) > 0x3F )                                                                      | 
  {                                                                                                         | 
    return 3221225711LL;                                                                                    | 
  }                                                                                                         | 
  if ( a3 > 1 )                                                                                             | 
  {                                                                                                         | 
    return 3221225713LL;                                                                                    | 
  }                                                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                        | 
  v11 = (_QWORD *)a5;                                                                                       | 
  if ( PreviousMode )                                                                                       | 
  {                                                                                                         | 
    if ( a5 )                                                                                               | 
    {                                                                                                       | 
      v12 = 0x7FFFFFFF0000LL;                                                                               | 
      if ( a5 < 0x7FFFFFFF0000LL )                                                                          | 
      {                                                                                                     | 
        v12 = a5;                                                                                           | 
      }                                                                                                     | 
      v16[0] = *(_QWORD *)v12;                                                                              | 
      v11 = v16;                                                                                            | 
      v15 = v16;                                                                                            | 
    }                                                                                                       | 
    v13 = 4 * v8;                                                                                           | 
    if ( 4 * v8 && (v13 + a2 > 0x7FFFFFFF0000LL || v13 + a2 < a2) )                                         | 
    {                                                                                                       | 
      v11 = v15;                                                                                            | 
    }                                                                                                       | 
  }                                                                                                         | 
  while ( v9 < (unsigned int)v8 )                                                                           | 
  {                                                                                                         | 
    v17[v9] = *(int *)(a2 + 4LL * v9);                                                                      | 
    ++v9;                                                                                                   | 
  }                                                                                                         | 
  return ObWaitForMultipleObjects(v8, (unsigned int)v17, PreviousMode, a3, PreviousMode, a4, (__int64)v11); | 
}                                                                                                           | 
