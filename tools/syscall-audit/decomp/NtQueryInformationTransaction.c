// attributes: thunk                                               | 
NTSTATUS __stdcall NtQueryInformationTransaction(                  | 
        HANDLE TransactionHandle,                                  | 
        TRANSACTION_INFORMATION_CLASS TransactionInformationClass, | 
        PVOID TransactionInformation,                              | 
        ULONG TransactionInformationLength,                        | 
        PULONG ReturnLength)                                       | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  return __imp_NtQueryInformationTransaction(                      | 
           TransactionHandle,                                      | 
           TransactionInformationClass,                            | 
           TransactionInformation,                                 | 
           TransactionInformationLength,                           | 
           ReturnLength);                                          | 
}                                                                  | 
17 row(s)
