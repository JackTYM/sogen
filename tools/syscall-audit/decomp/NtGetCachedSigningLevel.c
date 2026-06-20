__int64 __fastcall NtGetCachedSigningLevel(                                                                | 
        void *a1,                                                                                          | 
        _DWORD *a2,                                                                                        | 
        volatile void *a3,                                                                                 | 
        volatile void *a4,                                                                                 | 
        unsigned int *a5,                                                                                  | 
        volatile void *a6)                                                                                 | 
{                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                            | 
                                                                                                           | 
  v26 = a4;                                                                                                | 
  Address = a3;                                                                                            | 
  Handle = a1;                                                                                             | 
  v27 = a6;                                                                                                | 
  v10 = 0LL;                                                                                               | 
  memset_0(Src, 0, sizeof(Src));                                                                           | 
  Length = 64LL;                                                                                           | 
  v20 = 0;                                                                                                 | 
  v19[0] = 0;                                                                                              | 
  if ( !a1 || !a2 || !a3 )                                                                                 | 
  {                                                                                                        | 
    v12 = -1073741811;                                                                                     | 
    goto LABEL_9;                                                                                          | 
  }                                                                                                        | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                       | 
  Object = 0LL;                                                                                            | 
  v12 = ObReferenceObjectByHandle(Handle, 1u, (POBJECT_TYPE)IoFileObjectType, PreviousMode, &Object, 0LL); | 
  v10 = Object;                                                                                            | 
  Handle = Object;                                                                                         | 
  if ( v12 < 0 )                                                                                           | 
  {                                                                                                        | 
    goto LABEL_9;                                                                                          | 
  }                                                                                                        | 
  if ( a4 )                                                                                                | 
  {                                                                                                        | 
    if ( !qword_140F04970 )                                                                                | 
    {                                                                                                      | 
      goto LABEL_7;                                                                                        | 
    }                                                                                                      | 
    if ( Object )                                                                                          | 
    {                                                                                                      | 
      v14 = guard_dispatch_icall_no_overrides(Object, v19, &v20, Src, &Length, (char *)&Length + 4);       | 
LABEL_17:                                                                                                  | 
      v12 = v14;                                                                                           | 
      goto LABEL_8;                                                                                        | 
    }                                                                                                      | 
  }                                                                                                        | 
  else                                                                                                     | 
  {                                                                                                        | 
    if ( !qword_140F04970 )                                                                                | 
    {                                                                                                      | 
LABEL_7:                                                                                                   | 
      v12 = -1073741823;                                                                                   | 
      goto LABEL_8;                                                                                        | 
    }                                                                                                      | 
    if ( Object )                                                                                          | 
    {                                                                                                      | 
      v14 = guard_dispatch_icall_no_overrides(Object, v19, &v20, 0LL, 0LL, 0LL);                           | 
      goto LABEL_17;                                                                                       | 
    }                                                                                                      | 
  }                                                                                                        | 
  v12 = -1073741811;                                                                                       | 
LABEL_8:                                                                                                   | 
  if ( v12 >= 0 )                                                                                          | 
  {                                                                                                        | 
    if ( PreviousMode == 1 )                                                                               | 
    {                                                                                                      | 
      ProbeForWrite(a2, 4uLL, 4u);                                                                         | 
      ProbeForWrite(Address, 1uLL, 1u);                                                                    | 
    }                                                                                                      | 
    v15 = v20;                                                                                             | 
    *a2 = v20;                                                                                             | 
    *(_BYTE *)Address = v19[0];                                                                            | 
    if ( a5 )                                                                                              | 
    {                                                                                                      | 
      if ( PreviousMode == 1 )                                                                             | 
      {                                                                                                    | 
        ProbeForWrite(a5, 4uLL, 4u);                                                                       | 
        v15 = v20;                                                                                         | 
      }                                                                                                    | 
      if ( (v15 & 2) != 0 )                                                                                | 
      {                                                                                                    | 
        v16 = Length;                                                                                      | 
        if ( *a5 >= (unsigned int)Length && (v17 = (void *)v26) != 0LL )                                   | 
        {                                                                                                  | 
          if ( PreviousMode == 1 )                                                                         | 
          {                                                                                                | 
            ProbeForWrite(v26, (unsigned int)Length, 1u);                                                  | 
            v16 = Length;                                                                                  | 
            v17 = (void *)v26;                                                                             | 
          }                                                                                                | 
          memmove(v17, Src, v16);                                                                          | 
        }                                                                                                  | 
        else                                                                                               | 
        {                                                                                                  | 
          v12 = -1073741789;                                                                               | 
          v22 = -1073741789;                                                                               | 
        }                                                                                                  | 
        *a5 = v16;                                                                                         | 
        v18 = v27;                                                                                         | 
        if ( v27 )                                                                                         | 
        {                                                                                                  | 
          if ( PreviousMode == 1 )                                                                         | 
          {                                                                                                | 
            ProbeForWrite(v27, 4uLL, 4u);                                                                  | 
          }                                                                                                | 
          *v18 = HIDWORD(Length);                                                                          | 
        }                                                                                                  | 
      }                                                                                                    | 
      else                                                                                                 | 
      {                                                                                                    | 
        *a5 = 0;                                                                                           | 
      }                                                                                                    | 
    }                                                                                                      | 
  }                                                                                                        | 
LABEL_9:                                                                                                   | 
  if ( v10 )                                                                                               | 
  {                                                                                                        | 
    ObfDereferenceObject(v10);                                                                             | 
  }                                                                                                        | 
  return (unsigned int)v12;                                                                                | 
}                                                                                                          | 
122 row(s)
