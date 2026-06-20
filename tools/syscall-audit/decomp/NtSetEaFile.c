__int64 __fastcall NtSetEaFile(int a1, unsigned __int64 a2, void *a3, ULONG a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v4 = a4;
  v5 = a3;
  FileObject = 0LL;
  v8 = 0LL;
  P = 0LL;
  v47 = 0LL;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = CurrentThread->PreviousMode;
  v37 = PreviousMode;
  if ( PreviousMode )
  {
    v10 = 0x7FFFFFFF0000LL;
    if ( a2 < 0x7FFFFFFF0000LL )
    {
      v10 = a2;
    }
    *(_DWORD *)v10 = *(_DWORD *)v10;
    if ( a4 && ((unsigned __int8)a3 & 3) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
  }
  LOBYTE(a3) = PreviousMode;
  result = IopReferenceFileObject(a1, 16, (int)a3, (int)&FileObject, 0LL);
  v39 = result;
  if ( (int)result >= 0 )
  {
    v12 = FileObject;
    p_Flags = &FileObject->Flags;
    if ( (FileObject->Flags & 2) != 0 )
    {
      v13 = (FileObject->Flags & 4) != 0;
      v14 = KeGetCurrentThread();
      --v14->KernelApcDisable;
      v15 = FileObject;
      v16 = KeAbPreAcquire(&FileObject->Lock, 0LL, 0LL);
      v36 = 0;
      if ( _InterlockedExchange((volatile __int32 *)&v15->Busy, 1) )
      {
        LOBYTE(v18) = v13;
        LOBYTE(v17) = PreviousMode;
        v12 = FileObject;
        v19 = IopWaitAndAcquireFileObjectLock((_DWORD)FileObject, v17, v18, v16, (__int64)&v36);
      }
      else
      {
        if ( v16 )
        {
          *(_BYTE *)(v16 + 10) = 1;
        }
        v12 = FileObject;
        PsReferenceSiloContext(FileObject);
        v19 = 0;
      }
      v39 = v19;
      if ( !v36 )
      {
        v36 = 1;
        v4 = a4;
        goto LABEL_15;
      }
    }
    else
    {
      Pool2 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);
      v8 = Pool2;
      P = Pool2;
      if ( Pool2 )
      {
        KeInitializeEvent(Pool2, SynchronizationEvent, 0);
        v36 = 0;
LABEL_15:
        IopResetEvent(v12);
        RelatedDeviceObject = IoGetRelatedDeviceObject(v12);
        LOBYTE(v20) = RelatedDeviceObject->StackSize;
        v21 = (IRP *)IopAllocateIrpExReturn(RelatedDeviceObject, v20, 0LL);
        v22 = v21;
        v45 = v21;
        if ( v21 )
        {
          v21->Tail.Overlay.OriginalFileObject = v12;
          v21->Tail.Overlay.Thread = CurrentThread;
          v23 = v37;
          v21->RequestorMode = v37;
          if ( v36 )
          {
            v24 = (struct _IO_STATUS_BLOCK *)a2;
            v25 = 0LL;
          }
          else
          {
            v21->Flags = 4;
            v24 = (struct _IO_STATUS_BLOCK *)&v47;
            v25 = v8;
          }
          v22->UserEvent = v25;
          v22->UserIosb = v24;
          v22->Overlay.AllocationSize.QuadPart = 0LL;
          v26 = (__int64)&v22->Tail.Overlay.CurrentStackLocation[-1];
          p_Flags = (ULONG *)v26;
          *(_BYTE *)v26 = 8;
          *(_QWORD *)(v26 + 48) = v12;
          Flags = RelatedDeviceObject->Flags;
          if ( (Flags & 4) != 0 )
          {
            ErrorOffset = 0;
            if ( (_DWORD)v4 )
            {
              v38 = 0;
              v31 = (struct _FILE_FULL_EA_INFORMATION *)ExAllocatePool2(0x61uLL, v4, 0x42536F49u);
              v22->AssociatedIrp.MasterIrp = (struct _IRP *)v31;
              memmove(v31, v5, v4);
              LODWORD(v4) = a4;
              v32 = IoCheckEaBufferValidity(v31, a4, &ErrorOffset);
              v39 = v32;
              if ( v32 < 0 )
              {
                v38 = 1;
                *(_DWORD *)a2 = v32;
                *(_QWORD *)(a2 + 8) = ErrorOffset;
                RtlRaiseStatus((unsigned int)v32);
              }
              v22->Flags |= 0x30u;
              v23 = v37;
              v26 = (__int64)p_Flags;
            }
            else
            {
              v22->AssociatedIrp.MasterIrp = 0LL;
            }
          }
          else if ( (Flags & 0x10) != 0 )
          {
            if ( (_DWORD)v4 )
            {
              Mdl = IoAllocateMdl(v5, v4, 0, 1u, v22);
              if ( !Mdl )
              {
                RtlRaiseStatus(3221225626LL);
              }
              LOBYTE(v34) = v23;
              IopProbeAndLockPages_0((_DWORD)Mdl, v34, 0, (_DWORD)RelatedDeviceObject, *(unsigned __int8 *)p_Flags);
              v26 = (__int64)p_Flags;
            }
          }
          else
          {
            v22->UserBuffer = v5;
          }
          *(_DWORD *)(v26 + 8) = v4;
          v28 = v36;
          LOBYTE(Irp) = v23;
          result = IopSynchronousServiceTail(RelatedDeviceObject, v22, v12, 0LL, Irp, v36, 2);
          if ( !v28 )
          {
            LOBYTE(v29) = v23;
            return IopSynchronousApiServiceTail((unsigned int)result, v8, v22, v29, &v47, a2);
          }
        }
        else
        {
          if ( (*p_Flags & 2) == 0 )
          {
            ExFreePoolWithTag(v8, 0);
          }
          IopAllocateIrpCleanup((ULONG_PTR)v12, 0LL);
          return 3221225626LL;
        }
        return result;
      }
      v19 = -1073741670;
    }
    ObfDereferenceObject(v12);
    return v19;
  }
  return result;
}
181 row(s)
