// attributes: thunk                                                       | 
NTSTATUS __stdcall NtQueryInformationResourceManager(                      | 
        HANDLE ResourceManagerHandle,                                      | 
        RESOURCEMANAGER_INFORMATION_CLASS ResourceManagerInformationClass, | 
        PVOID ResourceManagerInformation,                                  | 
        ULONG ResourceManagerInformationLength,                            | 
        PULONG ReturnLength)                                               | 
{                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]            | 
                                                                           | 
  return __imp_NtQueryInformationResourceManager(                          | 
           ResourceManagerHandle,                                          | 
           ResourceManagerInformationClass,                                | 
           ResourceManagerInformation,                                     | 
           ResourceManagerInformationLength,                               | 
           ReturnLength);                                                  | 
}                                                                          | 
17 row(s)
