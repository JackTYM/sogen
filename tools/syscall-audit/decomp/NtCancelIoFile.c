__int64 __fastcall NtCancelIoFile(int a1, unsigned __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Object = 0LL;
  v4 = 0;
  CurrentThread = KeGetCurrentThread();
  PreviousMode = (unsigned __int8)CurrentThread->PreviousMode;
  if ( (_BYTE)PreviousMode )
  {
    v7 = 0x7FFFFFFF0000LL;
    if ( a2 < 0x7FFFFFFF0000LL )
    {
      v7 = a2;
    }
    *(_DWORD *)v7 = *(_DWORD *)v7;
  }
  result = IopReferenceFileObject(a1, 0, PreviousMode, (int)&Object, 0LL);
  if ( (int)result >= 0 )
  {
    v9 = KeGetCurrentThread();
    ++v9->OtherOperationCount;
    __incgsdword(0x2EE4u);
    CurrentIrql = KeGetCurrentIrql();
    __writecr8(1uLL);
    if ( KiIrqlFlags )
    {
      KiRaiseIrqlProcessIrqlFlags(CurrentIrql, 1LL);
    }
    p_SystemCallNumber = &CurrentThread[1].SystemCallNumber;
    v12 = *(unsigned int **)p_SystemCallNumber;
    for ( i = Object; p_SystemCallNumber != v12; v12 = *(unsigned int **)v12 )
    {
      if ( *((PVOID *)v12 + 20) == i )
      {
        v4 = 1;
        IoCancelIrp((PIRP)(v12 - 8));
      }
    }
    if ( KiIrqlFlags )
    {
      KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), CurrentIrql);
    }
    __writecr8(CurrentIrql);
    if ( v4 )
    {
      Interval.QuadPart = -100000LL;
      while ( 2 )
      {
        v14 = 0;
        v15 = KeGetCurrentIrql();
        __writecr8(1uLL);
        if ( KiIrqlFlags )
        {
          KiRaiseIrqlProcessIrqlFlags(v15, 1LL);
        }
        for ( j = *(unsigned int **)p_SystemCallNumber; p_SystemCallNumber != j; j = *(unsigned int **)j )
        {
          if ( *((PVOID *)j + 20) == i )
          {
            v14 = 1;
            break;
          }
        }
        if ( KiIrqlFlags )
        {
          KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v15);
        }
        __writecr8(v15);
        if ( v14 )
        {
          KeDelayExecutionThread(0, 0, &Interval);
          continue;
        }
        break;
      }
    }
    v17 = Object;
    IopCancelIrpsInFileObjectList(
      (_DWORD)Object,
      KeGetCurrentThread()->ApcState.Process,
      0,
      (unsigned int)KeGetCurrentThread(),
      1,
      0);
    *(_DWORD *)a2 = 0;
    *(_QWORD *)(a2 + 8) = 0LL;
    ObfDereferenceObject(v17);
    return 0LL;
  }
  return result;
}
92 row(s)
