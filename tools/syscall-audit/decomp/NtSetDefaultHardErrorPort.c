NTSTATUS __fastcall NtSetDefaultHardErrorPort(HANDLE Handle)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentServerSiloGlobals = PsGetCurrentServerSiloGlobals();
  if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, KeGetCurrentThread()->PreviousMode) )
  {
    return -1073741727;
  }
  if ( *(_DWORD *)(CurrentServerSiloGlobals + 864) == 1 )
  {
    return -1073741823;
  }
  Object = 0LL;
  result = ObReferenceObjectByHandle(Handle, 0, LpcPortObjectType, KeGetCurrentThread()->PreviousMode, &Object, 0LL);
  *(_QWORD *)(CurrentServerSiloGlobals + 856) = Object;
  if ( result >= 0 )
  {
    if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo() )
    {
      ExReadyForErrors = 1;
    }
    *(_DWORD *)(CurrentServerSiloGlobals + 864) = 1;
    *(_QWORD *)(CurrentServerSiloGlobals + 848) = KeGetCurrentThread()->ApcState.Process;
    PsReferenceSiloContext();
    return 0;
  }
  return result;
}
29 row(s)
