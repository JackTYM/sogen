// attributes: thunk                                            | 
NTSTATUS __stdcall NtCreateTransactionManager(                  | 
        PHANDLE TmHandle,                                       | 
        ACCESS_MASK DesiredAccess,                              | 
        POBJECT_ATTRIBUTES ObjectAttributes,                    | 
        PUNICODE_STRING LogFileName,                            | 
        ULONG CreateOptions,                                    | 
        ULONG CommitStrength)                                   | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtCreateTransactionManager(                      | 
           TmHandle,                                            | 
           DesiredAccess,                                       | 
           ObjectAttributes,                                    | 
           LogFileName,                                         | 
           CreateOptions,                                       | 
           CommitStrength);                                     | 
}                                                               | 
19 row(s)
