// attributes: thunk                                            | 
NTSTATUS __stdcall NtCreateTransaction(                         | 
        PHANDLE TransactionHandle,                              | 
        ACCESS_MASK DesiredAccess,                              | 
        POBJECT_ATTRIBUTES ObjectAttributes,                    | 
        LPGUID Uow,                                             | 
        HANDLE TmHandle,                                        | 
        ULONG CreateOptions,                                    | 
        ULONG IsolationLevel,                                   | 
        ULONG IsolationFlags,                                   | 
        PLARGE_INTEGER Timeout,                                 | 
        PUNICODE_STRING Description)                            | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtCreateTransaction(                             | 
           TransactionHandle,                                   | 
           DesiredAccess,                                       | 
           ObjectAttributes,                                    | 
           Uow,                                                 | 
           TmHandle,                                            | 
           CreateOptions,                                       | 
           IsolationLevel,                                      | 
           IsolationFlags,                                      | 
           Timeout,                                             | 
           Description);                                        | 
}                                                               | 
27 row(s)
