__int64 __fastcall NtCreateWorkerFactory(
        __int64 *a1,
        __int64 a2,
        int a3,
        void *a4,
        ULONG_PTR BugCheckParameter1,
        __int64 a6,
        __int64 a7,
        int a8,
        __int64 a9,
        __int64 a10)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Handle = 0LL;
  v31 = 0LL;
  v32 = 0LL;
  v30 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v14 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
    {
      v14 = (__int64)a1;
    }
    *(_QWORD *)v14 = *(_QWORD *)v14;
  }
  Pool2 = ExAllocatePool2(0x41uLL, 0x28uLL, 0x63577054u);
  v16 = Pool2;
  if ( !Pool2 )
  {
    return (unsigned int)-1073741801;
  }
  *(_QWORD *)Pool2 = 0LL;
  *(_QWORD *)(Pool2 + 24) = 0LL;
  *(_WORD *)(Pool2 + 32) = 0;
  *(_BYTE *)(Pool2 + 34) = 0;
  v29 = 0LL;
  v17 = ObReferenceObjectByHandle(a4, 2u, IoCompletionObjectType, PreviousMode, &v29, 0LL);
  *(_QWORD *)(v16 + 8) = v29;
  if ( v17 < 0 )
  {
LABEL_23:
    ExFreePoolWithTag((PVOID)v16, 0);
    return (unsigned int)v17;
  }
  KeDisableQueueingPriorityIncrement();
  v17 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x66577845u, (__int64)&v30, 0LL, 0LL);
  if ( v17 < 0 )
  {
LABEL_22:
    ObfDereferenceObject(*(PVOID *)(v16 + 8));
    goto LABEL_23;
  }
  v18 = v30;
  if ( KeGetCurrentThread()->ApcState.Process != v30 )
  {
    v17 = -1073741811;
    goto LABEL_21;
  }
  v17 = ObOpenObjectByPointer(v30, 0x200u, 0LL, 0x2Au, (POBJECT_TYPE)PsProcessType, 0, &Handle);
  if ( v17 < 0 )
  {
LABEL_21:
    ObfDereferenceObjectWithTag(v18, 0x66577845u);
    goto LABEL_22;
  }
  MiniCompletionPacket = IoAllocateMiniCompletionPacket(&ExpWorkerFactoryCompletionPacketRoutine, v16);
  *(_QWORD *)(v16 + 16) = MiniCompletionPacket;
  v21 = 0;
  if ( !MiniCompletionPacket )
  {
    v17 = -1073741801;
    goto LABEL_20;
  }
  LOBYTE(v20) = PreviousMode;
  LOBYTE(v21) = PreviousMode;
  v17 = ObCreateObjectEx(v21, (_DWORD)ExpWorkerFactoryObjectType, a3, v20, Object, 672, 0, 0, (__int64)&v31, 0LL);
  if ( v17 < 0 )
  {
    v26 = *(_QWORD *)(v16 + 16);
    *(_QWORD *)(v26 + 56) = 0LL;
    IopFreeMiniCompletionPacket(v26);
LABEL_20:
    ObCloseHandle(Handle, 0);
    goto LABEL_21;
  }
  v22 = v31;
  *((_QWORD *)v31 + 2) = v16;
  v22[14] = -10000000LL * ExpWorkerFactoryThreadIdleTimeoutInSeconds;
  *((_DWORD *)v22 + 94) = 0;
  *((_DWORD *)v22 + 95) = a8;
  *((_DWORD *)v22 + 99) = 0;
  v22[48] = 0LL;
  v23 = 0x10000LL;
  if ( a9 )
  {
    v23 = a9;
  }
  v22[7] = v23;
  memset_0(v22 + 15, 0, 0x100uLL);
  v22[51] = 0LL;
  v24 = 4096LL;
  if ( a10 )
  {
    v24 = a10;
  }
  v22[8] = v24;
  v22[3] = a6;
  v22[4] = a7;
  v22[50] = 0LL;
  *((_DWORD *)v22 + 104) = 0;
  v22[5] = Handle;
  v22[6] = v18;
  *((_DWORD *)v22 + 98) = 0;
  *((_DWORD *)v22 + 166) = 0;
  *((_DWORD *)v22 + 152) = 1;
  KeInitializeTimer2(v22 + 53, 0LL, 0LL, 8LL);
  ExpInitializeThreadHistory(v22);
  v22[13] = 0LL;
  PsReferenceSiloContext();
  KeRegisterObjectNotification(v22 + 53, &ExpWorkerFactoryManagerQueue, v22 + 70);
  v33[0] = 0LL;
  v33[1] = -1LL;
  KeSetTimer2(v22 + 53, v22[14], -v22[14], v33);
  result = ObInsertObjectEx(v22, 0, 0LL, (__int64)&v32);
  if ( (int)result >= 0 )
  {
    *a1 = v32;
  }
  return result;
}
133 row(s)
