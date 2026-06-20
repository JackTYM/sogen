__int64 __fastcall NtAllocateUuids(unsigned __int64 a1, unsigned __int64 a2, unsigned __int64 a3, unsigned __int64 a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v20[0] = 0LL;
  v18 = 0;
  v19 = 0;
  CurrentThread = KeGetCurrentThread();
  if ( CurrentThread->PreviousMode )
  {
    if ( (a1 & 3) != 0 )
    {
      goto LABEL_6;
    }
    v8 = 0x7FFFFFFF0000LL;
    v9 = 0x7FFFFFFF0000LL;
    if ( a1 < 0x7FFFFFFF0000LL )
    {
      v9 = a1;
    }
    *(_BYTE *)v9 = *(_BYTE *)v9;
    *(_BYTE *)(v9 + 7) = *(_BYTE *)(v9 + 7);
    if ( (a2 & 3) != 0 )
    {
      goto LABEL_6;
    }
    v10 = 0x7FFFFFFF0000LL;
    if ( a2 < 0x7FFFFFFF0000LL )
    {
      v10 = a2;
    }
    *(_DWORD *)v10 = *(_DWORD *)v10;
    if ( (a3 & 3) != 0 )
    {
LABEL_6:
      ExRaiseDatatypeMisalignment();
    }
    v11 = 0x7FFFFFFF0000LL;
    if ( a3 < 0x7FFFFFFF0000LL )
    {
      v11 = a3;
    }
    *(_DWORD *)v11 = *(_DWORD *)v11;
    if ( a4 < 0x7FFFFFFF0000LL )
    {
      v8 = a4;
    }
    *(_BYTE *)v8 = *(_BYTE *)v8;
    *(_BYTE *)(v8 + 5) = *(_BYTE *)(v8 + 5);
  }
  v12 = KeGetCurrentThread();
  --v12->KernelApcDisable;
  v13 = KeAbPreAcquire(&ExpUuidLock, 0LL, 0LL);
  v14 = v13;
  if ( _interlockedbittestandset64((volatile signed __int32 *)&ExpUuidLock, 0LL) )
  {
    ExfAcquirePushLockExclusiveEx(&ExpUuidLock, v13, &ExpUuidLock);
  }
  if ( v14 )
  {
    *(_BYTE *)(v14 + 10) = 1;
  }
  v15 = ExpAllocateUuids(v20, &v18, &v19);
  if ( v15 >= 0 )
  {
    ExpUuidSaveSequenceNumberIf();
    v17 = HIBYTE(NlsMbOemCodePageTag);
    if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&ExpUuidLock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
    {
      ExfTryToWakePushLock(&ExpUuidLock);
    }
    KeAbPostRelease((ULONG_PTR)&ExpUuidLock);
    KiLeaveCriticalRegionUnsafe(v12);
    *(_QWORD *)a1 = v20[0];
    *(_DWORD *)a2 = v18;
    *(_DWORD *)a3 = v19;
    *(_DWORD *)a4 = *(int *)((char *)&dword_140FD7254 + 2);
    *(_WORD *)(a4 + 4) = word_140FD725A;
    return v17 == 0 ? 0x40020056 : 0;
  }
  else
  {
    if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&ExpUuidLock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
    {
      ExfTryToWakePushLock(&ExpUuidLock);
    }
    KeAbPostRelease((ULONG_PTR)&ExpUuidLock);
    KiLeaveCriticalRegionUnsafe(v12);
    return (unsigned int)v15;
  }
}
