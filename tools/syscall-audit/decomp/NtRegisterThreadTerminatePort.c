NTSTATUS __fastcall NtRegisterThreadTerminatePort(void *a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  Object = 0LL;
  result = ObReferenceObjectByHandle(a1, 1u, LpcPortObjectType, CurrentThread->PreviousMode, &Object, 0LL);
  if ( result >= 0 )
  {
    Pool2 = (void **)ExAllocatePool2(0x101uLL, 0x10uLL, 0x70547350u);
    v4 = Pool2;
    if ( Pool2 )
    {
      Pool2[1] = Object;
      *Pool2 = CurrentThread[1].InitialStack;
      result = 0;
      CurrentThread[1].InitialStack = v4;
    }
    else
    {
      ObfDereferenceObject(Object);
      return -1073741670;
    }
  }
  return result;
}
26 row(s)
