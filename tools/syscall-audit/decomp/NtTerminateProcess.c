__int64 __fastcall NtTerminateProcess(ULONG_PTR a1, unsigned int a2)                                                                  | 
{                                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                       | 
                                                                                                                                      | 
  CurrentThread = KeGetCurrentThread();                                                                                               | 
  v3 = 0;                                                                                                                             | 
  Object = 0LL;                                                                                                                       | 
  Process = CurrentThread->ApcState.Process;                                                                                          | 
  PreviousMode = CurrentThread->PreviousMode;                                                                                         | 
  if ( a1 )                                                                                                                           | 
  {                                                                                                                                   | 
    result = ObpReferenceObjectByHandleWithTag(a1, 0x65547350u, (__int64)&Object, 0LL, 0LL);                                          | 
    if ( (int)result < 0 )                                                                                                            | 
    {                                                                                                                                 | 
      return result;                                                                                                                  | 
    }                                                                                                                                 | 
    IsEnabledDeviceUsageNoInline = Feature_Servicing_ZwTerminateMinimalProcess_Terminate_Fix__private_IsEnabledDeviceUsageNoInline(); | 
    v9 = (struct _KPROCESS *)Object;                                                                                                  | 
    if ( IsEnabledDeviceUsageNoInline && (*((_DWORD *)Object + 383) & 1) != 0 && !*((_QWORD *)Object + 200) )                         | 
    {                                                                                                                                 | 
      if ( PreviousMode )                                                                                                             | 
      {                                                                                                                               | 
        v3 = -1073741790;                                                                                                             | 
      }                                                                                                                               | 
      else if ( Object == Process )                                                                                                   | 
      {                                                                                                                               | 
        v3 = -1073741637;                                                                                                             | 
      }                                                                                                                               | 
      else                                                                                                                            | 
      {                                                                                                                               | 
        PsTerminateMinimalProcess((PRKPROCESS)Object);                                                                                | 
      }                                                                                                                               | 
      ObfDereferenceObjectWithTag(v9, 0x65547350u);                                                                                   | 
      return v3;                                                                                                                      | 
    }                                                                                                                                 | 
    v10 = *((_DWORD *)Object + 116);                                                                                                  | 
    ProcessStartKey = PsGetProcessStartKey(Object);                                                                                   | 
    TimeQuadPart = PsGetProcessCreateTimeQuadPart(v9);                                                                                | 
    --CurrentThread->KernelApcDisable;                                                                                                | 
    v3 = PspTerminateProcess(v9);                                                                                                     | 
    ObfDereferenceObjectWithTag(v9, 0x65547350u);                                                                                     | 
    if ( v9 == Process )                                                                                                              | 
    {                                                                                                                                 | 
      if ( PreviousMode == 1 || (Process->SecureState.SecureHandle & 1) != 0 )                                                        | 
      {                                                                                                                               | 
        _InterlockedOr((volatile signed __int32 *)&CurrentThread[1].SwapListEntry + 2, 1u);                                           | 
        KeForceResumeThread(CurrentThread);                                                                                           | 
        KeLeaveCriticalRegionThread(CurrentThread);                                                                                   | 
        PspExitThread(a2);                                                                                                            | 
        JUMPOUT(0x140ACF415LL);                                                                                                       | 
      }                                                                                                                               | 
    }                                                                                                                                 | 
    else                                                                                                                              | 
    {                                                                                                                                 | 
      PspLogAuditTerminateRemoteProcessEvent(v10, v3, ProcessStartKey, TimeQuadPart);                                                 | 
    }                                                                                                                                 | 
  }                                                                                                                                   | 
  else                                                                                                                                | 
  {                                                                                                                                   | 
    if ( PreviousMode != 1 && (Process->SecureState.SecureHandle & 1) == 0                                                            | 
      || (Process[3].ActiveGroupsMask.Masks[1] & 0x100000000LL) != 0 )                                                                | 
    {                                                                                                                                 | 
      return 3221225659LL;                                                                                                            | 
    }                                                                                                                                 | 
    PspLockProcessExclusive(CurrentThread->ApcState.Process, CurrentThread);                                                          | 
    _m_prefetchw((char *)&Process[1].DirectoryTableBase + 4);                                                                         | 
    DirectoryTableBase_high = HIDWORD(Process[1].DirectoryTableBase);                                                                 | 
    do                                                                                                                                | 
    {                                                                                                                                 | 
      v12 = DirectoryTableBase_high;                                                                                                  | 
      DirectoryTableBase_high = _InterlockedCompareExchange(                                                                          | 
                                  (volatile signed __int32 *)&Process[1].DirectoryTableBase + 1,                                      | 
                                  DirectoryTableBase_high | 0x40000000,                                                               | 
                                  DirectoryTableBase_high);                                                                           | 
    }                                                                                                                                 | 
    while ( v12 != DirectoryTableBase_high );                                                                                         | 
    if ( (DirectoryTableBase_high & 0x40000008) != 0 )                                                                                | 
    {                                                                                                                                 | 
      PspUnlockProcessExclusive(Process, CurrentThread);                                                                              | 
      LOBYTE(v13) = 1;                                                                                                                | 
      PspTerminateThreadByPointer(CurrentThread, a2, v13);                                                                            | 
      return 0LL;                                                                                                                     | 
    }                                                                                                                                 | 
    *((_DWORD *)&CurrentThread[1].SwapListEntry + 3) |= 0x40u;                                                                        | 
    if ( HIDWORD(Process[2].AvailableCpuState) == 259 )                                                                               | 
    {                                                                                                                                 | 
      HIDWORD(Process[2].AvailableCpuState) = a2;                                                                                     | 
    }                                                                                                                                 | 
    if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&Process[1].Header.Lock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )            | 
    {                                                                                                                                 | 
      ExfTryToWakePushLock(&Process[1]);                                                                                              | 
    }                                                                                                                                 | 
    KeAbPostRelease((ULONG_PTR)&Process[1]);                                                                                          | 
    v3 = PspTerminateAllThreads(Process);                                                                                             | 
  }                                                                                                                                   | 
  KiLeaveCriticalRegionUnsafe(CurrentThread);                                                                                         | 
  return v3;                                                                                                                          | 
}                                                                                                                                     | 
