__int64 __fastcall NtQueryEaFile(
        int a1,
        unsigned __int64 a2,
        void *a3,
        ULONG a4,
        char a5,
        void *Src,
        size_t Size,
        ULONG *a8,
        char a9)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v12 = 0;
  FileObject = 0LL;
  v13 = 0LL;
  v63 = 0LL;
  P = 0LL;
  v56 = 0;
  v61 = 0;
  v68 = 0LL;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = CurrentThread->PreviousMode;
  v57 = PreviousMode;
  if ( !PreviousMode )
  {
    if ( Src && (_DWORD)Size )
    {
      v56 = 1;
      Pool2 = (UNICODE_STRING *)ExAllocatePool2(0x41uLL, (unsigned int)Size, 0x61456F49u);
      v16 = Pool2;
      P = Pool2;
      if ( !Pool2 )
      {
        return 3221225626LL;
      }
      memmove(Pool2, Src, (unsigned int)Size);
      v12 = 0;
    }
    else
    {
      v16 = (UNICODE_STRING *)P;
    }
    if ( a8 )
    {
      v61 = *a8;
    }
LABEL_27:
    LOBYTE(v12) = PreviousMode;
    v24 = IopReferenceFileObject(a1, 8, v12, (int)&FileObject, 0LL);
    if ( v24 < 0 )
    {
      if ( v56 )
      {
        ExFreePoolWithTag(v16, 0);
      }
      return (unsigned int)v24;
    }
    v25 = FileObject;
    p_Flags = &FileObject->Flags;
    if ( (FileObject->Flags & 2) != 0 )
    {
      v27 = (FileObject->Flags & 4) != 0;
      v28 = KeGetCurrentThread();
      --v28->KernelApcDisable;
      v29 = FileObject;
      v32 = KeAbPreAcquire(&FileObject->Lock, 0LL, 0LL);
      v58[0] = 0;
      if ( _InterlockedExchange((volatile __int32 *)&v29->Busy, 1) )
      {
        LOBYTE(v31) = v27;
        LOBYTE(v30) = PreviousMode;
        v25 = FileObject;
        v33 = IopWaitAndAcquireFileObjectLock((_DWORD)FileObject, v30, v31, v32, (__int64)v58);
      }
      else
      {
        if ( v32 )
        {
          *(_BYTE *)(v32 + 10) = 1;
        }
        v25 = FileObject;
        PsReferenceSiloContext(FileObject);
        v33 = 0;
      }
      if ( !v58[0] )
      {
        v34 = 1;
        v16 = (UNICODE_STRING *)P;
LABEL_35:
        v58[0] = v34;
        IopResetEvent(v25);
        RelatedDeviceObject = IoGetRelatedDeviceObject(v25);
        LOBYTE(v35) = RelatedDeviceObject->StackSize;
        v36 = (IRP *)IopAllocateIrpExReturn(RelatedDeviceObject, v35, 0LL);
        v37 = v36;
        v67 = v36;
        if ( v36 )
        {
          v36->Tail.Overlay.OriginalFileObject = v25;
          v36->Tail.Overlay.Thread = CurrentThread;
          v36->RequestorMode = v57;
          v38 = &v36->Flags;
          if ( v34 )
          {
            v39 = (struct _IO_STATUS_BLOCK *)a2;
            v40 = 0LL;
          }
          else
          {
            *v38 = 4;
            v39 = (struct _IO_STATUS_BLOCK *)&v68;
            v40 = (struct _KEVENT *)v63;
          }
          v37->UserEvent = v40;
          v37->UserIosb = v39;
          v37->Overlay.AllocationSize.QuadPart = 0LL;
          CurrentStackLocation = v37->Tail.Overlay.CurrentStackLocation;
          CurrentStackLocation[-1].MajorFunction = 7;
          CurrentStackLocation[-1].FileObject = v25;
          if ( v56 )
          {
            v37->Tail.Overlay.AuxiliaryBuffer = (PCHAR)v16;
            CurrentStackLocation[-1].Parameters.QueryDirectory.FileName = v16;
            CurrentStackLocation[-1].Parameters.Read.ByteOffset.LowPart = Size;
          }
          v42 = RelatedDeviceObject;
          Flags = RelatedDeviceObject->Flags;
          if ( (Flags & 4) == 0 )
          {
            if ( (Flags & 0x10) != 0 )
            {
              if ( a4 )
              {
                Mdl = IoAllocateMdl(a3, a4, 0, 1u, v37);
                if ( !Mdl )
                {
                  RtlRaiseStatus(3221225626LL);
                }
                v45 = v57;
                LOBYTE(v54) = v57;
                IopProbeAndLockPages_0(
                  (_DWORD)Mdl,
                  v54,
                  1,
                  (_DWORD)RelatedDeviceObject,
                  CurrentStackLocation[-1].MajorFunction);
                v42 = RelatedDeviceObject;
                v44 = a4;
                goto LABEL_45;
              }
              goto LABEL_43;
            }
LABEL_42:
            v37->UserBuffer = a3;
LABEL_43:
            v44 = a4;
LABEL_44:
            v45 = v57;
LABEL_45:
            CurrentStackLocation[-1].Parameters.Read.Length = v44;
            CurrentStackLocation[-1].Parameters.Create.EaLength = v61;
            CurrentStackLocation[-1].Flags = 0;
            v46 = 0;
            if ( a9 )
            {
              CurrentStackLocation[-1].Flags = 1;
              v46 = 1;
            }
            if ( a5 )
            {
              v46 |= 2u;
              CurrentStackLocation[-1].Flags = v46;
            }
            if ( a8 )
            {
              CurrentStackLocation[-1].Flags = v46 | 4;
            }
            v47 = v58[0];
            LOBYTE(Irp) = v45;
            result = IopSynchronousServiceTail(v42, v37, v25, 0LL, Irp, v58[0], 2);
            if ( !v47 )
            {
              LOBYTE(v48) = v45;
              return IopSynchronousApiServiceTail((unsigned int)result, v63, v37, v48, &v68, a2);
            }
            return result;
          }
          v44 = a4;
          if ( !a4 )
          {
            v37->AssociatedIrp.MasterIrp = 0LL;
            *v38 |= 0x50u;
            goto LABEL_44;
          }
          v52 = ExAllocatePool2(0x41uLL, a4, 0x42536F49u);
          v37->AssociatedIrp.MasterIrp = (struct _IRP *)v52;
          if ( v52 )
          {
            *v38 |= 0x70u;
            v42 = RelatedDeviceObject;
            goto LABEL_42;
          }
          IopExceptionCleanupEx((ULONG_PTR)v25, v37, 0LL, v63, (*p_Flags & 2) != 0);
          v51 = v16 == 0LL;
        }
        else
        {
          if ( (*p_Flags & 2) == 0 )
          {
            ExFreePoolWithTag(v13, 0);
          }
          if ( (*p_Flags & 2) != 0 )
          {
            IopReleaseFileObjectLock((ULONG_PTR)v25);
          }
          ObfDereferenceObject(v25);
          v51 = v56 == 0;
        }
        if ( !v51 )
        {
          ExFreePoolWithTag(v16, 0);
        }
        return 3221225626LL;
      }
      if ( v56 )
      {
        ExFreePoolWithTag(P, 0);
      }
    }
    else
    {
      v49 = (struct _KEVENT *)ExAllocatePool2(0x40uLL, 0x18uLL, 0x76456F49u);
      v13 = v49;
      v63 = v49;
      if ( v49 )
      {
        KeInitializeEvent(v49, SynchronizationEvent, 0);
        v34 = 0;
        goto LABEL_35;
      }
      if ( v56 )
      {
        ExFreePoolWithTag(v16, 0);
      }
      v33 = -1073741670;
    }
    ObfDereferenceObject(v25);
    return v33;
  }
  v15 = 0x7FFFFFFF0000LL;
  if ( a2 < 0x7FFFFFFF0000LL )
  {
    v15 = a2;
  }
  *(_DWORD *)v15 = *(_DWORD *)v15;
  ProbeForWrite(a3, a4, 4u);
  v12 = 0;
  if ( a8 )
  {
    v22 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a8 < 0x7FFFFFFF0000LL )
    {
      v22 = (__int64)a8;
    }
    v61 = *(_DWORD *)v22;
  }
  if ( !Src || !(_DWORD)Size )
  {
    v16 = (UNICODE_STRING *)P;
    goto LABEL_27;
  }
  v65 = 0LL;
  v59 = 0;
  v56 = 1;
  if ( ((unsigned __int8)Src & 3) != 0 )
  {
    ExRaiseDatatypeMisalignment();
  }
  v16 = (UNICODE_STRING *)ExAllocatePool2(0x61uLL, (unsigned int)Size, 0x61456F49u);
  P = v16;
  memmove(v16, Src, (unsigned int)Size);
  v17 = v16;
  v65 = v16;
  v18 = Size;
  v59 = Size;
  v12 = 0;
  while ( 1 )
  {
    if ( v18 < 5 )
    {
      v59 = 0;
      ExFreePoolWithTag(v16, 0);
      P = 0LL;
      *(_DWORD *)a2 = -2147483628;
      *(_QWORD *)(a2 + 8) = 0LL;
      return 2147483668LL;
    }
    v19 = *((unsigned __int8 *)&v17->MaximumLength + 2) + 6;
    if ( v18 < v19 )
    {
      break;
    }
    v20 = *(unsigned int *)&v17->Length;
    if ( !(_DWORD)v20 )
    {
      v59 = v18 - v19;
      if ( (int)(v18 - v19) >= 0 )
      {
        v13 = 0LL;
        goto LABEL_27;
      }
      break;
    }
    if ( ((*((unsigned __int8 *)&v17->MaximumLength + 2) + 9) & 0xFFFFFFFC) != (_DWORD)v20 )
    {
      break;
    }
    if ( (int)v20 < 0 )
    {
      break;
    }
    v18 -= v20;
    v59 = v18;
    if ( v18 < 0 )
    {
      break;
    }
    v17 = (UNICODE_STRING *)((char *)v17 + v20);
    v65 = v17;
  }
  v23 = (_DWORD)v17 - (_DWORD)v16;
  v59 = v23;
  ExFreePoolWithTag(v16, 0);
  P = 0LL;
  *(_DWORD *)a2 = -2147483628;
  *(_QWORD *)(a2 + 8) = v23;
  return 2147483668LL;
}
339 row(s)
