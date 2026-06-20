__int64 __fastcall NtAlpcDisconnectPort(void *a1, unsigned int a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( (a2 & 0xFFFFFFFE) != 0 )
  {
    v4 = -1073741811;
  }
  else
  {
    Object = 0LL;
    v4 = ObReferenceObjectByHandle(a1, 1u, AlpcPortObjectType, KeGetCurrentThread()->PreviousMode, &Object, 0LL);
    if ( v4 >= 0 )
    {
      v4 = AlpcpDisconnectPort(Object, a2);
      ObfDereferenceObject(Object);
    }
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v4;
}
23 row(s)
