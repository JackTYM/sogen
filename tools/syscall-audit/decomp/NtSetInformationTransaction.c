// attributes: thunk                                               | 
NTSTATUS __stdcall NtSetInformationTransaction(                    | 
        HANDLE TransactionHandle,                                  | 
        TRANSACTION_INFORMATION_CLASS TransactionInformationClass, | 
        PVOID TransactionInformation,                              | 
        ULONG TransactionInformationLength)                        | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  return __imp_NtSetInformationTransaction(                        | 
           TransactionHandle,                                      | 
           TransactionInformationClass,                            | 
           TransactionInformation,                                 | 
           TransactionInformationLength);                          | 
}                                                                  | 
15 row(s)
