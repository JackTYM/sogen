__int64 __fastcall NtSetInformationIoRing(HANDLE Handle, int a2, unsigned int a3, _QWORD *a4) | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                          | 
  v8 = 0LL;                                                                                   | 
  Object = 0LL;                                                                               | 
  if ( a2 < 0 )                                                                               | 
  {                                                                                           | 
    goto LABEL_11;                                                                            | 
  }                                                                                           | 
  if ( (unsigned __int64)a2 >= 2 )                                                            | 
  {                                                                                           | 
    goto LABEL_11;                                                                            | 
  }                                                                                           | 
  v9 = *((_DWORD *)&IopIoRingSetOperationLength + a2);                                        | 
  if ( !v9 )                                                                                  | 
  {                                                                                           | 
    goto LABEL_11;                                                                            | 
  }                                                                                           | 
  if ( a3 < v9 )                                                                              | 
  {                                                                                           | 
    v10 = -1073741820;                                                                        | 
    goto LABEL_12;                                                                            | 
  }                                                                                           | 
  Object = 0LL;                                                                               | 
  v10 = ObReferenceObjectByHandle(Handle, 0, IoRingObjectType, PreviousMode, &Object, 0LL);   | 
  v8 = Object;                                                                                | 
  if ( v10 >= 0 )                                                                             | 
  {                                                                                           | 
    if ( a2 == 1 )                                                                            | 
    {                                                                                         | 
      v15[0] = *a4;                                                                           | 
      LOBYTE(v11) = PreviousMode;                                                             | 
      updated = IopIoRingUpdateCompletionUserEvent(Object, v15, v11);                         | 
      v10 = updated;                                                                          | 
      if ( updated == -1073741816 || updated == -1073741788 )                                 | 
      {                                                                                       | 
        v10 = -1073741582;                                                                    | 
      }                                                                                       | 
      goto LABEL_12;                                                                          | 
    }                                                                                         | 
LABEL_11:                                                                                     | 
    v10 = -1073741821;                                                                        | 
  }                                                                                           | 
LABEL_12:                                                                                     | 
  if ( v8 )                                                                                   | 
  {                                                                                           | 
    ObfDereferenceObject(v8);                                                                 | 
  }                                                                                           | 
  return (unsigned int)v10;                                                                   | 
}                                                                                             | 
52 row(s)
