__int64 __fastcall NtAlpcCreateResourceReserve(HANDLE Handle, int a2, __int64 a3, _DWORD *a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v15 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( a2 )
  {
    v9 = -1073741811;
  }
  else
  {
    PreviousMode = KeGetCurrentThread()->PreviousMode;
    if ( PreviousMode )
    {
      v13 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )
      {
        v13 = (__int64)a4;
      }
      *(_DWORD *)v13 = *(_DWORD *)v13;
    }
    Object = 0LL;
    v9 = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
    if ( v9 >= 0 )
    {
      v10 = a3;
      v11 = Object;
      v9 = AlpcpCreateReserve(Object, v10, &v15);
      if ( v9 >= 0 )
      {
        *a4 = v15 | 0x80000000;
      }
      ObfDereferenceObject(v11);
    }
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v9;
}
40 row(s)
