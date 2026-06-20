NTSTATUS __stdcall NtSetVolumeInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FsInformation,
        ULONG Length,
        FS_INFORMATION_CLASS FsInformationClass)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Size[0] = Length;
  Src = FsInformation;
  v48 = IoStatusBlock;
  v6 = (int)FileHandle;
  *(_QWORD *)&Size[1] = 0LL;
  v7 = 0LL;
  P = 0LL;
  Object = 0LL;
  CurrentThread = KeGetCurrentThread();
  v50 = CurrentThread;
  PreviousMode = CurrentThread->PreviousMode;
  v45 = PreviousMode;
  if ( PreviousMode )
  {
    if ( (unsigned int)FsInformationClass >= 0x10 )
    {
      return -1073741821;
    }
    v10 = FsInformationClass;
    v11 = *((unsigned __int8 *)IopSetFsOperationLength + (int)FsInformationClass);
    if ( !(_BYTE)v11 )
    {
      return -1073741821;
    }
    if ( Length < v11 )
    {
      return -1073741820;
    }
    v13 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)IoStatusBlock < 0x7FFFFFFF0000LL )
    {
      v13 = (__int64)IoStatusBlock;
    }
    *(_DWORD *)v13 = *(_DWORD *)v13;
    Process = CurrentThread->ApcState.Process;
    if ( Process[1].ReadyTime
      && ((v15 = WORD2(Process[3].PerProcessorCycleTimes), v15 == 332) || v15 == 452 ? (v16 = 1) : (v16 = 0),
          (v44 = v16) != 0) )
    {
      if ( Length && ((unsigned __int8)FsInformation & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
    }
    else if ( Length
           && ((*((unsigned __int8 *)IopQuerySetFsAlignmentRequirement + (int)FsInformationClass) - 1LL) & (unsigned __int64)FsInformation) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
  }
  else
  {
    v10 = FsInformationClass;
  }
  LOBYTE(FsInformation) = PreviousMode;
  result = IopReferenceFileObject(v6, IopSetFsOperationAccess[v10], (int)FsInformation, (int)&Size[1], 0LL);
  if ( result >= 0 )
  {
    v17 = *(struct _FILE_OBJECT **)&Size[1];
    RelatedTargetDevice = IoGetRelatedTargetDevice(*(_QWORD *)&Size[1], &Object);
    v19 = (struct _DEVICE_OBJECT *)Object;
    if ( RelatedTargetDevice < 0 )
    {
      v19 = 0LL;
    }
    Object = v19;
    p_Flags = &v17->Flags;
    v52 = &v17->Flags;
    if ( (v17->Flags & 2) != 0 )
    {
      v21 = (v17->Flags & 4) != 0;
      v22 = KeGetCurrentThread();
      --v22->KernelApcDisable;
      v23 = *(_QWORD *)&Size[1];
      v24 = KeAbPreAcquire(*(_QWORD *)&Size[1] + 128LL, 0LL, 0LL);
      v44 = 0;
      if ( _InterlockedExchange((volatile __int32 *)(v23 + 116), 1) )
      {
        LOBYTE(v26) = v21;
        LOBYTE(v25) = v45;
        v17 = *(struct _FILE_OBJECT **)&Size[1];
        v27 = IopWaitAndAcquireFileObjectLock(Size[1], v25, v26, v24, (__int64)&v44);
      }
      else
      {
        if ( v24 )
        {
          *(_BYTE *)(v24 + 10) = 1;
        }
        v17 = *(struct _FILE_OBJECT **)&Size[1];
        PsReferenceSiloContext(*(_QWORD *)&Size[1]);
        v27 = 0;
      }
      if ( v44 )
      {
        ObfDereferenceObject(v17);
        v28 = Object;
        if ( !Object )
        {
          return v27;
        }
LABEL_34:
        ObfDereferenceObject(v28);
        return v27;
      }
      v29 = 1;
      v19 = (struct _DEVICE_OBJECT *)Object;
    }
    else
    {
      Pool2 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);
      v7 = Pool2;
      P = Pool2;
      if ( !Pool2 )
      {
        ObfDereferenceObject(v17);
LABEL_44:
        if ( v19 )
        {
          ObfDereferenceObject(v19);
        }
        return -1073741670;
      }
      KeInitializeEvent(Pool2, SynchronizationEvent, 0);
      v29 = 0;
    }
    v44 = v29;
    IopResetEvent(v17);
    RelatedDeviceObject = IoGetRelatedDeviceObject(v17);
    LOBYTE(v31) = RelatedDeviceObject->StackSize;
    v32 = (IRP *)IopAllocateIrpExReturn(RelatedDeviceObject, v31, 0LL);
    v33 = v32;
    Irp = v32;
    if ( !v32 )
    {
      if ( (*p_Flags & 2) == 0 )
      {
        ExFreePoolWithTag(v7, 0);
      }
      IopAllocateIrpCleanup((ULONG_PTR)v17, 0LL);
      goto LABEL_44;
    }
    v32->Tail.Overlay.OriginalFileObject = v17;
    v32->Tail.Overlay.Thread = v50;
    v32->RequestorMode = v45;
    v55 = 0LL;
    if ( v29 )
    {
      v34 = v48;
      v35 = 0LL;
    }
    else
    {
      v32->Flags = 4;
      v34 = (struct _IO_STATUS_BLOCK *)&v55;
      v35 = v7;
    }
    v33->UserEvent = v35;
    v33->UserIosb = v34;
    v33->Overlay.AllocationSize.QuadPart = 0LL;
    CurrentStackLocation = v33->Tail.Overlay.CurrentStackLocation;
    CurrentStackLocation[-1].MajorFunction = 11;
    CurrentStackLocation[-1].FileObject = v17;
    v33->AssociatedIrp.MasterIrp = 0LL;
    v33->MdlAddress = 0LL;
    v37 = Size[0];
    v38 = (struct _IRP *)ExAllocatePool2(0x61uLL, Size[0], 0x42536F49u);
    v33->AssociatedIrp.MasterIrp = v38;
    memmove(v38, Src, v37);
    v39 = v45;
    if ( !v45
      || FsInformationClass != FileFsLabelInformation
      || (v40 = *(_DWORD *)v33->AssociatedIrp.MasterIrp, v40 >= 0) && (unsigned int)(v40 + 4) <= Size[0] )
    {
      v33->Flags |= 0x30u;
      CurrentStackLocation[-1].Parameters.Read.Length = Size[0];
      CurrentStackLocation[-1].Parameters.Create.Options = FsInformationClass;
      v41 = v44;
      LOBYTE(v43) = v39;
      v27 = IopSynchronousServiceTail(RelatedDeviceObject, v33, v17, 0LL, v43, v44, 2);
      if ( !v41 )
      {
        LOBYTE(v42) = v45;
        v27 = IopSynchronousApiServiceTail((unsigned int)v27, v7, v33, v42, &v55, v48);
      }
      if ( !v19 )
      {
        return v27;
      }
      if ( v27 >= 0 )
      {
        v58 = 0;
        v61 = 0;
        NotificationStructure = 2359297;
        v59 = 0LL;
        v60 = -1;
        v57 = GUID_IO_VOLUME_CHANGE;
        IoReportTargetDeviceChange(v19, &NotificationStructure);
      }
      v28 = v19;
      goto LABEL_34;
    }
    IopExceptionCleanupEx((ULONG_PTR)v17, v33, 0LL, v7, (*v52 & 2) != 0);
    if ( v19 )
    {
      ObfDereferenceObject(v19);
    }
    return -1073741811;
  }
  return result;
}
220 row(s)
