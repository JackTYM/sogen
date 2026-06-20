__int64 __fastcall NtNotifyChangeDirectoryFileEx(                                                                     | 
        int a1,                                                                                                       | 
        HANDLE Handle,                                                                                                | 
        __int64 a3,                                                                                                   | 
        __int64 a4,                                                                                                   | 
        unsigned int *a5,                                                                                             | 
        volatile void *Address,                                                                                       | 
        SIZE_T Length,                                                                                                | 
        int a8,                                                                                                       | 
        char a9,                                                                                                      | 
        int a10)                                                                                                      | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v51 = a3;                                                                                                           | 
  FileObject = 0LL;                                                                                                   | 
  v12 = 0LL;                                                                                                          | 
  v48 = 0LL;                                                                                                          | 
  CurrentThread = KeGetCurrentThread();                                                                               | 
  PreviousMode = CurrentThread->PreviousMode;                                                                         | 
  if ( PreviousMode )                                                                                                 | 
  {                                                                                                                   | 
    v15 = 0x7FFFFFFF0000LL;                                                                                           | 
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                    | 
    {                                                                                                                 | 
      v15 = (__int64)a5;                                                                                              | 
    }                                                                                                                 | 
    *(_DWORD *)v15 = *(_DWORD *)v15;                                                                                  | 
    v16 = Length;                                                                                                     | 
    if ( (_DWORD)Length )                                                                                             | 
    {                                                                                                                 | 
      ProbeForWrite(Address, (unsigned int)Length, 4u);                                                               | 
    }                                                                                                                 | 
    if ( (a8 & 0xFFFFF000) != 0 || !a8 )                                                                              | 
    {                                                                                                                 | 
      return 3221225485LL;                                                                                            | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v16 = Length;                                                                                                     | 
  }                                                                                                                   | 
  LOBYTE(a3) = PreviousMode;                                                                                          | 
  result = IopReferenceFileObject(a1, 1, a3, (int)&FileObject, 0LL);                                                  | 
  if ( (int)result >= 0 )                                                                                             | 
  {                                                                                                                   | 
    v18 = FileObject;                                                                                                 | 
    if ( FileObject->CompletionContext && (v51 & 0xFFFFFFFFFFFFFFFEuLL) != 0 )                                        | 
    {                                                                                                                 | 
      v32 = -1073741811;                                                                                              | 
LABEL_34:                                                                                                             | 
      ObfDereferenceObject(v18);                                                                                      | 
      return (unsigned int)v32;                                                                                       | 
    }                                                                                                                 | 
    if ( Handle )                                                                                                     | 
    {                                                                                                                 | 
      v49 = 0LL;                                                                                                      | 
      v32 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExEventObjectType, PreviousMode, &v49, 0LL);          | 
      v12 = v49;                                                                                                      | 
      v48 = v49;                                                                                                      | 
      if ( v32 < 0 )                                                                                                  | 
      {                                                                                                               | 
        goto LABEL_34;                                                                                                | 
      }                                                                                                               | 
      KeResetEvent((PRKEVENT)v49);                                                                                    | 
    }                                                                                                                 | 
    if ( (v18->Flags & 2) != 0 )                                                                                      | 
    {                                                                                                                 | 
      v33 = (v18->Flags & 4) != 0;                                                                                    | 
      v34 = KeGetCurrentThread();                                                                                     | 
      --v34->KernelApcDisable;                                                                                        | 
      v35 = FileObject;                                                                                               | 
      v36 = KeAbPreAcquire(&FileObject->Lock, 0LL, 0LL);                                                              | 
      v46[0] = 0;                                                                                                     | 
      if ( _InterlockedExchange((volatile __int32 *)&v35->Busy, 1) )                                                  | 
      {                                                                                                               | 
        LOBYTE(v38) = v33;                                                                                            | 
        LOBYTE(v37) = PreviousMode;                                                                                   | 
        v18 = FileObject;                                                                                             | 
        v32 = IopWaitAndAcquireFileObjectLock((_DWORD)FileObject, v37, v38, v36, (__int64)v46);                       | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        if ( v36 )                                                                                                    | 
        {                                                                                                             | 
          *(_BYTE *)(v36 + 10) = 1;                                                                                   | 
        }                                                                                                             | 
        v18 = FileObject;                                                                                             | 
        PsReferenceSiloContext(FileObject);                                                                           | 
        v32 = 0;                                                                                                      | 
      }                                                                                                               | 
      if ( v46[0] )                                                                                                   | 
      {                                                                                                               | 
        if ( v12 )                                                                                                    | 
        {                                                                                                             | 
          ObfDereferenceObject(v12);                                                                                  | 
        }                                                                                                             | 
        goto LABEL_34;                                                                                                | 
      }                                                                                                               | 
      v45 = 1;                                                                                                        | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v45 = 0;                                                                                                        | 
      if ( PreviousMode )                                                                                             | 
      {                                                                                                               | 
        v50 = KeGetCurrentThread();                                                                                   | 
        Process = v50->ApcState.Process;                                                                              | 
        if ( Process[1].ReadyTime )                                                                                   | 
        {                                                                                                             | 
          v20 = WORD2(Process[3].PerProcessorCycleTimes);                                                             | 
          if ( v20 == 332 || (v23 = v20 == 452, v21 = 0, v23) )                                                       | 
          {                                                                                                           | 
            v21 = 1;                                                                                                  | 
          }                                                                                                           | 
          v46[1] = v21;                                                                                               | 
          if ( v21 )                                                                                                  | 
          {                                                                                                           | 
            v22 = *a5;                                                                                                | 
            *(_DWORD *)v22 = *(_DWORD *)v22;                                                                          | 
            a5 = (unsigned int *)v22;                                                                                 | 
            v51 |= 1uLL;                                                                                              | 
          }                                                                                                           | 
        }                                                                                                             | 
        v18 = FileObject;                                                                                             | 
      }                                                                                                               | 
    }                                                                                                                 | 
    IopResetEvent(v18);                                                                                               | 
    RelatedDeviceObject = IoGetRelatedDeviceObject(v18);                                                              | 
    LOBYTE(v25) = v45 ^ 1;                                                                                            | 
    LOBYTE(v26) = RelatedDeviceObject->StackSize;                                                                     | 
    Irp = IopAllocateIrpExReturn(RelatedDeviceObject, v26, v25);                                                      | 
    v28 = (IRP *)Irp;                                                                                                 | 
    v49 = (PVOID)Irp;                                                                                                 | 
    if ( Irp )                                                                                                        | 
    {                                                                                                                 | 
      *(_QWORD *)(Irp + 192) = v18;                                                                                   | 
      *(_QWORD *)(Irp + 152) = CurrentThread;                                                                         | 
      *(_BYTE *)(Irp + 64) = PreviousMode;                                                                            | 
      *(_QWORD *)(Irp + 80) = v12;                                                                                    | 
      *(_QWORD *)(Irp + 72) = a5;                                                                                     | 
      *(_QWORD *)(Irp + 88) = v51;                                                                                    | 
      *(_QWORD *)(Irp + 96) = a4;                                                                                     | 
      v29 = *(_QWORD *)(Irp + 184);                                                                                   | 
      *(_BYTE *)(v29 - 72) = 12;                                                                                      | 
      v30 = a10;                                                                                                      | 
      *(_BYTE *)(v29 - 71) = (a10 != 1) + 2;                                                                          | 
      *(_QWORD *)(v29 - 24) = v18;                                                                                    | 
      if ( !v16 )                                                                                                     | 
      {                                                                                                               | 
LABEL_26:                                                                                                             | 
        *(_DWORD *)(v29 - 64) = v16;                                                                                  | 
        *(_DWORD *)(v29 - 56) = a8;                                                                                   | 
        if ( *(_BYTE *)(v29 - 71) == 3 )                                                                              | 
        {                                                                                                             | 
          *(_DWORD *)(v29 - 48) = v30;                                                                                | 
        }                                                                                                             | 
        *(_BYTE *)(v29 - 70) = 0;                                                                                     | 
        if ( a9 )                                                                                                     | 
        {                                                                                                             | 
          *(_BYTE *)(v29 - 70) = 1;                                                                                   | 
        }                                                                                                             | 
        LOBYTE(HandleInformation) = v45;                                                                              | 
        LOBYTE(Object) = PreviousMode;                                                                                | 
        return IopSynchronousServiceTail(RelatedDeviceObject, v28, v18, 0LL, Object, HandleInformation, 2);           | 
      }                                                                                                               | 
      Flags = RelatedDeviceObject->Flags;                                                                             | 
      if ( (Flags & 4) == 0 )                                                                                         | 
      {                                                                                                               | 
        if ( (Flags & 0x10) != 0 )                                                                                    | 
        {                                                                                                             | 
          Mdl = IoAllocateMdl((PVOID)Address, v16, 0, 1u, v28);                                                       | 
          if ( !Mdl )                                                                                                 | 
          {                                                                                                           | 
            RtlRaiseStatus(3221225626LL);                                                                             | 
          }                                                                                                           | 
          LOBYTE(v41) = PreviousMode;                                                                                 | 
          IopProbeAndLockPages_2((_DWORD)Mdl, v41, v42, (_DWORD)RelatedDeviceObject, *(unsigned __int8 *)(v29 - 72)); | 
          v30 = a10;                                                                                                  | 
          goto LABEL_26;                                                                                              | 
        }                                                                                                             | 
        goto LABEL_25;                                                                                                | 
      }                                                                                                               | 
      Pool2 = ExAllocatePool2(0x41uLL, v16, 0x42536F49u);                                                             | 
      v28->AssociatedIrp.MasterIrp = (struct _IRP *)Pool2;                                                            | 
      if ( Pool2 )                                                                                                    | 
      {                                                                                                               | 
        v28->Flags = 112;                                                                                             | 
        v30 = a10;                                                                                                    | 
LABEL_25:                                                                                                             | 
        v28->UserBuffer = (PVOID)Address;                                                                             | 
        goto LABEL_26;                                                                                                | 
      }                                                                                                               | 
      IopExceptionCleanupEx((ULONG_PTR)v18, v28, v12, 0LL, (v18->Flags & 2) != 0);                                    | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      IopAllocateIrpCleanup((ULONG_PTR)v18, v12);                                                                     | 
    }                                                                                                                 | 
    return 3221225626LL;                                                                                              | 
  }                                                                                                                   | 
  return result;                                                                                                      | 
}                                                                                                                     | 
203 row(s)
