NTSTATUS __stdcall NtDeviceIoControlFile(                       | 
        HANDLE FileHandle,                                      | 
        HANDLE Event,                                           | 
        PIO_APC_ROUTINE ApcRoutine,                             | 
        PVOID ApcContext,                                       | 
        PIO_STATUS_BLOCK IoStatusBlock,                         | 
        ULONG IoControlCode,                                    | 
        PVOID InputBuffer,                                      | 
        ULONG InputBufferLength,                                | 
        PVOID OutputBuffer,                                     | 
        ULONG OutputBufferLength)                               | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return IopXxxControlFile(                                     | 
           FileHandle,                                          | 
           (__int64)IoStatusBlock,                              | 
           IoControlCode,                                       | 
           (__int64)InputBuffer,                                | 
           InputBufferLength,                                   | 
           (__int64)OutputBuffer,                               | 
           OutputBufferLength,                                  | 
           1,                                                   | 
           0);                                                  | 
}                                                               | 
25 row(s)
