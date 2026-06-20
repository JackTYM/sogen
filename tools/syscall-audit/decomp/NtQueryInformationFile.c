NTSTATUS __stdcall NtQueryInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        FILE_INFORMATION_CLASS FileInformationClass)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v97 = Length;
  v101 = FileInformation;
  v99 = IoStatusBlock;
  v114 = IoStatusBlock;
  v107 = IoStatusBlock;
  v111 = IoStatusBlock;
  v112 = FileInformation;
  LODWORD(v104) = Length;
  v8 = FileInformationClass;
  LODWORD(v100) = FileInformationClass;
  memset(&Event, 0, sizeof(Event));
  v119 = 0LL;
  HandleInformation = 0LL;
  v90 = 0;
  v88 = 0;
  CurrentThread = KeGetCurrentThread();
  v89 = *((_BYTE *)CurrentThread + 562);
  v94 = v89;
  result = IopValidateQueryInformationParameters(
             v89,
             (_DWORD)CurrentThread,
             (_DWORD)IoStatusBlock,
             (_DWORD)FileInformation,
             Length,
             FileInformationClass);
  if ( result < 0 )
  {
    return result;
  }
  if ( !v89 && FileInformationClass == FileCaseSensitiveInformationForceAccessCheck )
  {
    v8 = FileCaseSensitiveInformation;
    LODWORD(v100) = 71;
    v88 = 1;
  }
  v10 = IopQueryOperationAccess[v8];
  Object = 0LL;
  v11 = ObReferenceObjectByHandle(FileHandle, v10, (POBJECT_TYPE)IoFileObjectType, v89, &Object, &HandleInformation);
  v12 = (struct _FILE_OBJECT *)Object;
  v102 = (ULONG_PTR)Object;
  BugCheckParameter2 = (ULONG_PTR)Object;
  if ( v11 >= 0 )
  {
    v59 = (_DWORD *)*((_QWORD *)Object + 26);
    if ( v59 )
    {
      if ( (*v59 & 4) != 0 )
      {
        IsProcessAppContainer = PsIsProcessAppContainer(KeGetCurrentThread()->ApcState.Process);
        v12 = (struct _FILE_OBJECT *)Object;
        if ( IsProcessAppContainer )
        {
          ObfDereferenceObject(Object);
          v11 = -1073739504;
        }
        v102 = BugCheckParameter2;
      }
    }
  }
  v95 = v11;
  if ( v11 < 0 )
  {
    return v11;
  }
  if ( v8 == FileAccessInformation )
  {
    v77 = KeGetCurrentThread();
    ++v77->OtherOperationCount;
    __incgsdword(0x2EE4u);
    if ( Length < 4 )
    {
      ObfDereferenceObject(Object);
      return -1073741820;
    }
    *(_DWORD *)FileInformation = HandleInformation.GrantedAccess;
    v78 = v99;
    v99->Status = 0;
    v78->Information = 4LL;
    v11 = v95;
    ObfDereferenceObject(Object);
    return v11;
  }
  p_Flags = &v12->Flags;
  v113 = (__int64)&v12->Flags;
  if ( (v12->Flags & 0x800) != 0 )
  {
    AttachedDevice = IoGetAttachedDevice(v12->DeviceObject);
  }
  else
  {
    AttachedDevice = IoGetRelatedDeviceObject(v12);
  }
  v15 = AttachedDevice;
  v108 = AttachedDevice;
  FastIoDispatch = AttachedDevice->DriverObject->FastIoDispatch;
  if ( v8 == FileIsRemoteDeviceInformation )
  {
    if ( v97 )
    {
      *v101 = (v12->DeviceObject->Characteristics & 0x10) != 0;
      v76 = v99;
      v99->Status = 0;
      v76->Information = 1LL;
      goto LABEL_93;
    }
    ObfDereferenceObject(v12);
    return -1073741820;
  }
  else
  {
    if ( v8 == FileVolumeNameInformation )
    {
      FileVolumeNameInformation = IopGetFileVolumeNameInformation(v12->DeviceObject, v99, v101, v97);
      ObfDereferenceObject(v12);
      return FileVolumeNameInformation;
    }
    if ( (*p_Flags & 2) != 0 )
    {
      v17 = (*p_Flags & 4) != 0;
      v18 = KeGetCurrentThread();
      --v18->KernelApcDisable;
      v19 = (volatile __int32 *)Object;
      v20 = KeAbPreAcquire((char *)Object + 128, 0LL, 0LL);
      v91 = 0;
      if ( _InterlockedExchange(v19 + 29, 1) )
      {
        v12 = (struct _FILE_OBJECT *)Object;
        v11 = IopWaitAndAcquireFileObjectLock((_DWORD)Object, v89, v17, v20, (__int64)&v91);
      }
      else
      {
        if ( v20 )
        {
          *(_BYTE *)(v20 + 10) = 1;
        }
        v12 = (struct _FILE_OBJECT *)Object;
        PsReferenceSiloContext(Object);
        v11 = 0;
      }
      v95 = v11;
      if ( v91 )
      {
        goto LABEL_93;
      }
      if ( v8 == FilePositionInformation )
      {
        v92 = 1;
        if ( v97 < 8 )
        {
          v11 = -1073741820;
        }
        else
        {
          *(_QWORD *)v101 = v12->CurrentByteOffset.QuadPart;
          v92 = 0;
          v68 = v99;
          v99->Status = 0;
          v68->Information = 8LL;
        }
LABEL_92:
        IopReleaseFileObjectLock((ULONG_PTR)v12);
LABEL_93:
        ObfDereferenceObject(v12);
        return v11;
      }
      if ( FastIoDispatch )
      {
        if ( (FastIoQueryBasicInfo = (ULONG_PTR)FastIoDispatch->FastIoQueryBasicInfo,
              v102 = FastIoQueryBasicInfo,
              v110 = (_DWORD *)FastIoQueryBasicInfo,
              FastIoQueryStandardInfo = FastIoDispatch->FastIoQueryStandardInfo,
              v8 == FileBasicInformation)
          && FastIoQueryBasicInfo
          || v8 == FileStandardInformation && FastIoQueryStandardInfo )
        {
          v115 = 0LL;
          v93 = 0;
          if ( (MmVerifierData & 0x10) != 0
            && (Entry = v15->DriverObject, MmIsDriverVerifying((struct _DRIVER_OBJECT *)Entry)) )
          {
            v61 = (void *)VfFastIoSnapState(Entry);
          }
          else
          {
            v61 = 0LL;
          }
          Entry = v61;
          v116 = v61;
          LOBYTE(v21) = 1;
          if ( v8 == FileBasicInformation )
          {
            v63 = v102;
            v62 = guard_dispatch_icall_no_overrides(v12, v21, v101, &v115, v15);
            v87 = v62;
          }
          else
          {
            v62 = guard_dispatch_icall_no_overrides(v12, v21, v101, &v115, v15);
            v87 = v62;
            v63 = v102;
          }
          if ( Entry )
          {
            VfFastIoCheckState(Entry, v63);
            v62 = v87;
          }
          if ( v62 )
          {
            v11 = v115;
            v95 = v115;
            v93 = 1;
            *(_OWORD *)&v99->Status = v115;
            v62 = v87;
          }
          if ( v62 )
          {
            goto LABEL_92;
          }
        }
      }
      v24 = 1;
      v102 = BugCheckParameter2;
    }
    else
    {
      KeInitializeEvent(&Event, SynchronizationEvent, 0);
      v24 = 0;
    }
    IopResetEvent(v12);
    Irp = IopAllocateIrpExReturn(v15, (unsigned __int8)v15->StackSize, 0LL);
    v26 = Irp;
    v100 = Irp;
    v27 = Irp;
    v113 = Irp;
    v118 = Irp;
    if ( Irp )
    {
      *(_QWORD *)(Irp + 192) = v12;
      v117 = (__int64 *)(Irp + 152);
      *(_QWORD *)(Irp + 152) = CurrentThread;
      Entry = (PVOID)(Irp + 64);
      *(_BYTE *)(Irp + 64) = v89;
      if ( v24 )
      {
        *(_BYTE *)(Irp + 71) |= 2u;
        p_Event = 0LL;
        v29 = (unsigned int *)(v26 + 16);
      }
      else
      {
        if ( v89 == 1 )
        {
          v90 = KeSetKernelStackSwapEnable(0);
          v26 = v27;
        }
        v29 = (unsigned int *)(v27 + 16);
        *(_DWORD *)(v27 + 16) = 4;
        v107 = (PIO_STATUS_BLOCK)&v119;
        p_Event = &Event;
      }
      v104 = v29;
      v112 = v29;
      *(_QWORD *)(v26 + 80) = p_Event;
      *(_QWORD *)(v26 + 72) = v107;
      *(_QWORD *)(v27 + 88) = 0LL;
      v30 = (_QWORD *)(*(_QWORD *)(v27 + 184) - 72LL);
      CurrentThread = v30;
      *(_BYTE *)v30 = 5;
      v31 = v102;
      v30[6] = v102;
      *(_QWORD *)(v27 + 112) = v101;
      *(_QWORD *)(v27 + 24) = 0LL;
      *(_QWORD *)(v27 + 8) = 0LL;
      Pool2 = ExAllocatePool2(0x41uLL, v97, 0x42536F49u);
      *(_QWORD *)(v27 + 24) = Pool2;
      if ( Pool2 )
      {
        v110 = v29;
        *v29 |= 0x870u;
        v33 = CurrentThread;
        v34 = v97;
        *((_DWORD *)CurrentThread + 2) = v97;
        v33[4] = v8;
        if ( v88 )
        {
          *((_BYTE *)v33 + 2) |= 1u;
        }
        IopQueueThreadIrp(v27);
        v37 = KeGetCurrentThread();
        ++v37->OtherOperationCount;
        __incgsdword(0x2EE4u);
        v38 = 0;
        NumaNodeInformation = 0;
        if ( v8 == FileModeInformation )
        {
          v40 = BugCheckParameter2;
          ModeInformation = IopGetModeInformation(BugCheckParameter2, v35, v36, *(_QWORD *)(v27 + 24));
          *v74 = ModeInformation;
          *(_QWORD *)(v27 + 56) = 4LL;
        }
        else if ( v8 == FileAlignmentInformation )
        {
          **(_DWORD **)(v27 + 24) = v15->AlignmentRequirement;
          *(_QWORD *)(v27 + 56) = 4LL;
          v40 = BugCheckParameter2;
        }
        else if ( v8 == FileIoCompletionNotificationInformation )
        {
          v82 = *(int **)(v27 + 24);
          *v82 = 0;
          v40 = BugCheckParameter2;
          v83 = 0;
          if ( (*(_DWORD *)(BugCheckParameter2 + 80) & 0x4000000) != 0 )
          {
            v83 = 2;
            *v82 = 2;
          }
          if ( (*(_DWORD *)(v40 + 80) & 0x8000000) != 0 )
          {
            v83 |= 4u;
            *v82 = v83;
          }
          if ( (*(_DWORD *)(v40 + 80) & 0x2000000) != 0 )
          {
            *v82 = v83 | 1;
          }
          *(_QWORD *)(v27 + 56) = 4LL;
        }
        else if ( v8 == FileIoPriorityHintInformation )
        {
          v40 = BugCheckParameter2;
          v84 = *(_QWORD *)(BugCheckParameter2 + 208);
          if ( v84 && (v85 = *(_DWORD *)(v84 + 88)) != 0 )
          {
            v86 = v85 - 1;
          }
          else
          {
            v86 = 2;
          }
          **(_DWORD **)(v27 + 24) = v86;
          *(_QWORD *)(v27 + 56) = 4LL;
        }
        else
        {
          if ( v8 == FileProcessIdsUsingFileInformation )
          {
            v80 = *(_QWORD *)(v27 + 24);
            LODWORD(v103) = 0;
            v81 = v34;
            v40 = BugCheckParameter2;
            NumaNodeInformation = IopQueryProcessIdsUsingFile(BugCheckParameter2, v80, v81, &v103);
            v38 = 1;
            if ( NumaNodeInformation == -1073741820 )
            {
              v99->Information = (unsigned int)v103;
            }
            else
            {
              *(_QWORD *)(v27 + 56) = (unsigned int)v103;
            }
          }
          else
          {
            v40 = BugCheckParameter2;
            if ( v8 == FileNumaNodeInformation )
            {
              NumaNodeInformation = IopGetNumaNodeInformation(BugCheckParameter2, *(_QWORD *)(v27 + 24));
              v38 = 1;
              if ( NumaNodeInformation >= 0 )
              {
                *(_QWORD *)(v27 + 56) = 2LL;
              }
            }
            else if ( v8 == FileAllInformation )
            {
              v41 = *(_DWORD **)(v27 + 24);
              v41[19] = HandleInformation.GrantedAccess;
              v42 = *(_DWORD *)(v40 + 80);
              v43 = (v42 >> 3) & 2 | 4;
              if ( (v42 & 0x20) == 0 )
              {
                v43 = (*(_DWORD *)(v40 + 80) >> 3) & 2;
              }
              v44 = v43 | 8;
              if ( (v42 & 8) == 0 )
              {
                v44 = v43;
              }
              if ( (v42 & 2) != 0 )
              {
                if ( (v42 & 4) != 0 )
                {
                  v44 |= 0x10u;
                }
                else
                {
                  v44 |= 0x20u;
                }
              }
              v45 = v44 | 0x1000;
              if ( (v42 & 0x10000) == 0 )
              {
                v45 = v44;
              }
              v41[22] = v45;
              v41[23] = v15->AlignmentRequirement;
              *(_QWORD *)(v27 + 56) = 12LL;
            }
          }
          if ( !v38 )
          {
            CurrentThread = (PVOID)v40;
            if ( !v24 )
            {
              PsReferenceSiloContext(v40);
            }
            v46 = *(_QWORD *)(v40 + 208);
            if ( v46 && (v72 = *(_DWORD *)(v46 + 88)) != 0 )
            {
              v51 = v72 << 17;
            }
            else
            {
              v47 = KeGetCurrentThread();
              v48 = (*((_DWORD *)&v47[1].SwapListEntry + 2) >> 9) & 7;
              v49 = v47->Process[1].Padding[3];
              if ( v49 )
              {
                v50 = *(_DWORD *)(v49 + 1084);
                if ( v48 >= v50 )
                {
                  v48 = v50;
                }
              }
              if ( v48 < 2 && v47 == KeGetCurrentThread() && LODWORD(v47[1].Timer.TimerListEntry.Flink) )
              {
                v48 = 2;
              }
              v51 = (v48 << 17) + 0x20000;
              v40 = BugCheckParameter2;
            }
            v52 = v104;
            v53 = *v104 & 0xFFF1FFFF;
            v54 = v53 | v51;
            *v104 = v53;
            *v110 = v54;
            if ( IoGetIoPriorityHint((PIRP)v27) < IoPriorityNormal && !*(_BYTE *)Entry )
            {
              v75 = *v117;
              if ( !*v117 || (*(_DWORD *)(v75 + 116) & 0x400) == 0 && (*(_DWORD *)(v75 + 1444) & 0x80u) == 0 )
              {
                ++IoKernelIssuedIoBoostedCount;
                *v52 = v54 & 0xFFF1FFFF | 0x60000;
              }
            }
            if ( *(char *)(v27 + 71) >= 0 && (v55 = *(_BYTE **)(v27 + 200)) != 0LL && (*v55 & 2) != 0 )
            {
              v56 = 1;
            }
            else
            {
              v56 = 0;
              v100 = v118;
            }
            if ( v56 )
            {
              v121 = 0LL;
              v121 = *(_OWORD *)(*(_QWORD *)(v100 + 200) + 24LL);
              v79 = IoSetActivityIdThread(&v121);
              NumaNodeInformation = IofCallDriver(v15, (PIRP)v27);
              IoSetActivityIdThread(v79);
            }
            else
            {
              NumaNodeInformation = IofCallDriver(v15, (PIRP)v27);
            }
            if ( !v24 )
            {
              ObDereferenceObjectDeferDelete(CurrentThread);
            }
LABEL_60:
            if ( NumaNodeInformation == 259 )
            {
              if ( !v24 )
              {
                v70 = KeWaitForSingleObject(&Event, Executive, v89, 0, 0LL);
                if ( v70 == 257 || v70 == 192 )
                {
                  IopCancelAlertedRequest(&Event, (PIRP)v27);
                }
                NumaNodeInformation = v119;
                *(_OWORD *)&v99->Status = v119;
                goto LABEL_67;
              }
              v57 = *(_DWORD *)(v40 + 80) & 4;
              while ( (*(_BYTE *)(v40 + 152) & 0x7F) != 0 || !*(_DWORD *)(v40 + 156) )
              {
                v64 = 0;
                if ( v57 )
                {
                  v64 = v89;
                }
                v65 = KeWaitForSingleObject((PVOID)(v40 + 152), Executive, v64, 1u, 0LL);
                if ( v65 != 192 && v65 != 257 )
                {
                  break;
                }
                if ( !v57 && (*(_DWORD *)(&KeGetCurrentThread()[1].SwapListEntry + 1) & 1) == 0 )
                {
                  v66 = 0;
                  CurrentIrql = KeGetCurrentIrql();
                  __writecr8(1uLL);
                  if ( KiIrqlFlags )
                  {
                    KiRaiseIrqlProcessIrqlFlags(CurrentIrql, 1LL);
                  }
                  if ( !*(_DWORD *)(v40 + 156) )
                  {
                    v66 = *(_BYTE *)(v27 + 68) == 1;
                  }
                  if ( KiIrqlFlags )
                  {
                    KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), CurrentIrql);
                  }
                  __writecr8(CurrentIrql);
                  if ( !v66 )
                  {
                    continue;
                  }
                }
                IopCancelAlertedRequest((PVOID)(v40 + 152), (PIRP)v27);
                break;
              }
              v58 = BugCheckParameter2;
              NumaNodeInformation = *(_DWORD *)(BugCheckParameter2 + 56);
            }
            else
            {
              v103 = 0LL;
              if ( !v24 )
              {
                *(_QWORD *)(v27 + 80) = 0LL;
              }
              *(_QWORD *)(v27 + 72) = v99;
              v69 = KeGetCurrentIrql();
              __writecr8(1uLL);
              if ( KiIrqlFlags )
              {
                KiRaiseIrqlProcessIrqlFlags(v69, 1LL);
              }
              IopCompleteRequest(
                v27 + 120,
                (unsigned int)&v121,
                (unsigned int)&v103,
                (unsigned int)&BugCheckParameter2,
                (__int64)&v103);
              if ( KiIrqlFlags )
              {
                KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v69);
              }
              __writecr8(v69);
              if ( !v24 )
              {
                goto LABEL_67;
              }
              v58 = v40;
            }
            IopReleaseFileObjectLock(v58);
LABEL_67:
            if ( v90 )
            {
              KeSetKernelStackSwapEnable(1u);
            }
            return NumaNodeInformation;
          }
        }
        *(_DWORD *)(v27 + 48) = NumaNodeInformation;
        goto LABEL_60;
      }
      IopExceptionCleanupEx(v31, (PIRP)v27, 0LL, 0LL, (*(_DWORD *)(v31 + 80) & 2) != 0);
      if ( v90 )
      {
        KeSetKernelStackSwapEnable(1u);
      }
      return -1073741670;
    }
    else
    {
      if ( (*p_Flags & 2) != 0 )
      {
        IopReleaseFileObjectLock((ULONG_PTR)v12);
      }
      ObfDereferenceObject(v12);
      return -1073741670;
    }
  }
}
607 row(s)
