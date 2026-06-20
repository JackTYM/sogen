// attributes: thunk                                            | 
NTSTATUS __stdcall NtOpenEnlistment(                            | 
        PHANDLE EnlistmentHandle,                               | 
        ACCESS_MASK DesiredAccess,                              | 
        HANDLE ResourceManagerHandle,                           | 
        LPGUID EnlistmentGuid,                                  | 
        POBJECT_ATTRIBUTES ObjectAttributes)                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtOpenEnlistment(                                | 
           EnlistmentHandle,                                    | 
           DesiredAccess,                                       | 
           ResourceManagerHandle,                               | 
           EnlistmentGuid,                                      | 
           ObjectAttributes);                                   | 
}                                                               | 
17 row(s)
