NTSTATUS __fastcall NtCancelSynchronousIoFile(void *a1, __int64 a2, unsigned __int64 a3)              | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                  | 
  if ( PreviousMode )                                                                                 | 
  {                                                                                                   | 
    v6 = 0x7FFFFFFF0000LL;                                                                            | 
    if ( a3 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                 | 
      v6 = a3;                                                                                        | 
    }                                                                                                 | 
    *(_DWORD *)v6 = *(_DWORD *)v6;                                                                    | 
  }                                                                                                   | 
  Object = 0LL;                                                                                       | 
  result = ObReferenceObjectByHandle(a1, 1u, (POBJECT_TYPE)PsThreadType, PreviousMode, &Object, 0LL); | 
  if ( result >= 0 )                                                                                  | 
  {                                                                                                   | 
    CurrentThread = KeGetCurrentThread();                                                             | 
    ++CurrentThread->OtherOperationCount;                                                             | 
    __incgsdword(0x2EE4u);                                                                            | 
    v9 = Object;                                                                                      | 
    v11 = (unsigned int)IopCancelSynchronousIrpsForThread(Object, a2) == 0 ? 0xC0000225 : 0;          | 
    *(_DWORD *)a3 = v11;                                                                              | 
    *(_QWORD *)(a3 + 8) = 0LL;                                                                        | 
    ObfDereferenceObject(v9);                                                                         | 
    return v11;                                                                                       | 
  }                                                                                                   | 
  return result;                                                                                      | 
}                                                                                                     | 
30 row(s)
