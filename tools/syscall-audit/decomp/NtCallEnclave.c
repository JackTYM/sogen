NTSTATUS __stdcall NtCallEnclave(
        PENCLAVE_ROUTINE EnclaveFn,
        PVOID Parameter,
        BOOLEAN WaitForThread,
        PVOID *ReturnValue)
{
  // Calls into an SGX/VBS enclave; returns STATUS_NOT_SUPPORTED if no enclave active
  return STATUS_NOT_SUPPORTED;
}
