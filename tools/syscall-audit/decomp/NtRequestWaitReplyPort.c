__int64 __fastcall NtRequestWaitReplyPort(void *a1, int a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v7 = ObReferenceObjectByHandle(a1, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v7 >= 0 )
  {
    if ( PreviousMode )
    {
      AlpcpProbeForWriteMessageHeader(a3, 0LL);
    }
    v7 = AlpcpProcessSynchronousRequest((_DWORD)Object, 0x20000, a2, 0, a3, 0LL, 0LL, 0LL, PreviousMode);
    if ( v7 == -1073740029 )
    {
      v7 = -1073741769;
    }
    if ( v7 == -1073740031 )
    {
      v7 = -1073741229;
    }
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v7;
}
32 row(s)
