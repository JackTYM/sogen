__int64 __fastcall NtCopyFileChunk(
        __int64 a1,
        void *a2,
        void *a3,
        NTSTATUS *a4,
        ULONG Length,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9,
        int a10)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v55 = a1;
  v52 = 0LL;
  v12 = 0LL;
  v44 = 0LL;
  v50 = 0LL;
  EventHandle = (HANDLE)-1LL;
  v13 = 0LL;
  v42 = 0LL;
  v51 = 0LL;
  FileObject = 0LL;
  v47 = 0LL;
  AlignmentRequirement = 0LL;
  ((void (*)(void))Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline)();
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v40 = PreviousMode;
  if ( (a10 & 0xFFFFFFFE) != 0 )
  {
    return 3221225485LL;
  }
  if ( PreviousMode )
  {
    if ( (a6 & 3) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v17 = 0x7FFFFFFF0000LL;
    v43 = *(_QWORD *)a6;
    if ( (a7 & 3) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v46 = *(_QWORD *)a7;
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )
    {
      v17 = (__int64)a4;
    }
    *(_DWORD *)v17 = *(_DWORD *)v17;
    v18 = *(_OWORD *)a4;
    v52 = *(_OWORD *)a4;
    v19 = v46;
    v20 = v43;
  }
  else
  {
    v20 = *(_QWORD *)a6;
    v43 = *(_QWORD *)a6;
    v19 = *(_QWORD *)a7;
    v46 = *(_QWORD *)a7;
    v18 = *(_OWORD *)a4;
    v52 = *(_OWORD *)a4;
  }
  if ( (a10 & 1) != 0 )
  {
    v54[0] = a1;
    v54[1] = v20;
    v54[2] = v19;
    v54[3] = Length;
    return IopXxxControlFile(a2, (__int64)a4, 623428, (__int64)v54, 32, 0LL, 0, 0, 1);
  }
  if ( (unsigned int)Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v20) )
  {
    LOBYTE(v22) = PreviousMode;
    File = IopReferenceFileObject(a1, 1, v22, (int)&FileObject, 0LL);
    if ( File < 0 )
    {
      goto LABEL_67;
    }
    File = ObReferenceFileObjectForWrite((ULONG_PTR)a2);
    v26 = File;
    if ( File >= 0 && (unsigned __int8)IopFileObjectRevoked(0LL) )
    {
      File = -1073739504;
      v26 = -1073739504;
    }
    if ( v26 < 0 )
    {
      goto LABEL_67;
    }
    if ( (FileObject->Flags & 8) != 0 )
    {
      AlignmentRequirement = IoGetRelatedDeviceObject(FileObject)->AlignmentRequirement;
    }
    else
    {
      AlignmentRequirement = 0LL;
    }
    v21 = 0LL;
    if ( (MEMORY[0x50] & 8) != 0 )
    {
      v21 = IoGetRelatedDeviceObject(0LL)->AlignmentRequirement;
      if ( (unsigned int)AlignmentRequirement <= (unsigned int)v21 )
      {
        AlignmentRequirement = (unsigned int)v21;
      }
    }
  }
  IsEnabledDeviceUsageNoInline = Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v21);
  v28 = Length + 80LL;
  if ( IsEnabledDeviceUsageNoInline )
  {
    Pool2 = ExAllocatePool2(0x4BuLL, AlignmentRequirement + v28, 0x70436F49u);
    v47 = Pool2;
    if ( !Pool2 )
    {
LABEL_26:
      File = -1073741670;
      goto LABEL_67;
    }
    v12 = (_QWORD *)(~AlignmentRequirement & (AlignmentRequirement + Pool2 + 80));
  }
  else
  {
    v12 = (_QWORD *)ExAllocatePool2(0x43uLL, v28, 0x70436F49u);
    if ( !v12 )
    {
      goto LABEL_26;
    }
  }
  if ( (unsigned int)Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v24) )
  {
    v30 = v12 - 10;
  }
  else
  {
    v30 = v12;
    v12 += 10;
  }
  memset_0(v30, 0, 0x50uLL);
  v30[8] = v43;
  if ( (unsigned int)Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v31) )
  {
    v30[7] = FileObject;
    v30[6] = 0LL;
    v30[9] = v47;
    FileObject = 0LL;
  }
  if ( !(unsigned int)Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v32) )
  {
    LOBYTE(v33) = PreviousMode;
    File = IopReferenceFileObject(v55, 1, v33, (int)v30 + 56, 0LL);
    if ( File < 0 )
    {
      goto LABEL_67;
    }
    File = ObReferenceFileObjectForWrite((ULONG_PTR)a2);
    v34 = File;
    if ( File >= 0 && (unsigned __int8)IopFileObjectRevoked(v30[6]) )
    {
      File = -1073739504;
      v34 = -1073739504;
    }
    if ( v34 < 0 )
    {
      goto LABEL_67;
    }
    PreviousMode = v40;
  }
  v35 = v30[6];
  v58 = *(_DWORD *)(v35 + 80) & 2;
  v36 = 0LL;
  if ( !v58 )
  {
    v36 = (__int64)a3;
  }
  File = IopPopulateCopyWriteWorkerData(
           v35,
           (_DWORD)a4,
           (_DWORD)v12,
           Length,
           InitialState,
           (__int64)(v30 + 7),
           v36,
           a7,
           a9,
           HIDWORD(v44),
           (__int64)(v30 + 4));
  if ( File >= 0 )
  {
    if ( v58 )
    {
      *(_QWORD *)&ObjectAttributes.Length = 48LL;
      *(_QWORD *)&ObjectAttributes.Attributes = 512LL;
      ObjectAttributes.RootDirectory = 0LL;
      ObjectAttributes.ObjectName = 0LL;
      *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;
      File = ZwCreateEvent(&EventHandle, 0x1F0003u, &ObjectAttributes, SynchronizationEvent, 0);
      if ( File < 0 )
      {
        goto LABEL_67;
      }
      Object = 0LL;
      File = ObReferenceObjectByHandle(EventHandle, 0, (POBJECT_TYPE)ExEventObjectType, 0, &Object, 0LL);
      v24 = (unsigned __int64)Object;
      v50 = Object;
      if ( File < 0 )
      {
        goto LABEL_67;
      }
      *(_QWORD *)(v30[4] + 80LL) = Object;
      if ( a3 )
      {
        v49 = 0LL;
        File = ObReferenceObjectByHandle(a3, 2u, (POBJECT_TYPE)ExEventObjectType, PreviousMode, &v49, 0LL);
        v13 = (struct _KEVENT *)v49;
        v42 = v49;
        if ( File < 0 )
        {
          goto LABEL_67;
        }
        KeResetEvent((PRKEVENT)v49);
      }
      v51 = (PIRP)v30[4];
    }
    *(_OWORD *)a4 = v18;
    PsReferenceSiloContext(v30[7]);
    File = IopReadFile(v30[7], (__int64)a4, v12, Length, a6, a8, 0LL, 0LL, 0x40000000, 0LL, (__int64)(v30 + 7));
    v12 = 0LL;
    if ( File >= 0 && v58 )
    {
      File = IopWaitForSynchronousIoEvent(v51);
      if ( File >= 0 )
      {
        if ( PreviousMode )
        {
          if ( IoIs32bitProcess(0LL) )
          {
            v37 = (NTSTATUS *)(unsigned int)*a4;
            v24 = (unsigned int)*v37;
            v39 = *v37;
          }
          else
          {
            v39 = *a4;
          }
          File = v39;
        }
        else if ( IoIs32bitProcess(0LL) )
        {
          File = *(_DWORD *)(unsigned int)*a4;
        }
        else
        {
          File = *a4;
        }
      }
      if ( v13 )
      {
        KeSetEvent(v13, 0, 0);
      }
    }
    else if ( File >= 0 )
    {
      File = 259;
    }
    goto LABEL_67;
  }
  v30[6] = 0LL;
LABEL_67:
  if ( v12 )
  {
    LOBYTE(v23) = 1;
    IopFreeCopyObjectsFromDataBuffer(v12, v23);
  }
  if ( (unsigned int)Feature_CopyFileChunk_BufferAlignment__private_IsEnabledDeviceUsageNoInline(v24) && FileObject )
  {
    ObfDereferenceObject(FileObject);
  }
  if ( EventHandle != (HANDLE)-1LL )
  {
    ObCloseHandle(EventHandle, 0);
  }
  if ( v13 )
  {
    ObfDereferenceObject(v13);
  }
  return (unsigned int)File;
}
291 row(s)
