NTSTATUS __stdcall NtLockFile(
        HANDLE FileHandle,
        HANDLE Event,
        PIO_APC_ROUTINE ApcRoutine,
        PVOID ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock,
        PLARGE_INTEGER ByteOffset,
        PLARGE_INTEGER Length,
        ULONG Key,
        BOOLEAN FailImmediately,
        BOOLEAN ExclusiveLock)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v11 = 0;
  Eventa = 0LL;
  QuadPart = 0LL;
  v53 = 0LL;
  v55 = 0LL;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = CurrentThread->PreviousMode;
  v48 = PreviousMode;
  v49 = 0LL;
  v13 = ObReferenceObjectByHandle(FileHandle, 0, (POBJECT_TYPE)IoFileObjectType, PreviousMode, &v49, &v55);
  v14 = v49;
  if ( v13 >= 0 && (unsigned __int8)IopFileObjectRevoked(v49) )
  {
    ObfDereferenceObject(v14);
    v13 = -1073739504;
  }
  if ( v13 < 0 )
  {
    return v13;
  }
  if ( PreviousMode )
  {
    if ( (v55.GrantedAccess & 3) == 0 )
    {
      ObfDereferenceObject(v14);
      return -1073741790;
    }
    v15 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)IoStatusBlock < 0x7FFFFFFF0000LL )
    {
      v15 = (__int64)IoStatusBlock;
    }
    *(_DWORD *)v15 = *(_DWORD *)v15;
    v16 = *((_BYTE *)v49 + 80) & 2;
    v59 = KeGetCurrentThread();
    Process = v59->ApcState.Process;
    if ( Process[1].ReadyTime )
    {
      v19 = WORD2(Process[3].PerProcessorCycleTimes);
      if ( (v19 == 332 || v19 == 452) && !v16 )
      {
        Status = (struct _IO_STATUS_BLOCK *)(unsigned int)IoStatusBlock->Status;
        Status->Status = Status->Status;
        IoStatusBlock = Status;
        ApcRoutine = (PIO_APC_ROUTINE)((unsigned __int64)ApcRoutine | 1);
      }
    }
    if ( ((unsigned __int8)ByteOffset & 3) != 0 || (QuadPart = ByteOffset->QuadPart, ((unsigned __int8)Length & 3) != 0) )
    {
      ExRaiseDatatypeMisalignment();
    }
    v53 = Length->QuadPart;
    v14 = v49;
    if ( *((_QWORD *)v49 + 22) && ((unsigned __int64)ApcRoutine & 0xFFFFFFFFFFFFFFFEuLL) != 0 )
    {
      ObfDereferenceObject(v49);
      return -1073741811;
    }
  }
  else
  {
    QuadPart = ByteOffset->QuadPart;
    v53 = Length->QuadPart;
  }
  v14[72] = 1;
  v21 = (struct _FILE_OBJECT *)v49;
  result = IopSetLockOperationProcess(v49, KeGetCurrentThread()->ApcState.Process, 0LL);
  if ( result >= 0 )
  {
    if ( Event )
    {
      v56 = 0LL;
      v39 = ObReferenceObjectByHandle(Event, 2u, (POBJECT_TYPE)ExEventObjectType, PreviousMode, &v56, 0LL);
      Eventa = (PRKEVENT)v56;
      if ( v39 >= 0 )
      {
        KeResetEvent((PRKEVENT)v56);
      }
    }
    RelatedDeviceObject = IoGetRelatedDeviceObject(v21);
    DriverObject = RelatedDeviceObject->DriverObject;
    FastIoDispatch = DriverObject->FastIoDispatch;
    if ( FastIoDispatch && (FastIoLock = (ULONG_PTR)FastIoDispatch->FastIoLock) != 0 )
    {
      v51 = 0LL;
      if ( (MmVerifierData & 0x10) != 0 && MmIsDriverVerifying(DriverObject) )
      {
        v26 = (void *)VfFastIoSnapState(DriverObject);
      }
      else
      {
        v26 = 0LL;
      }
      v27 = ExclusiveLock;
      LOBYTE(HandleInformation) = FailImmediately;
      v28 = Key;
      v29 = guard_dispatch_icall_no_overrides(
              v21,
              &QuadPart,
              &v53,
              CurrentThread->ApcState.Process,
              Key,
              HandleInformation,
              ExclusiveLock,
              &v51,
              RelatedDeviceObject);
      if ( v26 )
      {
        VfFastIoCheckState(v26, FastIoLock);
      }
      if ( v29 )
      {
        if ( ((unsigned __int8)ApcRoutine & 1) != 0 )
        {
          HIDWORD(IoStatusBlock->Pointer) = DWORD2(v51);
          IoStatusBlock->Status = v51;
        }
        else
        {
          *(_OWORD *)&IoStatusBlock->Status = v51;
        }
        v30 = Eventa;
        if ( Eventa )
        {
          if ( (v21->Flags & 0x8000000) == 0 )
          {
            KeSetEvent(Eventa, 0, 0);
          }
          ObfDereferenceObject(v30);
        }
        if ( v21->CompletionContext )
        {
          if ( ApcContext )
          {
            if ( (v21->Flags & 0x2000000) == 0 )
            {
              v57 = 0LL;
              Irp = 0LL;
              v47 = 0;
              IopIncrementCompletionContextUsageCountAndReadData((ULONG_PTR)v21);
              if ( v57 )
              {
                v43 = IoSetIoCompletionEx(v57, (_DWORD)Irp, (_DWORD)ApcContext, v51, *((__int64 *)&v51 + 1), 1, 0LL);
                v44 = v51;
                if ( v43 < 0 )
                {
                  v44 = -1073741670;
                }
                LODWORD(v51) = v44;
              }
            }
          }
        }
        ObfDereferenceObject(v21);
        return v51;
      }
      v11 = 0;
    }
    else
    {
      v27 = ExclusiveLock;
      v28 = Key;
    }
    p_Flags = &v21->Flags;
    if ( (v21->Flags & 2) == 0 )
    {
      v32 = 0;
      goto LABEL_44;
    }
    v35 = (v21->Flags & 4) != 0;
    v36 = KeGetCurrentThread();
    --v36->KernelApcDisable;
    v37 = (volatile __int32 *)v49;
    v38 = KeAbPreAcquire((char *)v49 + 128, 0LL, 0LL);
    v47 = 0;
    if ( _InterlockedExchange(v37 + 29, 1) )
    {
      v21 = (struct _FILE_OBJECT *)v49;
      v11 = IopWaitAndAcquireFileObjectLock((_DWORD)v49, v48, v35, v38, (__int64)&v47);
    }
    else
    {
      if ( v38 )
      {
        *(_BYTE *)(v38 + 10) = 1;
      }
      v21 = (struct _FILE_OBJECT *)v49;
      PsReferenceSiloContext(v49);
    }
    if ( !v47 )
    {
      v32 = 1;
LABEL_44:
      IopResetEvent(v21);
      v33 = (IRP *)IopAllocateIrpExReturn(
                     RelatedDeviceObject,
                     (unsigned __int8)RelatedDeviceObject->StackSize,
                     (unsigned __int8)v32 ^ 1u);
      v34 = v33;
      Irp = v33;
      if ( v33 )
      {
        v33->Tail.Overlay.OriginalFileObject = v21;
        v33->Tail.Overlay.Thread = CurrentThread;
        v33->RequestorMode = v48;
        v33->UserEvent = Eventa;
        v33->UserIosb = IoStatusBlock;
        v33->Overlay.AllocationSize.QuadPart = (LONGLONG)ApcRoutine;
        v33->Overlay.AsynchronousParameters.UserApcContext = ApcContext;
        CurrentStackLocation = v33->Tail.Overlay.CurrentStackLocation;
        *(_WORD *)&CurrentStackLocation[-1].MajorFunction = 273;
        CurrentStackLocation[-1].FileObject = v21;
        CurrentStackLocation[-1].Flags = 0;
        v41 = 0;
        if ( FailImmediately )
        {
          CurrentStackLocation[-1].Flags = 1;
          v41 = 1;
        }
        if ( v27 )
        {
          CurrentStackLocation[-1].Flags = v41 | 2;
        }
        CurrentStackLocation[-1].Parameters.Create.Options = v28;
        CurrentStackLocation[-1].Parameters.Read.ByteOffset.QuadPart = QuadPart;
        Pool2 = (LONGLONG *)ExAllocatePool2(0x61uLL, 8uLL, 0x654C6F49u);
        *Pool2 = v53;
        v34->Tail.Overlay.AuxiliaryBuffer = (PCHAR)Pool2;
        CurrentStackLocation[-1].Parameters.WMI.ProviderId = (ULONG_PTR)Pool2;
        LOBYTE(HandleInformation) = v32;
        LOBYTE(Object) = v48;
        return IopSynchronousServiceTail(RelatedDeviceObject, v34, v21, 0LL, Object, HandleInformation, 2);
      }
      else
      {
        if ( Eventa )
        {
          ObfDereferenceObject(Eventa);
        }
        if ( (*p_Flags & 2) != 0 )
        {
          IopReleaseFileObjectLock((ULONG_PTR)v21);
        }
        ObfDereferenceObject(v21);
        return -1073741670;
      }
    }
    if ( Eventa )
    {
      ObfDereferenceObject(Eventa);
    }
    ObfDereferenceObject(v21);
    return v11;
  }
  return result;
}
270 row(s)
