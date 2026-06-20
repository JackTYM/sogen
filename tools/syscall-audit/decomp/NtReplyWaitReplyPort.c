__int64 __fastcall NtReplyWaitReplyPort(void *a1, __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v5 = ObReferenceObjectByHandle(a1, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v5 >= 0 )
  {
    if ( (*((_DWORD *)Object + 104) & 6) == 2 )
    {
      v5 = -1073741811;
    }
    else
    {
      if ( PreviousMode )
      {
        AlpcpProbeForWriteMessageHeader(a2, 0LL);
      }
      v5 = AlpcpProcessSynchronousRequest((_DWORD)Object, 131073, a2, 0, a2, 0LL, 0LL, 0LL, PreviousMode);
      if ( v5 == -1073740029 )
      {
        v5 = -1073741769;
      }
      if ( v5 == -1073740031 )
      {
        v5 = -1073741229;
      }
    }
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v5;
}
39 row(s)
