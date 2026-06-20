// attributes: thunk                                             | 
NTSTATUS __stdcall NtQueryInformationEnlistment(                 | 
        HANDLE EnlistmentHandle,                                 | 
        ENLISTMENT_INFORMATION_CLASS EnlistmentInformationClass, | 
        PVOID EnlistmentInformation,                             | 
        ULONG EnlistmentInformationLength,                       | 
        PULONG ReturnLength)                                     | 
{                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]  | 
                                                                 | 
  return __imp_NtQueryInformationEnlistment(                     | 
           EnlistmentHandle,                                     | 
           EnlistmentInformationClass,                           | 
           EnlistmentInformation,                                | 
           EnlistmentInformationLength,                          | 
           ReturnLength);                                        | 
}                                                                | 
17 row(s)
