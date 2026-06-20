// attributes: thunk                                            | 
NTSTATUS __stdcall NtRegisterProtocolAddressInformation(        | 
        HANDLE ResourceManager,                                 | 
        PCRM_PROTOCOL_ID ProtocolId,                            | 
        ULONG ProtocolInformationSize,                          | 
        PVOID ProtocolInformation,                              | 
        ULONG CreateOptions)                                    | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return __imp_NtRegisterProtocolAddressInformation(            | 
           ResourceManager,                                     | 
           ProtocolId,                                          | 
           ProtocolInformationSize,                             | 
           ProtocolInformation,                                 | 
           CreateOptions);                                      | 
}                                                               | 
17 row(s)
