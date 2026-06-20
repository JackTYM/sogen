NTSTATUS __stdcall NtQueryDirectoryFile(
        HANDLE FileHandle,
        HANDLE Event,
        PIO_APC_ROUTINE ApcRoutine,
        PVOID ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        FILE_INFORMATION_CLASS FileInformationClass,
        BOOLEAN ReturnSingleEntry,
        PUNICODE_STRING FileName,
        BOOLEAN RestartScan)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v11 = ReturnSingleEntry != 0;
  ReturnSingleEntry = 0;
  LOBYTE(v20) = 0;
  v23 = 0LL;
  v22 = 0LL;
  v21 = 0LL;
  v13 = (v11 ? 2 : 0) | 1;
  if ( !RestartScan )
  {
    v13 = v11 ? 2 : 0;
  }
  LODWORD(v18) = Length;
  result = BuildQueryDirectoryIrp(
             FileHandle,
             (__int64)IoStatusBlock,
             FileInformation,
             v18,
             FileInformationClass,
             v13,
             (__int64)FileName,
             v19,
             (__int64)&ReturnSingleEntry,
             (__int64)&v23,
             (__int64)&v22,
             (__int64)&v21,
             (__int64)&v20);
  if ( !result )
  {
    LOBYTE(v15) = 1;
    LOBYTE(Address) = ReturnSingleEntry;
    LOBYTE(v16) = v20;
    return IopSynchronousServiceTail(v23, v22, v21, v15, v16, Address, 2);
  }
  return result;
}
50 row(s)
