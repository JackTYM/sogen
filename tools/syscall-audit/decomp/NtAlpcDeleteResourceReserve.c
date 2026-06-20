__int64 __fastcall NtAlpcDeleteResourceReserve(void *a1, int a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( a2 || a3 >= 0 )
  {
    v5 = -1073741811;
  }
  else
  {
    Object = 0LL;
    v5 = ObReferenceObjectByHandle(a1, 1u, AlpcPortObjectType, KeGetCurrentThread()->PreviousMode, &Object, 0LL);
    if ( v5 >= 0 )
    {
      v6 = Object;
      v7 = (unsigned int)a3;
      LODWORD(v7) = a3 & 0x7FFFFFFF;
      v8 = AlpcReferenceBlobByHandle(*((_QWORD *)Object + 2) + 40LL, v7, AlpcReserveType);
      v9 = v8;
      if ( v8 )
      {
        if ( (unsigned __int8)AlpcpDeleteBlob(v8) )
        {
          AlpcpDereferenceBlobEx(v9);
        }
        else
        {
          v5 = -1073741738;
        }
        AlpcpDereferenceBlobEx(v9);
      }
      else
      {
        v5 = -1073741816;
      }
      ObfDereferenceObject(v6);
    }
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v5;
}
43 row(s)
