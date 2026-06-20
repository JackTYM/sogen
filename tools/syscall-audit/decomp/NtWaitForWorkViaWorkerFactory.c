__int64 __fastcall NtWaitForWorkViaWorkerFactory(
        HANDLE Handle,
        volatile void *Address,
        unsigned int a3,
        _DWORD *a4,
        __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v96 = a4;
  v91 = a3;
  v103 = Address;
  v90 = Handle;
  BugCheckParameter2 = (ULONG_PTR)Address;
  v93 = a3;
  v87 = (__int64)a4;
  v98 = 0LL;
  *(_OWORD *)Handlea = 0LL;
  v100 = 0LL;
  memset(&v88, 0, sizeof(v88));
  memset(v110, 0, sizeof(v110));
  v92 = 0;
  v7 = 0LL;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = CurrentThread->PreviousMode;
  v86 = PreviousMode;
  v85 = PreviousMode;
  P = v110;
  if ( a3 - 1 > 0x7FFFFFE )
  {
    v10 = -1073741811;
    goto LABEL_31;
  }
  if ( PreviousMode )
  {
    ProbeForWrite(Address, 32LL * a3, 8u);
    v9 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)v96 < 0x7FFFFFFF0000LL )
    {
      v9 = (__int64)v96;
    }
    *(_DWORD *)v9 = *(_DWORD *)v9;
    if ( (a5 & 7) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    *(_OWORD *)Handlea = *(_OWORD *)a5;
    v100 = *(_QWORD *)(a5 + 16);
  }
  else
  {
    *(_OWORD *)Handlea = *(_OWORD *)a5;
    v100 = *(_QWORD *)(a5 + 16);
  }
  Object = 0LL;
  v10 = ObReferenceObjectByHandle(Handle, 2u, ExpWorkerFactoryObjectType, PreviousMode, &Object, 0LL);
  v7 = (char *)Object;
  v101[1] = (__int64)Object;
  if ( v10 >= 0 )
  {
    if ( a3 > 0x10 )
    {
      P = (PVOID)ExAllocatePool2(0x40uLL, 8LL * a3, 0x656E6F4Eu);
      if ( !P )
      {
        v91 = 16;
        P = v110;
      }
    }
    v11 = (unsigned __int64 *)*((_QWORD *)v7 + 2);
    v88.LockQueue.Lock = v11;
    v88.LockQueue.Next = 0LL;
    CurrentIrql = KeGetCurrentIrql();
    __writecr8(2uLL);
    if ( KiIrqlFlags )
    {
      KiRaiseIrqlProcessIrqlFlags(CurrentIrql, 2LL);
    }
    v88.OldIrql = CurrentIrql;
    if ( (BYTE6(PerfGlobalGroupMask) & 0x21) == 0 || PopHibernateInProgress )
    {
      v13 = _InterlockedExchange64((volatile __int64 *)v11, (__int64)&v88);
      if ( v13 )
      {
        KxWaitForLockOwnerShip(&v88);
      }
    }
    else
    {
      KiAcquireQueuedSpinLockInstrumented(&v88, v11);
    }
    v14 = Object;
    v15 = *((_QWORD *)Object + 2);
    if ( *(_BYTE *)(v15 + 33) )
    {
      KeReleaseInStackQueuedSpinLock(&v88);
      v10 = 128;
      goto LABEL_31;
    }
    v16 = *((_DWORD *)Object + 102);
    if ( (v16 & 0x200) != 0 )
    {
      if ( (v16 & 0x400) != 0
        && KiDeregisterObjectWaitBlock(*(void **)(v15 + 8), (struct _KWAIT_BLOCK *)((char *)Object + 616)) )
      {
        ObfDereferenceObjectWithTag(v14, 0x746C6644u);
        *((_DWORD *)v14 + 102) &= ~0x400u;
      }
      *((_DWORD *)v14 + 102) &= ~0x200u;
    }
    ++*(_DWORD *)(*((_QWORD *)v14 + 2) + 28LL);
    while ( 1 )
    {
      if ( *((_DWORD *)v7 + 95) < *((_DWORD *)v7 + 96) || *(_BYTE *)(*((_QWORD *)v7 + 2) + 33LL) )
      {
        v10 = 258;
LABEL_94:
        --*(_DWORD *)(*((_QWORD *)v7 + 2) + 28LL);
        if ( v10 == 258 )
        {
          --*((_DWORD *)v7 + 96);
          --*((_DWORD *)v7 + 97);
          ExpRemoveCurrentThreadFromThreadHistory(v7);
        }
        else if ( (*((_DWORD *)v7 + 102) & 7) != 4 )
        {
          v56 = KeGetCurrentThread();
          v57 = 0;
          v58 = (char *)Object;
          while ( v57 < 4 )
          {
            if ( *((struct _KTHREAD **)Object + v57 + 9) == v56 )
            {
              goto LABEL_101;
            }
            ++v57;
          }
          ObfReferenceObjectWithTag(v56, 0x746C6644u);
          for ( i = 0; ; ++i )
          {
            if ( i >= 4 )
            {
              v59 = *((_DWORD *)v7 + 102) & 7;
              v60 = (PVOID *)&v7[8 * v59];
              ObfDereferenceObjectWithTag(v60[9], 0x746C6644u);
              v60[9] = v56;
              *((_DWORD *)v7 + 102) = *((_DWORD *)v7 + 102) & 0xFFFFFFF8 | ((_BYTE)v59 + 1) & 3;
              goto LABEL_101;
            }
            v71 = 8LL * i + 72;
            if ( !*(_QWORD *)&v58[v71] )
            {
              break;
            }
          }
          *(_QWORD *)&v7[v71] = v56;
        }
LABEL_101:
        v61 = *((_QWORD *)v7 + 2);
        if ( *((_DWORD *)v7 + 96) >= *((_DWORD *)v7 + 95) || *(_DWORD *)(v61 + 28) )
        {
LABEL_103:
          if ( (BYTE6(PerfGlobalGroupMask) & 1) != 0 && !PopHibernateInProgress )
          {
            KiReleaseQueuedSpinLockInstrumented(&v88, retaddr);
            goto LABEL_106;
          }
          _m_prefetchw(&v88);
          Next = v88.LockQueue.Next;
          if ( v88.LockQueue.Next )
          {
LABEL_114:
            v88.LockQueue.Next = 0LL;
            Lock = v88.LockQueue.Lock;
            if ( (((unsigned __int8)Lock ^ (unsigned __int8)_InterlockedExchange64(
                                                              (volatile __int64 *)&Next->Lock,
                                                              (__int64)v88.LockQueue.Lock)) & 4) != 0 )
            {
              _InterlockedOr(v82, 0);
              v65 = KeDisableInterrupts(Lock, v13, ((unsigned __int64)&Next->Lock >> 5) & 0x7F);
              KiHaltOnAddressWakeEntireList(v67, _InterlockedExchange64(&KiHaltOnAddressHashTable[v66], 0LL));
              if ( v65 )
              {
                SchedulerAssist = (signed __int32 *)KeGetCurrentPrcb()->SchedulerAssist;
                if ( SchedulerAssist )
                {
                  _m_prefetchw(SchedulerAssist);
                  v77 = *SchedulerAssist;
                  do
                  {
                    v78 = v77;
                    v77 = _InterlockedCompareExchange(SchedulerAssist, v77 & 0xFFDFFFFF, v77);
                  }
                  while ( v78 != v77 );
                  if ( (v77 & 0x200000) != 0 )
                  {
                    KiRemoveSystemWorkPriorityKick();
                  }
                }
                _enable();
              }
            }
          }
          else if ( (struct _KLOCK_QUEUE_HANDLE *)_InterlockedCompareExchange64(
                                                    (volatile signed __int64 *)v88.LockQueue.Lock,
                                                    0LL,
                                                    (signed __int64)&v88) != &v88 )
          {
            Next = (_KSPIN_LOCK_QUEUE *volatile)KxWaitForLockChainValid(&v88);
            goto LABEL_114;
          }
LABEL_106:
          OldIrql = v88.OldIrql;
          if ( KiIrqlFlags )
          {
            KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v88.OldIrql);
          }
          __writecr8(OldIrql);
        }
        else
        {
          if ( *((_DWORD *)v7 + 101) )
          {
            v69 = *((_DWORD *)v7 + 102) | 0x200;
            *((_DWORD *)v7 + 102) = v69;
            if ( !*(_DWORD *)(*(_QWORD *)(v61 + 8) + 4LL) )
            {
              if ( (v69 & 0x400) == 0 )
              {
                *((_DWORD *)v7 + 102) = v69 | 0x400;
                ObfReferenceObjectWithTag(v7, 0x746C6644u);
                KeRegisterObjectNotification(*(_QWORD *)(v61 + 8), &ExpWorkerFactoryManagerQueue, v7 + 616);
              }
              goto LABEL_103;
            }
          }
          ExpWorkerFactoryCheckCreate(v7, &v88);
        }
        if ( !v10 )
        {
          *v96 = v92;
        }
        break;
      }
      if ( (BYTE6(PerfGlobalGroupMask) & 1) == 0 || PopHibernateInProgress )
      {
        _m_prefetchw(&v88);
        v17 = v88.LockQueue.Next;
        if ( !v88.LockQueue.Next )
        {
          if ( (struct _KLOCK_QUEUE_HANDLE *)_InterlockedCompareExchange64(
                                               (volatile signed __int64 *)v88.LockQueue.Lock,
                                               0LL,
                                               (signed __int64)&v88) == &v88 )
          {
            goto LABEL_47;
          }
          v17 = (_KSPIN_LOCK_QUEUE *volatile)KxWaitForLockChainValid(&v88);
        }
        v88.LockQueue.Next = 0LL;
        v18 = (unsigned __int8)v88.LockQueue.Lock;
        if ( ((v18 ^ (unsigned __int8)_InterlockedExchange64(
                                        (volatile __int64 *)&v17->Lock,
                                        (__int64)v88.LockQueue.Lock)) & 4) != 0 )
        {
          KeWakeAddressAll();
        }
      }
      else
      {
        KiReleaseQueuedSpinLockInstrumented(&v88, retaddr);
      }
LABEL_47:
      v23 = v88.OldIrql;
      if ( KiIrqlFlags )
      {
        KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v88.OldIrql);
      }
      __writecr8(v23);
      v24 = BYTE4(v100);
      if ( (v100 & 0x100000000LL) == 0 )
      {
        goto LABEL_82;
      }
      v25 = Handlea[0];
      v26 = v100;
      v27 = Handlea[1];
      *(_OWORD *)v104 = 0LL;
      v105 = 0LL;
      v106 = 0LL;
      v107 = 0LL;
      v28 = KeGetCurrentThread();
      --v28->KernelApcDisable;
      v98 = 0LL;
      v29 = v26 & 0xFFFF0000;
      if ( (v29 & 0x20000) == 0 )
      {
        v94 = 0LL;
        if ( ObReferenceObjectByHandle(v27, 1u, AlpcPortObjectType, PreviousMode, &v94, 0LL) >= 0 )
        {
          v30 = v94;
          if ( (v29 & 0x40000) != 0 )
          {
            v31 = _InterlockedIncrement((volatile signed __int32 *)v94 + 101);
            v30 = v94;
            v90 = v94;
            if ( *((_QWORD *)v94 + 51) )
            {
              BugCheckParameter2 = (ULONG_PTR)v94 + 352;
              v79 = KeAbPreAcquire((char *)v94 + 352, 0LL, 0LL);
              v87 = v79;
              v80 = (volatile signed __int64 *)BugCheckParameter2;
              if ( _interlockedbittestandset64((volatile signed __int32 *)BugCheckParameter2, 0LL) )
              {
                ExfAcquirePushLockExclusiveEx(v80, v79, v80);
                v80 = (volatile signed __int64 *)BugCheckParameter2;
                v79 = v87;
              }
              if ( v79 )
              {
                *(_BYTE *)(v79 + 10) = 1;
              }
              v81 = *((_QWORD *)v90 + 51);
              if ( v81 && v31 == *(_DWORD *)(v81 + 24) )
              {
                KeSetEvent(*((PRKEVENT *)v90 + 51), 0, 0);
                v80 = (volatile signed __int64 *)BugCheckParameter2;
              }
              if ( (_InterlockedExchangeAdd64(v80, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
              {
                ExfTryToWakePushLock();
                v80 = (volatile signed __int64 *)BugCheckParameter2;
              }
              KeAbPostRelease((ULONG_PTR)v80);
              v30 = v94;
            }
          }
          v104[0] = v30;
          LODWORD(v107) = v29 | 4;
          v106 = 0uLL;
          *((_QWORD *)&v105 + 1) = 0LL;
          if ( (int)AlpcpSendMessage(v104, v25, 0LL, PreviousMode) < 0 )
          {
            ObfDereferenceObject(v104[0]);
          }
          else
          {
            v32 = v106;
            v90 = (PVOID)v106;
            *(_QWORD *)&v98 = v106;
            *((PVOID *)&v98 + 1) = v104[0];
            if ( !(_QWORD)v106 )
            {
              if ( *((_QWORD *)&v105 + 1) )
              {
                KeReleaseSemaphoreEx((PVOID)(*((_QWORD *)&v105 + 1) + 1304LL), 5, 0LL);
              }
              else if ( (BYTE8(v106) & 1) != 0 )
              {
                if ( (*((_QWORD *)&v106 + 1) & 0xFFFFFFFFFFFFFFFCuLL) != 0 )
                {
                  KeSetEvent((PRKEVENT)(*((_QWORD *)&v106 + 1) & 0xFFFFFFFFFFFFFFFCuLL), 0, 1u);
                  AlpcpReleaseDirectAttribute(*((_QWORD *)&v106 + 1));
                }
                *((_QWORD *)&v106 + 1) = 0LL;
              }
              goto LABEL_81;
            }
            if ( BYTE10(v107) )
            {
              v85 = BYTE11(v107);
              *(_QWORD *)&LockHandle.OldIrql = 0LL;
              v33 = *(_QWORD *)(v106 + 32);
              v34 = *(_QWORD *)(v106 + 48);
              LockHandle.LockQueue.Lock = (unsigned __int64 *volatile)v34;
              LockHandle.LockQueue.Next = 0LL;
              v35 = KeGetCurrentIrql();
              v87 = v35;
              __writecr8(2uLL);
              if ( KiIrqlFlags )
              {
                KiRaiseIrqlProcessIrqlFlags(v35, 2LL);
                LOBYTE(v35) = v87;
              }
              LockHandle.OldIrql = v35;
              if ( (BYTE6(PerfGlobalGroupMask) & 0x21) == 0 || PopHibernateInProgress )
              {
                if ( _InterlockedExchange64((volatile __int64 *)v34, (__int64)&LockHandle) )
                {
                  KxWaitForLockOwnerShip(&LockHandle);
                }
              }
              else
              {
                KiAcquireQueuedSpinLockInstrumented(&LockHandle, v34);
              }
              v36 = *(_DWORD *)(v34 + 12);
              if ( v36 >= *(_DWORD *)(v34 + 8) )
              {
                KeReleaseInStackQueuedSpinLock(&LockHandle);
                if ( !(unsigned int)AlpcpQueueIoCompletion(v33, *(_QWORD *)(v32 + 40), -(v85 != 0), 0, 0, 0) )
                {
                  KeAcquireInStackQueuedSpinLock((PKSPIN_LOCK)v34, &LockHandle);
                  if ( v85 )
                  {
                    ++*(_DWORD *)(v34 + 20);
                  }
                  else
                  {
                    ++*(_DWORD *)(v34 + 16);
                  }
                  KeReleaseInStackQueuedSpinLock(&LockHandle);
                }
                if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v32 + 352), 0LL, 17LL) != 17 )
                {
                  ExfReleasePushLockShared(v32 + 352);
                }
                KeAbPostRelease(v32 + 352);
                goto LABEL_81;
              }
              v37 = *(_QWORD **)(v34 + 32);
              if ( v37 )
              {
                *(_QWORD *)(v34 + 32) = *v37;
              }
              MiniCompletionPacket = v37[1];
              *(_DWORD *)(v34 + 12) = v36 + 1;
              KeReleaseInStackQueuedSpinLock(&LockHandle);
              v39 = v90;
              v40 = (ULONG_PTR)v90 + 352;
              if ( _InterlockedCompareExchange64((volatile signed __int64 *)v90 + 44, 0LL, 17LL) != 17 )
              {
                ExfReleasePushLockShared(v40);
                v40 = (ULONG_PTR)(v39 + 44);
              }
              KeAbPostRelease(v40);
              v42 = v85 != 0;
              v85 = -v85;
              v43 = -(__int64)v42;
              v87 = v43;
              v44 = v39[5];
              if ( MiniCompletionPacket )
              {
                goto LABEL_68;
              }
              LOBYTE(v41) = 1;
              MiniCompletionPacket = IopAllocateMiniCompletionPacket(v41, 0LL);
              if ( MiniCompletionPacket )
              {
                v43 = v87;
LABEL_68:
                *(_QWORD *)(MiniCompletionPacket + 24) = v44;
                *(_QWORD *)(MiniCompletionPacket + 32) = v43;
                *(_DWORD *)(MiniCompletionPacket + 40) = 0;
                *(_QWORD *)(MiniCompletionPacket + 48) = 0LL;
                v85 = 0;
                v45 = KeGetCurrentIrql();
                v87 = v45;
                __writecr8(2uLL);
                if ( KiIrqlFlags )
                {
                  KiRaiseIrqlProcessIrqlFlags(v45, 2LL);
                }
                CurrentPrcb = KeGetCurrentPrcb();
                v47 = CurrentPrcb->CurrentThread;
                v90 = v47;
                if ( (DWORD1(PerfGlobalGroupMask) & 0x1000000) != 0 )
                {
                  IsThreadRunning = KeIsThreadRunning(v47);
                  EtwTraceEnqueueWork(v76, MiniCompletionPacket, IsThreadRunning);
                  v47 = (_KTHREAD *)v90;
                }
                LODWORD(BugCheckParameter2) = ((unsigned __int8)~*(_BYTE *)(v33 + 1) >> 1) & 1;
                p_Lock = v47[1].WaitBlock[1].Object;
                if ( !p_Lock )
                {
                  p_Lock = &v47->Header.Lock;
                }
                v93 = *(_DWORD *)(p_Lock[68] + 136LL) & 0x2000;
                KiAcquireKobjectLockSafe(v33);
                v108 = *(_DWORD *)(v33 + 4);
                v49 = (_QWORD *)(v33 + 8);
                if ( (_QWORD *)*v49 == v49
                  || *(_DWORD *)(v33 + 40) >= *(_DWORD *)(v33 + 44) && !v93
                  || *((_QWORD *)v90 + 29) == v33 && *((_BYTE *)v90 + 643) == 15 )
                {
                  v50 = v85;
                }
                else
                {
                  v50 = KiWakeQueueWaiter(CurrentPrcb, v33, MiniCompletionPacket);
                  v49 = (_QWORD *)(v33 + 8);
                }
                if ( v50 )
                {
                  *(_QWORD *)MiniCompletionPacket = 0LL;
                }
                else
                {
                  v72 = *(_DWORD *)(v33 + 4);
                  *(_DWORD *)(v33 + 4) = v72 + 1;
                  v73 = *(__int64 **)(v33 + 32);
                  if ( *v73 != v33 + 24 )
                  {
                    __fastfail(3u);
                  }
                  *(_QWORD *)MiniCompletionPacket = v33 + 24;
                  *(_QWORD *)(MiniCompletionPacket + 8) = v73;
                  *v73 = MiniCompletionPacket;
                  *(_QWORD *)(v33 + 32) = MiniCompletionPacket;
                  if ( !v72 && (_QWORD *)*v49 != v49 )
                  {
                    KiWakeOtherQueueWaiters(CurrentPrcb, v33);
                  }
                }
                _InterlockedAnd((volatile signed __int32 *)v33, 0xFFFFFF7F);
                KiExitDispatcher(CurrentPrcb, v87);
              }
            }
            else
            {
              v74 = *(void **)(v106 + 248);
              LODWORD(v101[0]) = 0;
              KeReleaseSemaphoreEx(v74, 9, (__int64)v101);
            }
          }
        }
      }
LABEL_81:
      KeLeaveCriticalRegionThread(KeGetCurrentThread());
      v24 = BYTE4(v100);
LABEL_82:
      if ( (v24 & 2) != 0 )
      {
        v51 = KeGetCurrentThread();
        if ( (v51->MiscFlags & 4) == 0 )
        {
          v52 = KeGetCurrentIrql();
          __writecr8(2uLL);
          if ( KiIrqlFlags )
          {
            KiRaiseIrqlProcessIrqlFlags(v52, 2LL);
          }
          v51->WaitIrql = v52;
          v51->MiscFlags |= 4u;
        }
        PspRevertContainerImpersonation((ULONG_PTR)KeGetCurrentThread());
      }
      v53 = *((_QWORD *)v7 + 2);
      LOBYTE(v84) = 1;
      LOBYTE(HandleInformation) = PreviousMode;
      v10 = IoRemoveIoCompletion(*(_QWORD *)(v53 + 8), v103, P, v91, &v92, (_DWORD)HandleInformation, 0LL, v84);
      ExpWorkerFactoryFinishDeferredWork(&v98);
      v54 = (unsigned __int64 *)*((_QWORD *)v7 + 2);
      v88.LockQueue.Lock = v54;
      v88.LockQueue.Next = 0LL;
      v55 = KeGetCurrentIrql();
      __writecr8(2uLL);
      if ( KiIrqlFlags )
      {
        KiRaiseIrqlProcessIrqlFlags(v55, 2LL);
      }
      v88.OldIrql = v55;
      if ( (BYTE6(PerfGlobalGroupMask) & 0x21) == 0 || PopHibernateInProgress )
      {
        v13 = _InterlockedExchange64((volatile __int64 *)v54, (__int64)&v88);
        if ( v13 )
        {
          KxWaitForLockOwnerShip(&v88);
        }
      }
      else
      {
        KiAcquireQueuedSpinLockInstrumented(&v88, v54);
      }
      if ( v10 != 258
        || !(unsigned __int8)ExpWorkerFactoryWantsToCreate(v7, 1LL)
        && *((_DWORD *)v7 + 96) > *((_DWORD *)v7 + 94)
        && *(struct _KTHREAD **)&CurrentThread[1].SystemCallNumber == (struct _KTHREAD *)&CurrentThread[1].SystemCallNumber )
      {
        goto LABEL_94;
      }
    }
  }
LABEL_31:
  if ( P != v110 )
  {
    ExFreePoolWithTag(P, 0);
  }
  if ( v7 )
  {
    if ( ObpTraceFlags )
    {
      ObpPushStackInfo((_DWORD)v7 - 48);
    }
    v19 = _InterlockedExchangeAdd64((volatile signed __int64 *)v7 - 6, 0xFFFFFFFFFFFFFFFFuLL);
    v20 = v19 <= 1;
    v21 = v19 - 1;
    if ( v20 )
    {
      if ( *((_QWORD *)v7 - 5) )
      {
        KeBugCheckEx(
          0x18u,
          ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ (unsigned __int8)*(v7 - 24) ^ (unsigned __int64)(unsigned __int8)((unsigned __int16)((_WORD)v7 - 48) >> 8)],
          (ULONG_PTR)v7,
          1uLL,
          *((_QWORD *)v7 - 5));
      }
      if ( v21 < 0 )
      {
        KeBugCheckEx(0x18u, 0LL, (ULONG_PTR)v7, 2uLL, v21);
      }
      if ( KeGetCurrentThread()->SpecialApcDisable || !(unsigned __int8)KeAreInterruptsEnabled() || KeGetCurrentIrql() )
      {
        ObpDeferObjectDeletion(v7 - 48);
      }
      else
      {
        if ( (*(v7 - 22) & 0x40) != 0 && *(_BYTE *)(*(_QWORD *)&v7[-ObpInfoMaskToOffset[*(v7 - 22) & 0x7F] - 48] + 24LL) )
        {
          ObpHandleRevocationBlockRemoveObject();
        }
        if ( ObpTraceFlags )
        {
          ObpDeregisterObject(v7 - 48);
        }
        ObpRemoveObjectRoutine(v7 - 48, 0LL);
      }
    }
  }
  if ( (v100 & 0x100000000LL) != 0 )
  {
    NtAlpcSendWaitReceivePort(Handlea[1], v100, Handlea[0], 0, 0LL, 0LL, 0LL, 0LL);
  }
  return (unsigned int)v10;
}
637 row(s)
