// attributes: thunk                                             | 
NTSTATUS __stdcall NtSetInformationEnlistment(                   | 
        HANDLE EnlistmentHandle,                                 | 
        ENLISTMENT_INFORMATION_CLASS EnlistmentInformationClass, | 
        PVOID EnlistmentInformation,                             | 
        ULONG EnlistmentInformationLength)                       | 
{                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]  | 
                                                                 | 
  return __imp_NtSetInformationEnlistment(                       | 
           EnlistmentHandle,                                     | 
           EnlistmentInformationClass,                           | 
           EnlistmentInformation,                                | 
           EnlistmentInformationLength);                         | 
}                                                                | 
15 row(s)
