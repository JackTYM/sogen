__int64 __fastcall NtAlpcOpenSenderThread(_QWORD *a1, void *a2, __int128 *a3, __int64 a4, int a5, __int128 *Src)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v22 = 0LL;
  v23 = 0LL;
  v24 = 0LL;
  v25 = 0LL;
  v26 = 0LL;
  *(_QWORD *)&v27 = 0LL;
  DWORD2(v27) = 0;
  v21 = 0LL;
  BugCheckParameter2 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v10 = ObReferenceObjectByHandle(a2, 0x20000u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v10 >= 0 )
  {
    if ( PreviousMode )
    {
      v12 = 0x7FFFFFFF0000LL;
      v13 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
      {
        v13 = (__int64)a1;
      }
      *(_QWORD *)v13 = *(_QWORD *)v13;
      AlpcpProbeAndCaptureMessageHeader(a3, &v22);
      if ( (unsigned __int64)Src < 0x7FFFFFFF0000LL )
      {
        v12 = (__int64)Src;
      }
      RtlCopyVolatileMemory(&v25, (const void *)v12, 0x30uLL);
    }
    else
    {
      v22 = *a3;
      v23 = a3[1];
      v24 = *((_QWORD *)a3 + 4);
      v25 = *Src;
      v26 = Src[1];
      v27 = Src[2];
    }
    v14 = Object;
    v10 = AlpcpLookupMessage((_DWORD)Object, DWORD2(v23), v24, v11, (__int64)&BugCheckParameter2);
    if ( v10 < 0 )
    {
      ObfDereferenceObject(v14);
    }
    else
    {
      v15 = BugCheckParameter2;
      if ( (*(_DWORD *)(BugCheckParameter2 + 40) & 0x80u) != 0 )
      {
        AlpcpUnlockMessage(BugCheckParameter2);
        ObfDereferenceObject(v14);
        v10 = -1073740029;
      }
      else
      {
        v16 = *(_QWORD **)(BugCheckParameter2 + 32);
        if ( !v16 )
        {
          goto LABEL_18;
        }
        v17 = v16[161] - *((_QWORD *)&v22 + 1);
        if ( !v17 )
        {
          v17 = v16[162] - v23;
        }
        if ( v17 )
        {
LABEL_18:
          AlpcpUnlockMessage(BugCheckParameter2);
          ObfDereferenceObject(v14);
          v10 = -1073741790;
        }
        else
        {
          PsReferenceSiloContext(*(_QWORD *)(BugCheckParameter2 + 32));
          AlpcpUnlockMessage(v15);
          v10 = PsOpenThread((unsigned int)&v21, a5, (unsigned int)&v25, (unsigned int)&v22 + 8, 0, PreviousMode);
          ObfDereferenceObject(v16);
          ObfDereferenceObject(v14);
          if ( v10 >= 0 )
          {
            *a1 = v21;
          }
        }
      }
    }
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v10;
}
97 row(s)
