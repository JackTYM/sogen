// attributes: thunk                                                             | 
NTSTATUS __stdcall NtQueryInformationTransactionManager(                         | 
        HANDLE TransactionManagerHandle,                                         | 
        TRANSACTIONMANAGER_INFORMATION_CLASS TransactionManagerInformationClass, | 
        PVOID TransactionManagerInformation,                                     | 
        ULONG TransactionManagerInformationLength,                               | 
        PULONG ReturnLength)                                                     | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  return __imp_NtQueryInformationTransactionManager(                             | 
           TransactionManagerHandle,                                             | 
           TransactionManagerInformationClass,                                   | 
           TransactionManagerInformation,                                        | 
           TransactionManagerInformationLength,                                  | 
           ReturnLength);                                                        | 
}                                                                                | 
17 row(s)
