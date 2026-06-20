__int64 __fastcall NtOpenPrivateNamespace(HANDLE *a1, ACCESS_MASK a2, __int64 a3, void *a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Handle = 0LL;
  AccessMode = KeGetCurrentThread()->PreviousMode;
  v22 = 0;
  if ( AccessMode )
  {
    v7 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
    {
      v7 = (__int64)a1;
    }
    *(_QWORD *)v7 = *(_QWORD *)v7;
    if ( a3 )
    {
      if ( (a3 & 7) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v22 = *(_DWORD *)(a3 + 24);
    }
  }
  else if ( a3 )
  {
    v22 = *(_DWORD *)(a3 + 24);
  }
  result = ObpCaptureBoundaryDescriptor(a4);
  if ( (int)result >= 0 )
  {
    CurrentServerSiloGlobals = PsGetCurrentServerSiloGlobals();
    CurrentThread = KeGetCurrentThread();
    --CurrentThread->KernelApcDisable;
    v11 = (signed __int64 *)(CurrentServerSiloGlobals + 720);
    v12 = KeAbPreAcquire(CurrentServerSiloGlobals + 720, 0LL, 0LL);
    v13 = v12;
    if ( _interlockedbittestandset64((volatile signed __int32 *)(CurrentServerSiloGlobals + 720), 0LL) )
    {
      ExfAcquirePushLockExclusiveEx(v11, v12, v11);
    }
    if ( v13 )
    {
      *(_BYTE *)(v13 + 10) = 1;
    }
    v14 = ObpLookupNamespaceEntry(CurrentServerSiloGlobals + 128, 0LL);
    ExFreePoolWithTag(0LL, 0x534E624Fu);
    if ( v14 && (v18 = *(void **)(v14 + 16)) != 0LL )
    {
      PsReferenceSiloContext(v18);
      _m_prefetchw(v11);
      v19 = *v11;
      v20 = *v11 - 16;
      if ( (*v11 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )
      {
        v20 = 0LL;
      }
      if ( (v19 & 2) != 0 || (v21 = *v11, v21 != _InterlockedCompareExchange64(v11, v20, v19)) )
      {
        ExfReleasePushLock(v11, v20);
      }
      KeAbPostRelease((ULONG_PTR)v11);
      KeLeaveCriticalRegion();
      P = ObOpenObjectByPointer(
            v18,
            v22 & (AccessMode != 0 ? 7666 : 73714),
            0LL,
            a2,
            ObpDirectoryObjectType,
            AccessMode,
            &Handle);
      ObfDereferenceObject(v18);
      *a1 = Handle;
      return P;
    }
    else
    {
      v15 = *v11;
      v16 = *v11 - 16;
      if ( (*v11 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )
      {
        v16 = 0LL;
      }
      _m_prefetchw(v11);
      if ( (v15 & 2) != 0 || (v17 = *v11, v17 != _InterlockedCompareExchange64(v11, v16, v15)) )
      {
        ExfReleasePushLock(v11, v16);
      }
      KeAbPostRelease((ULONG_PTR)v11);
      KeLeaveCriticalRegion();
      return 3221225530LL;
    }
  }
  return result;
}
95 row(s)
