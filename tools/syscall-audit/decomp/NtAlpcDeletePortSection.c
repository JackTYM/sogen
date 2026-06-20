__int64 __fastcall NtAlpcDeletePortSection(void *a1, int a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( a2 )
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
      v7 = AlpcReferenceBlobByHandle(*((_QWORD *)Object + 2) + 40LL, a3, AlpcSectionType);
      v8 = v7;
      if ( v7 )
      {
        if ( (unsigned __int8)AlpcpDeleteBlob(v7) )
        {
          AlpcpDereferenceBlobEx(v8);
        }
        else
        {
          v5 = -1073741738;
        }
        AlpcpDereferenceBlobEx(v8);
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
41 row(s)
