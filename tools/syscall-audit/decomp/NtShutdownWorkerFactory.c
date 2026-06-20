__int64 __fastcall NtShutdownWorkerFactory(void *a1, volatile signed __int32 *a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset(&LockHandle, 0, sizeof(LockHandle));
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode && ((unsigned __int8)a2 & 3) != 0 )
  {
    ExRaiseDatatypeMisalignment();
  }
  Object = 0LL;
  v4 = ObReferenceObjectByHandle(a1, 0x20u, ExpWorkerFactoryObjectType, PreviousMode, &Object, 0LL);
  if ( v4 >= 0 )
  {
    v5 = (struct _EX_RUNDOWN_REF *)Object;
    ExpShutdownWorkerFactory(Object);
    ExWaitForRundownProtectionRelease(v5 + 13);
    v5 += 49;
    v9 = v5;
    _InterlockedAdd(a2, v5->Count);
    KeAcquireInStackQueuedSpinLock(*((PKSPIN_LOCK *)Object + 2), &LockHandle);
    LODWORD(v5->Count) = 0;
    KeReleaseInStackQueuedSpinLock(&LockHandle);
    ObfDereferenceObjectWithTag(Object, 0x746C6644u);
  }
  return (unsigned int)v4;
}
27 row(s)
