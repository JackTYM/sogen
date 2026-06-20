NTSTATUS __stdcall NtOpenThreadToken(                                                  | 
        HANDLE ThreadHandle,                                                           | 
        ACCESS_MASK DesiredAccess,                                                     | 
        BOOLEAN OpenAsSelf,                                                            | 
        PHANDLE TokenHandle)                                                           | 
{                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                        | 
                                                                                       | 
  return NtOpenThreadTokenEx(ThreadHandle, DesiredAccess, OpenAsSelf, 0, TokenHandle); | 
}                                                                                      | 
