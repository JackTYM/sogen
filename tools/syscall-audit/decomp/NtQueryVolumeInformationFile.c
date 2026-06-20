NTSTATUS __stdcall NtQueryVolumeInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FsInformation,
        ULONG Length,
        FS_INFORMATION_CLASS FsInformationClass)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  P = 0LL;
  CurrentThread = KeGetCurrentThread();
  v57 = CurrentThread;
  PreviousMode = CurrentThread->PreviousMode;
  v49 = PreviousMode;
  if ( PreviousMode )
  {
    if ( (unsigned int)FsInformationClass >= 0x10 )
    {
      return -1073741821;
    }
    v9 = FsInformationClass;
    v10 = *((unsigned __int8 *)IopQueryFsOperationLength + (int)FsInformationClass);
    if ( !(_BYTE)v10 )
    {
      return -1073741821;
    }
    if ( Length < v10 )
    {
      return -1073741820;
    }
    v11 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)IoStatusBlock < 0x7FFFFFFF0000LL )
    {
      v11 = (__int64)IoStatusBlock;
    }
    *(_DWORD *)v11 = *(_DWORD *)v11;
    Process = CurrentThread->ApcState.Process;
    v14 = 0;
    if ( Process[1].ReadyTime )
    {
      v13 = WORD2(Process[3].PerProcessorCycleTimes);
      if ( v13 == 332 || v13 == 452 )
      {
        v14 = 1;
      }
    }
    v50 = v14;
    if ( v14 )
    {
      ProbeForWrite(FsInformation, Length, 4u);
      PreviousMode = v49;
    }
    else if ( Length )
    {
      v15 = (unsigned __int64)FsInformation;
      if ( ((*((unsigned __int8 *)IopQuerySetFsAlignmentRequirement + (int)FsInformationClass) - 1) & (unsigned int)FsInformation) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v16 = (unsigned __int64)FsInformation + Length - 1;
      if ( (unsigned __int64)FsInformation > v16 || v16 >= 0x7FFFFFFF0000LL )
      {
        ExRaiseAccessViolation();
      }
      v17 = (v16 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      do
      {
        *(_BYTE *)v15 = *(_BYTE *)v15;
        v15 = (v15 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      }
      while ( v15 != v17 );
    }
  }
  else
  {
    v9 = FsInformationClass;
  }
  v18 = IopQueryFsOperationAccess[v9];
  v51 = 0LL;
  v19 = ObReferenceObjectByHandle(FileHandle, v18, (POBJECT_TYPE)IoFileObjectType, PreviousMode, &v51, 0LL);
  v20 = (struct _FILE_OBJECT *)v51;
  BugCheckParameter2 = (ULONG_PTR)v51;
  if ( v19 < 0 )
  {
    return v19;
  }
  if ( (unsigned __int8)IopFileObjectRevoked(v51) )
  {
    ObfDereferenceObject(v20);
    v19 = -1073739504;
  }
  if ( v19 < 0 )
  {
    return v19;
  }
  p_Flags = &v20->Flags;
  v56 = &v20->Flags;
  Flags = v20->Flags;
  if ( (Flags & 0x800) != 0 )
  {
    if ( FsInformationClass != FileFsDeviceInformation )
    {
      ObfDereferenceObject(v20);
      return -1073741808;
    }
  }
  else if ( FsInformationClass != FileFsDeviceInformation )
  {
    goto LABEL_32;
  }
  if ( (Flags & 0x800) == 0 && v20->DeviceObject->DeviceType == 20 )
  {
LABEL_32:
    if ( (Flags & 2) != 0 )
    {
      v24 = (Flags & 4) != 0;
      v25 = KeGetCurrentThread();
      --v25->KernelApcDisable;
      v26 = (volatile __int32 *)v51;
      v27 = KeAbPreAcquire((char *)v51 + 128, 0LL, 0LL);
      v48 = 0;
      if ( _InterlockedExchange(v26 + 29, 1) )
      {
        v20 = (struct _FILE_OBJECT *)v51;
        v19 = IopWaitAndAcquireFileObjectLock((_DWORD)v51, v49, v24, v27, (__int64)&v48);
      }
      else
      {
        if ( v27 )
        {
          *(_BYTE *)(v27 + 10) = 1;
        }
        v20 = (struct _FILE_OBJECT *)v51;
        PsReferenceSiloContext(v51);
        v19 = 0;
      }
      if ( v48 )
      {
LABEL_70:
        ObfDereferenceObject(v20);
        return v19;
      }
      v48 = 1;
    }
    else
    {
      v48 = 0;
    }
    if ( FsInformationClass != FileFsDriverPathInformation )
    {
      IopResetEvent(v20);
      RelatedDeviceObject = IoGetRelatedDeviceObject(v20);
      if ( (*p_Flags & 2) == 0 )
      {
        Pool2 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);
        P = Pool2;
        if ( !Pool2 )
        {
          goto LABEL_63;
        }
        KeInitializeEvent(Pool2, SynchronizationEvent, 0);
      }
      Irp = IopAllocateIrpExReturn(RelatedDeviceObject, (unsigned __int8)RelatedDeviceObject->StackSize, 0LL);
      v30 = (IRP *)Irp;
      if ( Irp )
      {
        *(_QWORD *)(Irp + 192) = v20;
        *(_QWORD *)(Irp + 152) = v57;
        *(_BYTE *)(Irp + 64) = v49;
        v58 = 0LL;
        if ( v48 )
        {
          v31 = IoStatusBlock;
          v32 = 0LL;
        }
        else
        {
          *(_DWORD *)(Irp + 16) = 4;
          v31 = (PIO_STATUS_BLOCK)&v58;
          v32 = (struct _KEVENT *)P;
        }
        v30->UserEvent = v32;
        v30->UserIosb = v31;
        v30->Overlay.AllocationSize.QuadPart = 0LL;
        CurrentStackLocation = v30->Tail.Overlay.CurrentStackLocation;
        CurrentStackLocation[-1].MajorFunction = 10;
        CurrentStackLocation[-1].FileObject = (PFILE_OBJECT)BugCheckParameter2;
        v30->UserBuffer = FsInformation;
        v30->AssociatedIrp.MasterIrp = 0LL;
        v30->MdlAddress = 0LL;
        v34 = ExAllocatePool2(0x41uLL, Length, 0x42536F49u);
        v30->AssociatedIrp.MasterIrp = (struct _IRP *)v34;
        if ( v34 )
        {
          v30->Flags |= 0x870u;
          CurrentStackLocation[-1].Parameters.Read.Length = Length;
          CurrentStackLocation[-1].Parameters.Create.Options = FsInformationClass;
          v36 = v48;
          LOBYTE(HandleInformation) = v48;
          v37 = v49;
          LOBYTE(Object) = v49;
          LOBYTE(v35) = 1;
          result = IopSynchronousServiceTail(
                     RelatedDeviceObject,
                     v30,
                     BugCheckParameter2,
                     v35,
                     Object,
                     HandleInformation,
                     2);
          if ( !v36 )
          {
            return IopSynchronousApiServiceTail((unsigned int)result, P, v30, v37, &v58, IoStatusBlock);
          }
        }
        else
        {
          IopExceptionCleanupEx(BugCheckParameter2, v30, 0LL, P, (*(_DWORD *)(BugCheckParameter2 + 80) & 2) != 0);
          return -1073741670;
        }
        return result;
      }
      if ( (*p_Flags & 2) == 0 )
      {
        ExFreePoolWithTag(P, 0);
      }
      if ( (*p_Flags & 2) != 0 )
      {
        IopReleaseFileObjectLock((ULONG_PTR)v20);
      }
LABEL_63:
      ObfDereferenceObject(v20);
      return -1073741670;
    }
    v55 = 0LL;
    v44 = (_BYTE *)ExAllocatePool2(0x61uLL, Length, 0x42536F49u);
    v55 = v44;
    memmove(v44, FsInformation, Length);
    DriverPathInformation = IopGetDriverPathInformation(v20, v44, Length);
    v19 = DriverPathInformation;
    v52 = DriverPathInformation;
    if ( DriverPathInformation < 0 )
    {
      RtlRaiseStatus((unsigned int)DriverPathInformation);
    }
    *(_BYTE *)FsInformation = *v44;
    IoStatusBlock->Status = 0;
    IoStatusBlock->Information = 12LL;
    if ( v44 )
    {
      ExFreePoolWithTag(v44, 0);
    }
    if ( (*p_Flags & 2) != 0 )
    {
      IopReleaseFileObjectLock((ULONG_PTR)v20);
    }
    goto LABEL_70;
  }
  MountFlag = 0;
  DeviceObject = v20->DeviceObject;
  Vpb = DeviceObject->Vpb;
  if ( Vpb )
  {
    v41 = v20->Vpb;
    if ( v41 == Vpb )
    {
      MountFlag = v41->Flags & 1;
      v20 = (struct _FILE_OBJECT *)v51;
    }
    else
    {
      MountFlag = (unsigned __int8)IopGetMountFlag(v20->DeviceObject);
    }
  }
  if ( Length < 8 )
  {
    ObfDereferenceObject(v20);
    return -1073741820;
  }
  else
  {
    *(_DWORD *)FsInformation = DeviceObject->DeviceType;
    Characteristics = DeviceObject->Characteristics;
    *((_DWORD *)FsInformation + 1) = Characteristics;
    if ( MountFlag )
    {
      *((_DWORD *)FsInformation + 1) = Characteristics | 0x20;
    }
    IoStatusBlock->Status = 0;
    IoStatusBlock->Information = 8LL;
    v52 = 0;
    ObfDereferenceObject(v20);
    return 0;
  }
}
295 row(s)
