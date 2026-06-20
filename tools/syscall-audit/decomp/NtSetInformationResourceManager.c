// attributes: thunk                                                       | 
NTSTATUS __stdcall NtSetInformationResourceManager(                        | 
        HANDLE ResourceManagerHandle,                                      | 
        RESOURCEMANAGER_INFORMATION_CLASS ResourceManagerInformationClass, | 
        PVOID ResourceManagerInformation,                                  | 
        ULONG ResourceManagerInformationLength)                            | 
{                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]            | 
                                                                           | 
  return __imp_NtSetInformationResourceManager(                            | 
           ResourceManagerHandle,                                          | 
           ResourceManagerInformationClass,                                | 
           ResourceManagerInformation,                                     | 
           ResourceManagerInformationLength);                              | 
}                                                                          | 
15 row(s)
