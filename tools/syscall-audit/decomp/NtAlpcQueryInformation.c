__int64 __fastcall NtAlpcQueryInformation(
        HANDLE Handle,
        int a2,
        unsigned __int64 a3,
        unsigned int a4,
        unsigned __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v6 = a3;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( !a3 )
  {
    v15 = -1073741811;
    goto LABEL_24;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    if ( ((a2 - 3) & 0xFFFFFFF7) != 0 && a4 )
    {
      v10 = a3;
      if ( (a3 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v11 = a4 + a3 - 1;
      if ( a3 > v11 || (a3 = 0x7FFFFFFF0000LL, v11 >= 0x7FFFFFFF0000LL) )
      {
        ExRaiseAccessViolation();
      }
      v12 = (v11 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      do
      {
        *(_BYTE *)v10 = *(_BYTE *)v10;
        v10 = (v10 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      }
      while ( v10 != v12 );
    }
    else
    {
      a3 = 0x7FFFFFFF0000LL;
    }
    v13 = a5;
    if ( a5 )
    {
      if ( a5 < 0x7FFFFFFF0000LL )
      {
        a3 = a5;
      }
      *(_DWORD *)a3 = *(_DWORD *)a3;
    }
  }
  else
  {
    v13 = a5;
  }
  v14 = 0LL;
  if ( !Handle
    || (v22 = 0LL,
        v15 = ObReferenceObjectByHandle(Handle, 0x20000u, AlpcPortObjectType, PreviousMode, &v22, 0LL),
        v14 = v22,
        v15 >= 0) )
  {
    if ( a2 )
    {
      v18 = a2 - 3;
      if ( v18 )
      {
        v19 = v18 - 1;
        if ( v19 )
        {
          v20 = v19 - 7;
          if ( v20 )
          {
            if ( v20 != 1 )
            {
              v15 = -1073741811;
LABEL_22:
              if ( v14 )
              {
                ObfDereferenceObject(v14);
              }
              goto LABEL_24;
            }
            ServerSessionInfo = AlpcpPortQueryServerSessionInfo(v14, v6, a4, v13);
          }
          else
          {
            ServerSessionInfo = AlpcpWaitForPortReferences((_DWORD)v14, v6, a4, v13, PreviousMode);
          }
        }
        else
        {
          ServerSessionInfo = AlpcpPortQueryServerInfo((int)v14, v6, a4, v13, PreviousMode);
        }
      }
      else
      {
        LOBYTE(Object) = PreviousMode;
        ServerSessionInfo = AlpcpPortQueryConnectedSidInfo(v14, v6, a3, v13, Object);
      }
    }
    else
    {
      ServerSessionInfo = AlpcpPortQueryBasicInfo(v14, v6, a4, v13);
    }
    v15 = ServerSessionInfo;
    goto LABEL_22;
  }
LABEL_24:
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v15;
}
115 row(s)
