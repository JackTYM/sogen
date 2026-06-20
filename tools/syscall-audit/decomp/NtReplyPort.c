__int64 __fastcall NtReplyPort(HANDLE Handle, __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset_0(v9, 0, 0x40uLL);
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  Object = 0LL;
  v5 = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, KeGetCurrentThread()->PreviousMode, &Object, 0LL);
  if ( v5 >= 0 )
  {
    v10 = 0;
    v9[0] = Object;
    if ( (*((_DWORD *)Object + 104) & 0x2000) != 0 )
    {
      LOBYTE(v6) = KeGetCurrentThread()->PreviousMode;
      v5 = AlpcpReplyLegacySynchronousRequest(v9, a2, v6);
    }
    else
    {
      v10 = 65537;
      LOBYTE(v7) = KeGetCurrentThread()->PreviousMode;
      v5 = AlpcpSendMessage(v9, a2, 0LL, v7);
      if ( v5 == -1073740029 )
      {
        v5 = -1073741769;
      }
    }
    ObfDereferenceObject(Object);
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v5;
}
33 row(s)
