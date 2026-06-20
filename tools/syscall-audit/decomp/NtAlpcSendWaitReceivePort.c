__int64 __fastcall NtAlpcSendWaitReceivePort(
        void *a1,
        int a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v25 = 0LL;
  v26 = 0LL;
  v27 = 0LL;
  v28 = 0LL;
  CurrentThread = KeGetCurrentThread();
  v11 = a2 & 0xFFFF0000;
  --CurrentThread->KernelApcDisable;
  Object = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v13 = ObReferenceObjectByHandle(a1, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v13 >= 0 )
  {
    if ( (v11 & 0x40000) != 0 )
    {
      v15 = _InterlockedExchangeAdd((volatile signed __int32 *)Object + 101, 1u);
      v16 = Object;
      v17 = v15 + 1;
      if ( !*((_QWORD *)Object + 51) )
      {
        goto LABEL_5;
      }
      v20 = (volatile signed __int32 *)((char *)Object + 352);
      v21 = KeAbPreAcquire((char *)Object + 352, 0LL, 0LL);
      v18 = _interlockedbittestandset64(v20, 0LL);
      v24 = v21;
      if ( v18 )
      {
        ExfAcquirePushLockExclusiveEx(v16 + 44, v21, v16 + 44);
        v21 = v24;
      }
      if ( v21 )
      {
        *(_BYTE *)(v21 + 10) = 1;
      }
      v22 = (struct _KEVENT *)v16[51];
      if ( v22 && v17 == v22[1].Header.LockNV )
      {
        KeSetEvent(v22, 0, 0);
      }
      if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v20, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
      {
        ExfTryToWakePushLock(v16 + 44);
      }
      KeAbPostRelease((ULONG_PTR)(v16 + 44));
    }
    v16 = Object;
LABEL_5:
    if ( (v11 & 0x20000) != 0 )
    {
      if ( a3 && (v11 & 0x10000) == 0 && (v11 & 0x1000000) == 0 )
      {
        if ( a5 )
        {
          v13 = AlpcpProcessSynchronousRequest((_DWORD)v16, v11, a3, a4, a5, a6, a7, a8, PreviousMode);
        }
        else
        {
          v13 = -1073740027;
        }
        goto LABEL_17;
      }
    }
    else
    {
      *(_QWORD *)&v25 = v16;
      LODWORD(v28) = v11;
      if ( !a3 )
      {
LABEL_7:
        if ( a5 )
        {
          v13 = AlpcpReceiveMessage(&v25, a5, a6, a7, a8);
        }
        v18 = _bittestandreset((signed __int32 *)&v28, 2u);
        if ( v18 )
        {
          v19 = (unsigned int)v28 >> 22;
          LOBYTE(v19) = (v28 & 0x400000) != 0;
          AlpcpSignal(&v25, 0LL, v19);
          if ( (_QWORD)v27 )
          {
            ObfDereferenceObject((PVOID)v27);
          }
        }
        goto LABEL_17;
      }
      if ( (v11 & 0x1000000) == 0 )
      {
        v27 = 0uLL;
        LODWORD(v28) = v11 | 4;
        *((_QWORD *)&v26 + 1) = 0LL;
        v13 = AlpcpSendMessage(&v25, a3, a4, PreviousMode);
        if ( v13 >= 0 )
        {
          goto LABEL_7;
        }
LABEL_17:
        ObfDereferenceObject(v16);
        goto LABEL_2;
      }
    }
    v13 = -1073741584;
    goto LABEL_17;
  }
LABEL_2:
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v13;
}
120 row(s)
