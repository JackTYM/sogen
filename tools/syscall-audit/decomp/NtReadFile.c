NTSTATUS __stdcall NtReadFile(
        HANDLE FileHandle,
        HANDLE Event,
        PIO_APC_ROUTINE ApcRoutine,
        PVOID ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID Buffer,
        ULONG Length,
        PLARGE_INTEGER ByteOffset,
        PULONG Key)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Object = 0LL;
  result = ObReferenceObjectByHandle(
             FileHandle,
             1u,
             (POBJECT_TYPE)IoFileObjectType,
             KeGetCurrentThread()->PreviousMode,
             &Object,
             0LL);
  if ( result >= 0 )
  {
    v10 = Object;
    v11 = (_DWORD *)*((_QWORD *)Object + 26);
    if ( v11
      && (*v11 & 4) != 0
      && (IsProcessAppContainer = PsIsProcessAppContainer(KeGetCurrentThread()->ApcState.Process),
          v10 = Object,
          IsProcessAppContainer) )
    {
      ObfDereferenceObject(Object);
      return -1073739504;
    }
    else
    {
      return IopReadFile(
               (ULONG_PTR)v10,
               (__int64)IoStatusBlock,
               Buffer,
               Length,
               (__int64)ByteOffset,
               (__int64)Key,
               0LL,
               0LL,
               0,
               0LL,
               0LL);
    }
  }
  return result;
}
52 row(s)
