// attributes: thunk                                                                              | 
NTSTATUS __stdcall NtPropagationComplete(                                                         | 
        HANDLE ResourceManagerHandle,                                                             | 
        ULONG RequestCookie,                                                                      | 
        ULONG BufferLength,                                                                       | 
        PVOID Buffer)                                                                             | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  return __imp_NtPropagationComplete(ResourceManagerHandle, RequestCookie, BufferLength, Buffer); | 
}                                                                                                 | 
11 row(s)
