__int64 __fastcall NtGetContextThread(void *a1, int a2)                                                              | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  Object = 0LL;                                                                                                      | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  ContextThreadInternal = ObReferenceObjectByHandle(a1, 8u, (POBJECT_TYPE)PsThreadType, PreviousMode, &Object, 0LL); | 
  if ( ContextThreadInternal >= 0 )                                                                                  | 
  {                                                                                                                  | 
    v7 = Object;                                                                                                     | 
    if ( (*((_DWORD *)Object + 29) & 0x400) != 0 )                                                                   | 
    {                                                                                                                | 
      ContextThreadInternal = -1073741816;                                                                           | 
    }                                                                                                                | 
    else                                                                                                             | 
    {                                                                                                                | 
      LOBYTE(v6) = PreviousMode;                                                                                     | 
      LOBYTE(v5) = PreviousMode;                                                                                     | 
      ContextThreadInternal = PspGetContextThreadInternal((_DWORD)Object, a2, v5, v6, 1);                            | 
    }                                                                                                                | 
    ObfDereferenceObject(v7);                                                                                        | 
  }                                                                                                                  | 
  return (unsigned int)ContextThreadInternal;                                                                        | 
}                                                                                                                    | 
