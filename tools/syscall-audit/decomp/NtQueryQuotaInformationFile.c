NTSTATUS __stdcall NtQueryQuotaInformationFile(                                                       | 
        HANDLE FileHandle,                                                                            | 
        PIO_STATUS_BLOCK IoStatusBlock,                                                               | 
        PVOID Buffer,                                                                                 | 
        ULONG Length,                                                                                 | 
        BOOLEAN ReturnSingleEntry,                                                                    | 
        PVOID SidList,                                                                                | 
        ULONG SidListLength,                                                                          | 
        PSID StartSid,                                                                                | 
        BOOLEAN RestartScan)                                                                          | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  VirtualAddress = Buffer;                                                                            | 
  v72 = (int)FileHandle;                                                                              | 
  v9 = Buffer;                                                                                        | 
  v11 = (int)FileHandle;                                                                              | 
  Object = 0LL;                                                                                       | 
  v12 = 0LL;                                                                                          | 
  v67 = 0LL;                                                                                          | 
  P = 0LL;                                                                                            | 
  LODWORD(RelatedDeviceObject) = 0;                                                                   | 
  Sid = 0LL;                                                                                          | 
  v65 = 0LL;                                                                                          | 
  v71[0] = 0LL;                                                                                       | 
  CurrentThread = KeGetCurrentThread();                                                               | 
  v69 = CurrentThread;                                                                                | 
  PreviousMode = CurrentThread->PreviousMode;                                                         | 
  AccessMode = PreviousMode;                                                                          | 
  if ( !PreviousMode )                                                                                | 
  {                                                                                                   | 
    if ( SidList )                                                                                    | 
    {                                                                                                 | 
      v29 = v65;                                                                                      | 
      if ( SidListLength )                                                                            | 
      {                                                                                               | 
        v29 = SidList;                                                                                | 
      }                                                                                               | 
      v65 = v29;                                                                                      | 
    }                                                                                                 | 
    v30 = Sid;                                                                                        | 
    v21 = StartSid;                                                                                   | 
    if ( StartSid )                                                                                   | 
    {                                                                                                 | 
      v30 = StartSid;                                                                                 | 
    }                                                                                                 | 
    Sid = v30;                                                                                        | 
    Pool2 = (char *)P;                                                                                | 
    goto LABEL_46;                                                                                    | 
  }                                                                                                   | 
  v15 = 0x7FFFFFFF0000LL;                                                                             | 
  if ( (unsigned __int64)IoStatusBlock < 0x7FFFFFFF0000LL )                                           | 
  {                                                                                                   | 
    v15 = (__int64)IoStatusBlock;                                                                     | 
  }                                                                                                   | 
  *(_DWORD *)v15 = *(_DWORD *)v15;                                                                    | 
  Process = CurrentThread->ApcState.Process;                                                          | 
  if ( !Process[1].ReadyTime )                                                                        | 
  {                                                                                                   | 
    goto LABEL_9;                                                                                     | 
  }                                                                                                   | 
  v17 = WORD2(Process[3].PerProcessorCycleTimes);                                                     | 
  if ( v17 == 332 || (v18 = v17 == 452, v19 = 0, v18) )                                               | 
  {                                                                                                   | 
    v19 = 1;                                                                                          | 
  }                                                                                                   | 
  v61 = v19;                                                                                          | 
  v20 = 4;                                                                                            | 
  if ( !v19 )                                                                                         | 
  {                                                                                                   | 
LABEL_9:                                                                                              | 
    v20 = 8;                                                                                          | 
  }                                                                                                   | 
  ProbeForWrite(v9, Length, v20);                                                                     | 
  v21 = StartSid;                                                                                     | 
  if ( StartSid )                                                                                     | 
  {                                                                                                   | 
    v22 = (__int64)StartSid + 1;                                                                      | 
    if ( (unsigned __int64)StartSid + 1 >= 0x7FFFFFFF0000LL )                                         | 
    {                                                                                                 | 
      v22 = 0x7FFFFFFF0000LL;                                                                         | 
    }                                                                                                 | 
    v59 = *(_BYTE *)v22;                                                                              | 
    v23 = RtlLengthRequiredSid(v59);                                                                  | 
    v24 = v23;                                                                                        | 
    LODWORD(RelatedDeviceObject) = v23;                                                               | 
    if ( !v23 )                                                                                       | 
    {                                                                                                 | 
      goto LABEL_21;                                                                                  | 
    }                                                                                                 | 
    if ( ((unsigned __int8)StartSid & 3) != 0 )                                                       | 
    {                                                                                                 | 
      ExRaiseDatatypeMisalignment();                                                                  | 
    }                                                                                                 | 
    if ( (unsigned __int64)StartSid + v23 <= 0x7FFFFFFF0000LL && (char *)StartSid + v23 >= StartSid ) | 
    {                                                                                                 | 
      goto LABEL_21;                                                                                  | 
    }                                                                                                 | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    v59 = 0;                                                                                          | 
  }                                                                                                   | 
  v24 = (unsigned int)RelatedDeviceObject;                                                            | 
LABEL_21:                                                                                             | 
  if ( SidList && SidListLength )                                                                     | 
  {                                                                                                   | 
    if ( ((unsigned __int8)SidList & 3) != 0 )                                                        | 
    {                                                                                                 | 
      ExRaiseDatatypeMisalignment();                                                                  | 
    }                                                                                                 | 
    v25 = (char *)SidList + SidListLength;                                                            | 
    if ( (unsigned __int64)v25 > 0x7FFFFFFF0000LL || v25 < SidList )                                  | 
    {                                                                                                 | 
      v24 = (unsigned int)RelatedDeviceObject;                                                        | 
    }                                                                                                 | 
    v26 = (SidListLength + 3) & 0xFFFFFFFC;                                                           | 
    if ( v26 > ~v24 )                                                                                 | 
    {                                                                                                 | 
      return -1073741811;                                                                             | 
    }                                                                                                 | 
    if ( v26 < SidListLength )                                                                        | 
    {                                                                                                 | 
      return -1073741811;                                                                             | 
    }                                                                                                 | 
    Pool2 = (char *)ExAllocatePool2(0x61uLL, v26 + v24, 0x73536F49u);                                 | 
    P = Pool2;                                                                                        | 
    v65 = Pool2;                                                                                      | 
    memmove(Pool2, SidList, SidListLength);                                                           | 
    v12 = 0LL;                                                                                        | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    SidListLength = 0;                                                                                | 
    if ( StartSid )                                                                                   | 
    {                                                                                                 | 
      Pool2 = (char *)ExAllocatePool2(0x121uLL, v24, 0x73536F49u);                                    | 
      P = Pool2;                                                                                      | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      Pool2 = (char *)P;                                                                              | 
    }                                                                                                 | 
  }                                                                                                   | 
  if ( StartSid )                                                                                     | 
  {                                                                                                   | 
    Sid = &Pool2[(SidListLength + 3) & 0xFFFFFFFC];                                                   | 
    memmove(Sid, StartSid, v24);                                                                      | 
    *((_BYTE *)Sid + 1) = v59;                                                                        | 
  }                                                                                                   | 
  PreviousMode = AccessMode;                                                                          | 
  v11 = v72;                                                                                          | 
LABEL_46:                                                                                             | 
  if ( v65 )                                                                                          | 
  {                                                                                                   | 
    v68 = 0LL;                                                                                        | 
    v31 = IopCheckGetQuotaBufferValidity(v65, SidListLength, &v68);                                   | 
    if ( v31 < 0 )                                                                                    | 
    {                                                                                                 | 
      IoStatusBlock->Information = (ULONG_PTR)v68;                                                    | 
LABEL_49:                                                                                             | 
      if ( Pool2 )                                                                                    | 
      {                                                                                               | 
        ExFreePoolWithTag(Pool2, 0);                                                                  | 
      }                                                                                               | 
      return v31;                                                                                     | 
    }                                                                                                 | 
  }                                                                                                   | 
  if ( Sid && !RtlValidSid(Sid) )                                                                     | 
  {                                                                                                   | 
    if ( Pool2 )                                                                                      | 
    {                                                                                                 | 
      ExFreePoolWithTag(Pool2, 0);                                                                    | 
    }                                                                                                 | 
    return -1073741704;                                                                               | 
  }                                                                                                   | 
  LOBYTE(Buffer) = PreviousMode;                                                                      | 
  v31 = IopReferenceFileObject(v11, 0, (int)Buffer, (int)&Object, 0LL);                               | 
  if ( v31 < 0 )                                                                                      | 
  {                                                                                                   | 
    goto LABEL_49;                                                                                    | 
  }                                                                                                   | 
  v32 = (struct _FILE_OBJECT *)Object;                                                                | 
  v33 = (char *)Object + 80;                                                                          | 
  v70 = (char *)Object + 80;                                                                          | 
  if ( (*((_DWORD *)Object + 20) & 2) != 0 )                                                          | 
  {                                                                                                   | 
    v34 = (*((_DWORD *)Object + 20) & 4) != 0;                                                        | 
    v35 = KeGetCurrentThread();                                                                       | 
    --v35->KernelApcDisable;                                                                          | 
    v36 = (volatile __int32 *)Object;                                                                 | 
    v39 = KeAbPreAcquire((char *)Object + 128, 0LL, 0LL);                                             | 
    v59 = 0;                                                                                          | 
    if ( _InterlockedExchange(v36 + 29, 1) )                                                          | 
    {                                                                                                 | 
      LOBYTE(v38) = v34;                                                                              | 
      LOBYTE(v37) = AccessMode;                                                                       | 
      v32 = (struct _FILE_OBJECT *)Object;                                                            | 
      v40 = IopWaitAndAcquireFileObjectLock((_DWORD)Object, v37, v38, v39, (__int64)&v59);            | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      if ( v39 )                                                                                      | 
      {                                                                                               | 
        *(_BYTE *)(v39 + 10) = 1;                                                                     | 
      }                                                                                               | 
      v32 = (struct _FILE_OBJECT *)Object;                                                            | 
      PsReferenceSiloContext(Object);                                                                 | 
      v40 = 0;                                                                                        | 
    }                                                                                                 | 
    if ( v59 )                                                                                        | 
    {                                                                                                 | 
      if ( P )                                                                                        | 
      {                                                                                               | 
        ExFreePoolWithTag(P, 0);                                                                      | 
      }                                                                                               | 
LABEL_72:                                                                                             | 
      ObfDereferenceObject(v32);                                                                      | 
      return v40;                                                                                     | 
    }                                                                                                 | 
    v59 = 1;                                                                                          | 
    Pool2 = (char *)P;                                                                                | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    v41 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);                           | 
    v12 = v41;                                                                                        | 
    v67 = v41;                                                                                        | 
    if ( !v41 )                                                                                       | 
    {                                                                                                 | 
      if ( Pool2 )                                                                                    | 
      {                                                                                               | 
        ExFreePoolWithTag(Pool2, 0);                                                                  | 
      }                                                                                               | 
      v40 = -1073741670;                                                                              | 
      goto LABEL_72;                                                                                  | 
    }                                                                                                 | 
    KeInitializeEvent(v41, SynchronizationEvent, 0);                                                  | 
    v59 = 0;                                                                                          | 
  }                                                                                                   | 
  IopResetEvent(v32);                                                                                 | 
  RelatedDeviceObject = IoGetRelatedDeviceObject(v32);                                                | 
  LOBYTE(v42) = RelatedDeviceObject->StackSize;                                                       | 
  v43 = (IRP *)IopAllocateIrpExReturn(RelatedDeviceObject, v42, 0LL);                                 | 
  v44 = v43;                                                                                          | 
  v68 = v43;                                                                                          | 
  if ( !v43 )                                                                                         | 
  {                                                                                                   | 
    if ( (*v33 & 2) == 0 )                                                                            | 
    {                                                                                                 | 
      ExFreePoolWithTag(v12, 0);                                                                      | 
    }                                                                                                 | 
    IopAllocateIrpCleanup((ULONG_PTR)v32, 0LL);                                                       | 
    goto LABEL_85;                                                                                    | 
  }                                                                                                   | 
  v43->Tail.Overlay.OriginalFileObject = v32;                                                         | 
  v43->Tail.Overlay.Thread = v69;                                                                     | 
  v43->RequestorMode = AccessMode;                                                                    | 
  if ( v59 )                                                                                          | 
  {                                                                                                   | 
    v45 = IoStatusBlock;                                                                              | 
    v46 = 0LL;                                                                                        | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    v43->Flags = 4;                                                                                   | 
    v45 = (PIO_STATUS_BLOCK)v71;                                                                      | 
    v46 = (struct _KEVENT *)v67;                                                                      | 
  }                                                                                                   | 
  v44->UserEvent = v46;                                                                               | 
  v44->UserIosb = v45;                                                                                | 
  v44->Overlay.AllocationSize.QuadPart = 0LL;                                                         | 
  CurrentStackLocation = v44->Tail.Overlay.CurrentStackLocation;                                      | 
  CurrentStackLocation[-1].MajorFunction = 25;                                                        | 
  CurrentStackLocation[-1].FileObject = v32;                                                          | 
  v44->Tail.Overlay.AuxiliaryBuffer = Pool2;                                                          | 
  CurrentStackLocation[-1].Parameters.Read.ByteOffset.QuadPart = (LONGLONG)v65;                       | 
  CurrentStackLocation[-1].Parameters.Create.EaLength = SidListLength;                                | 
  Flags = RelatedDeviceObject->Flags;                                                                 | 
  if ( (Flags & 4) != 0 )                                                                             | 
  {                                                                                                   | 
    if ( Length )                                                                                     | 
    {                                                                                                 | 
      v49 = ExAllocatePool2(0x41uLL, Length, 0x42536F49u);                                            | 
      v44->AssociatedIrp.MasterIrp = (struct _IRP *)v49;                                              | 
      if ( !v49 )                                                                                     | 
      {                                                                                               | 
        IopExceptionCleanupEx((ULONG_PTR)v32, v44, 0LL, v67, (*v70 & 2) != 0);                        | 
LABEL_85:                                                                                             | 
        if ( Pool2 )                                                                                  | 
        {                                                                                             | 
          ExFreePoolWithTag(Pool2, 0);                                                                | 
        }                                                                                             | 
        return -1073741670;                                                                           | 
      }                                                                                               | 
      v44->Flags |= 0x70u;                                                                            | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      v44->AssociatedIrp.MasterIrp = 0LL;                                                             | 
    }                                                                                                 | 
  }                                                                                                   | 
  else if ( (Flags & 0x10) != 0 )                                                                     | 
  {                                                                                                   | 
    v50 = Length;                                                                                     | 
    if ( Length )                                                                                     | 
    {                                                                                                 | 
      Mdl = IoAllocateMdl(VirtualAddress, Length, 0, 1u, v44);                                        | 
      v52 = Mdl;                                                                                      | 
      if ( !Mdl )                                                                                     | 
      {                                                                                               | 
        RtlRaiseStatus(3221225626LL);                                                                 | 
      }                                                                                               | 
      MajorFunction = CurrentStackLocation[-1].MajorFunction;                                         | 
      MmProbeAndLockPages(Mdl, AccessMode, IoWriteAccess);                                            | 
      if ( (MmTrackLockedPages & 1) != 0 )                                                            | 
      {                                                                                               | 
        MmUpdateMdlTracker(v52, RelatedDeviceObject->DriverObject->MajorFunction[MajorFunction]);     | 
      }                                                                                               | 
      v50 = Length;                                                                                   | 
    }                                                                                                 | 
    goto LABEL_98;                                                                                    | 
  }                                                                                                   | 
  v44->UserBuffer = VirtualAddress;                                                                   | 
  v50 = Length;                                                                                       | 
LABEL_98:                                                                                             | 
  CurrentStackLocation[-1].Parameters.Read.Length = v50;                                              | 
  CurrentStackLocation[-1].Parameters.QueryDirectory.FileName = (PUNICODE_STRING)Sid;                 | 
  CurrentStackLocation[-1].Flags = 0;                                                                 | 
  v54 = 0;                                                                                            | 
  if ( RestartScan )                                                                                  | 
  {                                                                                                   | 
    CurrentStackLocation[-1].Flags = 1;                                                               | 
    v54 = 1;                                                                                          | 
  }                                                                                                   | 
  if ( ReturnSingleEntry )                                                                            | 
  {                                                                                                   | 
    v54 |= 2u;                                                                                        | 
    CurrentStackLocation[-1].Flags = v54;                                                             | 
  }                                                                                                   | 
  if ( v21 )                                                                                          | 
  {                                                                                                   | 
    CurrentStackLocation[-1].Flags = v54 | 4;                                                         | 
  }                                                                                                   | 
  v55 = v59;                                                                                          | 
  v56 = AccessMode;                                                                                   | 
  LOBYTE(Irp) = AccessMode;                                                                           | 
  result = IopSynchronousServiceTail(RelatedDeviceObject, v44, v32, 0LL, Irp, v59, 2);                | 
  if ( !v55 )                                                                                         | 
  {                                                                                                   | 
    LOBYTE(v57) = v56;                                                                                | 
    return IopSynchronousApiServiceTail((unsigned int)result, v67, v44, v57, v71, IoStatusBlock);     | 
  }                                                                                                   | 
  return result;                                                                                      | 
}                                                                                                     | 
355 row(s)
