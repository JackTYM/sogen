__int64 __fastcall NtCancelWaitCompletionPacket(void *a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  memset(&LockHandle, 0, sizeof(LockHandle));
  Object = 0LL;
  v1 = ObReferenceObjectByHandle(
         a1,
         1u,
         IopWaitCompletionPacketObjectType,
         KeGetCurrentThread()->PreviousMode,
         &Object,
         0LL);
  v2 = Object;
  v3 = v1;
  if ( v1 >= 0 )
  {
    v4 = (KSPIN_LOCK *)((char *)Object + 96);
    v5 = KeAcquireSpinLockRaiseToDpc((PKSPIN_LOCK)Object + 12);
    v6 = (KSPIN_LOCK *)*((_QWORD *)v2 + 11);
    v7 = v5;
    if ( v6 )
    {
      ObfReferenceObjectWithTag(*((PVOID *)v2 + 11), 0x746C6644u);
    }
    KeReleaseSpinLock((PKSPIN_LOCK)v2 + 12, v7);
    if ( !v6 )
    {
      v3 = -1073741536;
      goto LABEL_18;
    }
    KeAcquireInStackQueuedSpinLock(v6 + 8, &LockHandle);
    v8 = KeAcquireSpinLockRaiseToDpc((PKSPIN_LOCK)v2 + 12);
    v9 = (unsigned int)Feature_1806129466__private_IsEnabledDeviceUsageNoInline() == 0;
    v10 = *((_BYTE *)v2 + 104);
    if ( v9 )
    {
      if ( !v10 )
      {
        goto LABEL_8;
      }
      v2 = Object;
    }
    else
    {
      if ( !v10 )
      {
LABEL_8:
        v3 = -1073741536;
LABEL_16:
        KeReleaseSpinLock(v4, v8);
        v2 = Object;
        goto LABEL_17;
      }
      v2 = Object;
      if ( *((KSPIN_LOCK **)Object + 11) != v6 )
      {
        v3 = -1073700861;
        KeReleaseSpinLock(v4, v8);
        goto LABEL_17;
      }
    }
    if ( !(unsigned __int8)IopCancelWaitCompletionPacket((struct _KWAIT_BLOCK *)v2) )
    {
      if ( *((_BYTE *)v2 + 104) )
      {
        v3 = 259;
      }
      goto LABEL_16;
    }
LABEL_17:
    KeReleaseInStackQueuedSpinLock(&LockHandle);
    ObfDereferenceObjectWithTag(v6, 0x746C6644u);
  }
LABEL_18:
  if ( v2 )
  {
    ObfDereferenceObjectWithTag(v2, 0x746C6644u);
  }
  return v3;
}
