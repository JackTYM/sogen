// attributes: thunk                                            | 
NTSTATUS __stdcall NtCreateEnlistment(                          | 
        PHANDLE EnlistmentHandle,                               | 
        ACCESS_MASK DesiredAccess,                              | 
        HANDLE ResourceManagerHandle,                           | 
        HANDLE TransactionHandle,                               | 
        POBJECT_ATTRIBUTES ObjectAttributes,                    | 
        ULONG CreateOptions,                                    | 
        NOTIFICATION_MASK NotificationMask,                     | 
        PVOID EnlistmentKey)                                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtCreateEnlistment(                              | 
           EnlistmentHandle,                                    | 
           DesiredAccess,                                       | 
           ResourceManagerHandle,                               | 
           TransactionHandle,                                   | 
           ObjectAttributes,                                    | 
           CreateOptions,                                       | 
           NotificationMask,                                    | 
           EnlistmentKey);                                      | 
}                                                               | 
23 row(s)
