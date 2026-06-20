NTSTATUS __stdcall NtUnlockFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PLARGE_INTEGER ByteOffset,
        PLARGE_INTEGER Length,
        ULONG Key)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  QuadPart = 0LL;
  v40 = 0LL;
  v39 = 0LL;
  v45 = 0LL;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = CurrentThread->PreviousMode;
  v8 = PreviousMode;
  v38 = 0LL;
  result = ObReferenceObjectByHandle(
             FileHandle,
             0,
             (POBJECT_TYPE)IoFileObjectType,
             PreviousMode,
             &v38,
             (POBJECT_HANDLE_INFORMATION)&v39);
  v10 = (struct _FILE_OBJECT *)v38;
  BugCheckParameter2 = (ULONG_PTR)v38;
  if ( result >= 0 )
  {
    if ( PreviousMode )
    {
      if ( (BYTE4(v39) & 3) == 0 )
      {
        ObfDereferenceObject(v38);
        return -1073741790;
      }
      v11 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)IoStatusBlock < 0x7FFFFFFF0000LL )
      {
        v11 = (__int64)IoStatusBlock;
      }
      *(_DWORD *)v11 = *(_DWORD *)v11;
      if ( ((unsigned __int8)ByteOffset & 3) != 0
        || (QuadPart = ByteOffset->QuadPart, ((unsigned __int8)Length & 3) != 0) )
      {
        ExRaiseDatatypeMisalignment();
      }
      v40 = Length->QuadPart;
      v10 = (struct _FILE_OBJECT *)v38;
    }
    else
    {
      QuadPart = ByteOffset->QuadPart;
      v40 = Length->QuadPart;
    }
    p_Flags = &v10->Flags;
    if ( (v10->Flags & 0x800) != 0 )
    {
      AttachedDevice = IoGetAttachedDevice(v10->DeviceObject);
    }
    else
    {
      AttachedDevice = IoGetRelatedDeviceObject(v10);
    }
    v14 = AttachedDevice;
    DriverObject = AttachedDevice->DriverObject;
    FastIoDispatch = DriverObject->FastIoDispatch;
    if ( FastIoDispatch )
    {
      FastIoUnlockSingle = (ULONG_PTR)FastIoDispatch->FastIoUnlockSingle;
      if ( FastIoUnlockSingle )
      {
        v44 = 0LL;
        if ( (MmVerifierData & 0x10) != 0 && MmIsDriverVerifying(DriverObject) )
        {
          v18 = (void *)VfFastIoSnapState(DriverObject);
        }
        else
        {
          v18 = 0LL;
        }
        v19 = guard_dispatch_icall_no_overrides(v10, &QuadPart, &v40, CurrentThread->ApcState.Process, Key, &v44, v14);
        if ( v18 )
        {
          VfFastIoCheckState(v18, FastIoUnlockSingle);
        }
        if ( v19 )
        {
          *(_OWORD *)&IoStatusBlock->Status = v44;
          ObfDereferenceObject(v10);
          return v44;
        }
        v8 = PreviousMode;
      }
    }
    if ( (*p_Flags & 2) != 0 )
    {
      v22 = (*p_Flags & 4) != 0;
      v23 = KeGetCurrentThread();
      --v23->KernelApcDisable;
      v24 = (volatile __int32 *)v38;
      v25 = KeAbPreAcquire((char *)v38 + 128, 0LL, 0LL);
      v36 = 0;
      if ( _InterlockedExchange(v24 + 29, 1) )
      {
        v10 = (struct _FILE_OBJECT *)v38;
        v26 = IopWaitAndAcquireFileObjectLock((_DWORD)v38, v8, v22, v25, (__int64)&v36);
      }
      else
      {
        if ( v25 )
        {
          *(_BYTE *)(v25 + 10) = 1;
        }
        v10 = (struct _FILE_OBJECT *)v38;
        PsReferenceSiloContext(v38);
        v26 = 0;
      }
      if ( v36 )
      {
        ObfDereferenceObject(v10);
        return v26;
      }
      v36 = 1;
      v21 = 0LL;
      v39 = 0LL;
    }
    else
    {
      Pool2 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);
      v21 = Pool2;
      v39 = Pool2;
      if ( !Pool2 )
      {
LABEL_25:
        ObfDereferenceObject(v10);
        return -1073741670;
      }
      KeInitializeEvent(Pool2, SynchronizationEvent, 0);
      v36 = 0;
    }
    IopResetEvent(v10);
    Irp = IopAllocateIrpExReturn(v14, (unsigned __int8)v14->StackSize, 0LL);
    v28 = (_QWORD *)Irp;
    v38 = (PVOID)Irp;
    if ( !Irp )
    {
      if ( v21 )
      {
        ExFreePoolWithTag(v21, 0);
      }
      if ( (*p_Flags & 2) != 0 )
      {
        IopReleaseFileObjectLock((ULONG_PTR)v10);
      }
      goto LABEL_25;
    }
    *(_QWORD *)(Irp + 192) = v10;
    *(_QWORD *)(Irp + 152) = CurrentThread;
    *(_BYTE *)(Irp + 64) = v8;
    v29 = v36;
    if ( v36 )
    {
      v30 = IoStatusBlock;
      v31 = 0LL;
    }
    else
    {
      *(_DWORD *)(Irp + 16) = 4;
      v30 = (PIO_STATUS_BLOCK)&v45;
      v31 = v21;
    }
    v28[10] = v31;
    v28[9] = v30;
    v28[11] = 0LL;
    v32 = v28[23];
    *(_WORD *)(v32 - 72) = 529;
    *(_QWORD *)(v32 - 24) = BugCheckParameter2;
    v33 = (LONGLONG *)ExAllocatePool2(0x61uLL, 8uLL, 0x654C6F49u);
    *v33 = v40;
    v28[20] = v33;
    *(_QWORD *)(v32 - 64) = v33;
    *(_DWORD *)(v32 - 56) = Key;
    *(_QWORD *)(v32 - 48) = QuadPart;
    LOBYTE(HandleInformation) = v29;
    LOBYTE(Object) = v8;
    result = IopSynchronousServiceTail(v14, v28, BugCheckParameter2, 0LL, Object, HandleInformation, 2);
    if ( !v29 )
    {
      return IopSynchronousApiServiceTail((unsigned int)result, v21, v28, v8, &v45, IoStatusBlock);
    }
  }
  return result;
}
193 row(s)
