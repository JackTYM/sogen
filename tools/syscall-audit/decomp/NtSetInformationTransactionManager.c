// attributes: thunk                                                             | 
NTSTATUS __stdcall NtSetInformationTransactionManager(                           | 
        HANDLE TmHandle,                                                         | 
        TRANSACTIONMANAGER_INFORMATION_CLASS TransactionManagerInformationClass, | 
        PVOID TransactionManagerInformation,                                     | 
        ULONG TransactionManagerInformationLength)                               | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  return __imp_NtSetInformationTransactionManager(                               | 
           TmHandle,                                                             | 
           TransactionManagerInformationClass,                                   | 
           TransactionManagerInformation,                                        | 
           TransactionManagerInformationLength);                                 | 
}                                                                                | 
15 row(s)
