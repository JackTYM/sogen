NTSTATUS __fastcall NtReleaseWorkerFactoryWorker(void *a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset(&LockHandle, 0, sizeof(LockHandle));
  BugCheckParameter2 = 0LL;
  result = ObReferenceObjectByHandle(
             a1,
             1u,
             ExpWorkerFactoryObjectType,
             KeGetCurrentThread()->PreviousMode,
             (PVOID *)&BugCheckParameter2,
             0LL);
  if ( result < 0 )
  {
    return result;
  }
  v2 = *(unsigned __int64 **)(BugCheckParameter2 + 16);
  LockHandle.LockQueue.Lock = v2;
  LockHandle.LockQueue.Next = 0LL;
  CurrentIrql = KeGetCurrentIrql();
  __writecr8(2uLL);
  if ( KiIrqlFlags )
  {
    KiRaiseIrqlProcessIrqlFlags(CurrentIrql, 2LL);
  }
  LockHandle.OldIrql = CurrentIrql;
  if ( (BYTE6(PerfGlobalGroupMask) & 0x21) == 0 || PopHibernateInProgress )
  {
    if ( _InterlockedExchange64((volatile __int64 *)v2, (__int64)&LockHandle) )
    {
      KxWaitForLockOwnerShip(&LockHandle);
    }
  }
  else
  {
    KiAcquireQueuedSpinLockInstrumented(&LockHandle, v2);
  }
  v4 = BugCheckParameter2;
  v5 = *(_QWORD *)(BugCheckParameter2 + 16);
  if ( *(_BYTE *)(v5 + 33) )
  {
    v7 = 128;
  }
  else
  {
    v6 = *(_DWORD *)(v5 + 24);
    if ( v6 == -1 )
    {
      v7 = -1073741823;
    }
    else
    {
      v7 = 0;
      *(_DWORD *)(v5 + 24) = v6 + 1;
      v8 = *(_QWORD *)(v4 + 16);
      if ( !*(_BYTE *)(v8 + 32) )
      {
        *(_BYTE *)(v8 + 32) = 1;
        if ( (*(_DWORD *)(v4 + 408) & 0x200) != 0 )
        {
          ExpLeaveWorkerFactoryAwayMode((PVOID)v4);
        }
        KeReleaseInStackQueuedSpinLock(&LockHandle);
        v10 = *(_QWORD *)(v4 + 16);
        MiniCompletionPacket = *(_QWORD *)(v10 + 16);
        v12 = *(_QWORD *)(v10 + 8);
        if ( MiniCompletionPacket
          || (LOBYTE(v9) = 1, (MiniCompletionPacket = IopAllocateMiniCompletionPacket(v9, 0LL)) != 0) )
        {
          *(_QWORD *)(MiniCompletionPacket + 24) = 0LL;
          v13 = (_QWORD *)(v12 + 8);
          *(_QWORD *)(MiniCompletionPacket + 32) = 0LL;
          *(_DWORD *)(MiniCompletionPacket + 40) = 0;
          *(_QWORD *)(MiniCompletionPacket + 48) = 0LL;
          v14 = KeGetCurrentIrql();
          __writecr8(2uLL);
          if ( KiIrqlFlags )
          {
            KiRaiseIrqlProcessIrqlFlags(v14, 2LL);
          }
          CurrentPrcb = KeGetCurrentPrcb();
          CurrentThread = CurrentPrcb->CurrentThread;
          if ( (DWORD1(PerfGlobalGroupMask) & 0x1000000) != 0 )
          {
            IsThreadRunning = KeIsThreadRunning(CurrentPrcb->CurrentThread);
            EtwTraceEnqueueWork(CurrentThread, MiniCompletionPacket, IsThreadRunning);
          }
          KiAcquireKobjectLockSafe(v12);
          if ( (_QWORD *)*v13 == v13
            || *(_DWORD *)(v12 + 40) >= *(_DWORD *)(v12 + 44)
            || CurrentThread->Queue == (_DISPATCHER_HEADER *volatile)v12 && CurrentThread->WaitReason == 15
            || !(unsigned __int8)KiWakeQueueWaiter(CurrentPrcb, v12, MiniCompletionPacket) )
          {
            v21 = *(_DWORD *)(v12 + 4);
            *(_DWORD *)(v12 + 4) = v21 + 1;
            v22 = *(__int64 **)(v12 + 32);
            if ( *v22 != v12 + 24 )
            {
              __fastfail(3u);
            }
            *(_QWORD *)MiniCompletionPacket = v12 + 24;
            *(_QWORD *)(MiniCompletionPacket + 8) = v22;
            *v22 = MiniCompletionPacket;
            *(_QWORD *)(v12 + 32) = MiniCompletionPacket;
            if ( !v21 && (_QWORD *)*v13 != v13 )
            {
              KiWakeOtherQueueWaiters(CurrentPrcb, v12);
            }
          }
          else
          {
            *(_QWORD *)MiniCompletionPacket = 0LL;
          }
          _InterlockedAnd((volatile signed __int32 *)v12, 0xFFFFFF7F);
          KiExitDispatcher(CurrentPrcb, v14);
          v4 = BugCheckParameter2;
        }
        ExpWorkerFactoryCheckCreate((PVOID)v4, 0LL);
        goto LABEL_24;
      }
    }
  }
  KeReleaseInStackQueuedSpinLock(&LockHandle);
LABEL_24:
  v17 = v4 - 48;
  if ( ObpTraceFlags )
  {
    ObpPushStackInfo(v17);
  }
  v18 = _InterlockedExchangeAdd64((volatile signed __int64 *)v17, 0xFFFFFFFFFFFFFFFFuLL);
  v19 = v18 <= 1;
  BugCheckParameter4 = v18 - 1;
  if ( v19 )
  {
    if ( *(_QWORD *)(v17 + 8) )
    {
      KeBugCheckEx(
        0x18u,
        ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ *(unsigned __int8 *)(v17 + 24) ^ (unsigned __int64)BYTE1(v17)],
        BugCheckParameter2,
        1uLL,
        *(_QWORD *)(v17 + 8));
    }
    if ( BugCheckParameter4 < 0 )
    {
      KeBugCheckEx(0x18u, 0LL, BugCheckParameter2, 2uLL, BugCheckParameter4);
    }
    if ( KeGetCurrentThread()->SpecialApcDisable || !(unsigned __int8)KeAreInterruptsEnabled() || KeGetCurrentIrql() )
    {
      ObpDeferObjectDeletion(v17);
    }
    else
    {
      if ( (*(_BYTE *)(v17 + 26) & 0x40) != 0
        && *(_BYTE *)(*(_QWORD *)(v17 - ObpInfoMaskToOffset[*(_BYTE *)(v17 + 26) & 0x7F]) + 24LL) )
      {
        ObpHandleRevocationBlockRemoveObject();
      }
      if ( ObpTraceFlags )
      {
        ObpDeregisterObject(v17);
      }
      ObpRemoveObjectRoutine(v17, 0LL);
    }
  }
  return v7;
}
168 row(s)
