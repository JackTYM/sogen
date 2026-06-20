NTSTATUS __stdcall NtOpenProcessTokenEx(                                                                            | 
        HANDLE ProcessHandle,                                                                                       | 
        ACCESS_MASK DesiredAccess,                                                                                  | 
        ULONG HandleAttributes,                                                                                     | 
        PHANDLE TokenHandle)                                                                                        | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  Handle = 0LL;                                                                                                     | 
  AccessMode = KeGetCurrentThread()->PreviousMode;                                                                  | 
  v8 = 73714;                                                                                                       | 
  if ( AccessMode )                                                                                                 | 
  {                                                                                                                 | 
    v8 = 7666;                                                                                                      | 
  }                                                                                                                 | 
  v9 = HandleAttributes & v8;                                                                                       | 
  if ( AccessMode )                                                                                                 | 
  {                                                                                                                 | 
    v10 = 0x7FFFFFFF0000LL;                                                                                         | 
    if ( (unsigned __int64)TokenHandle < 0x7FFFFFFF0000LL )                                                         | 
    {                                                                                                               | 
      v10 = (__int64)TokenHandle;                                                                                   | 
    }                                                                                                               | 
    *(_QWORD *)v10 = *(_QWORD *)v10;                                                                                | 
  }                                                                                                                 | 
  Object = 0LL;                                                                                                     | 
  result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)ProcessHandle, 0x65537350u, (__int64)&Object, 0LL, 0LL);    | 
  if ( result >= 0 )                                                                                                | 
  {                                                                                                                 | 
    v12 = (void *)PsReferencePrimaryTokenWithTag(Object, 1953261124LL);                                             | 
    ObfDereferenceObjectWithTag(Object, 0x65537350u);                                                               | 
    v13 = ObOpenObjectByPointer(v12, v9, 0LL, DesiredAccess, (POBJECT_TYPE)SeTokenObjectType, AccessMode, &Handle); | 
    ObfDereferenceObject(v12);                                                                                      | 
    if ( v13 >= 0 )                                                                                                 | 
    {                                                                                                               | 
      *TokenHandle = Handle;                                                                                        | 
    }                                                                                                               | 
    return v13;                                                                                                     | 
  }                                                                                                                 | 
  return result;                                                                                                    | 
}                                                                                                                   | 
