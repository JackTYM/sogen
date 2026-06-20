// attributes: thunk                                                                                 | 
NTSTATUS __stdcall NtOpenTransaction(                                                                | 
        PHANDLE TransactionHandle,                                                                   | 
        ACCESS_MASK DesiredAccess,                                                                   | 
        POBJECT_ATTRIBUTES ObjectAttributes,                                                         | 
        LPGUID Uow,                                                                                  | 
        HANDLE TmHandle)                                                                             | 
{                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                      | 
                                                                                                     | 
  return __imp_NtOpenTransaction(TransactionHandle, DesiredAccess, ObjectAttributes, Uow, TmHandle); | 
}                                                                                                    | 
12 row(s)
