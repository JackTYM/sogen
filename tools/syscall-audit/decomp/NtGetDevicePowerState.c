NTSTATUS __fastcall NtGetDevicePowerState(HANDLE Handle, _DWORD *a2)                                         | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v4 = 0LL;                                                                                                  | 
  if ( KeGetCurrentThread()->PreviousMode )                                                                  | 
  {                                                                                                          | 
    v5 = 0x7FFFFFFF0000LL;                                                                                   | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                        | 
      v5 = (__int64)a2;                                                                                      | 
    }                                                                                                        | 
    *(_DWORD *)v5 = *(_DWORD *)v5;                                                                           | 
  }                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                         | 
  Object = 0LL;                                                                                              | 
  result = ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)IoFileObjectType, PreviousMode, &Object, 0LL); | 
  if ( result >= 0 )                                                                                         | 
  {                                                                                                          | 
    v10 = 0LL;                                                                                               | 
    RelatedTargetDevice = PnpGetRelatedTargetDevice((PFILE_OBJECT)Object);                                   | 
    if ( RelatedTargetDevice >= 0 && v10 )                                                                   | 
    {                                                                                                        | 
      v4 = *(_QWORD **)(v10 + 32);                                                                           | 
    }                                                                                                        | 
    ObfDereferenceObject(Object);                                                                            | 
    if ( RelatedTargetDevice >= 0 )                                                                          | 
    {                                                                                                        | 
      *a2 = PopLockGetDoDevicePowerState(v4[39]);                                                            | 
      ObfDereferenceObject(v4);                                                                              | 
    }                                                                                                        | 
    return RelatedTargetDevice;                                                                              | 
  }                                                                                                          | 
  return result;                                                                                             | 
}                                                                                                            | 
35 row(s)
