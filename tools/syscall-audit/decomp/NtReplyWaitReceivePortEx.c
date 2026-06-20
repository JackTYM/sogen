__int64 __fastcall NtReplyWaitReceivePortEx(HANDLE Handle, __int64 a2, __int64 a3, unsigned __int64 a4, __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset_0(v21, 0, 0x40uLL);
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    if ( (a4 & 3) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v11 = 0x7FFFFFFF0000LL;
    if ( a4 < 0x7FFFFFFF0000LL )
    {
      v11 = a4;
    }
    *(_BYTE *)v11 = *(_BYTE *)v11;
    *(_BYTE *)(v11 + 39) = *(_BYTE *)(v11 + 39);
  }
  Object = 0LL;
  v12 = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v12 >= 0 )
  {
    v15 = Object;
    v21[0] = Object;
    v25[0] = 0;
    if ( !a3 )
    {
      goto LABEL_9;
    }
    if ( (*((_DWORD *)Object + 104) & 0x2000) != 0 )
    {
      v25[0] = 4;
      v23 = 0LL;
      v22 = 0LL;
      v24 = 0LL;
      LOBYTE(v13) = PreviousMode;
      v19 = AlpcpReplyLegacySynchronousRequest(v21, a3, v13);
    }
    else
    {
      v25[0] = 65541;
      v23 = 0LL;
      v22 = 0LL;
      v24 = 0LL;
      LOBYTE(v14) = PreviousMode;
      v19 = AlpcpSendMessage(v21, a3, 0LL, v14);
    }
    v12 = v19;
    if ( v19 < 0 )
    {
      ObfDereferenceObject(v15);
      if ( v12 == -1073740029 )
      {
        v12 = -1073741769;
      }
    }
    else
    {
LABEL_9:
      v12 = AlpcpReceiveLegacyMessage(v21, a4, a5, a2);
      if ( _bittestandreset((signed __int32 *)v25, 2u) )
      {
        v18 = v25[0] >> 22;
        LOBYTE(v18) = (v25[0] & 0x400000) != 0;
        AlpcpSignal(v21, 0LL, v18);
        if ( v23 )
        {
          ObfDereferenceObject(v23);
        }
      }
      ObfDereferenceObject(v15);
    }
  }
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v12;
}
80 row(s)
