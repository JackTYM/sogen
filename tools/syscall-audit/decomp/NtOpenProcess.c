NTSTATUS __stdcall NtOpenProcess(                               | 
        PHANDLE ProcessHandle,                                  | 
        ACCESS_MASK DesiredAccess,                              | 
        POBJECT_ATTRIBUTES ObjectAttributes,                    | 
        PCLIENT_ID ClientId)                                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;            | 
  return PsOpenProcess(                                         | 
           (_DWORD)ProcessHandle,                               | 
           DesiredAccess,                                       | 
           (_DWORD)ObjectAttributes,                            | 
           (_DWORD)ClientId,                                    | 
           PreviousMode,                                        | 
           PreviousMode);                                       | 
}                                                               | 
