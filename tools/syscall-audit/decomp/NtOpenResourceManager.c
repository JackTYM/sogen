// attributes: thunk                                            | 
NTSTATUS __stdcall NtOpenResourceManager(                       | 
        PHANDLE ResourceManagerHandle,                          | 
        ACCESS_MASK DesiredAccess,                              | 
        HANDLE TmHandle,                                        | 
        LPGUID ResourceManagerGuid,                             | 
        POBJECT_ATTRIBUTES ObjectAttributes)                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtOpenResourceManager(                           | 
           ResourceManagerHandle,                               | 
           DesiredAccess,                                       | 
           TmHandle,                                            | 
           ResourceManagerGuid,                                 | 
           ObjectAttributes);                                   | 
}                                                               | 
17 row(s)
