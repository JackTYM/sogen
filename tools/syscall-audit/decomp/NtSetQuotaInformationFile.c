NTSTATUS __stdcall NtSetQuotaInformationFile(                   | 
        HANDLE FileHandle,                                      | 
        PIO_STATUS_BLOCK IoStatusBlock,                         | 
        PVOID Buffer,                                           | 
        ULONG Length)                                           | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return IopSetEaOrQuotaInformationFile((int)FileHandle, v5);   | 
}                                                               | 
10 row(s)
