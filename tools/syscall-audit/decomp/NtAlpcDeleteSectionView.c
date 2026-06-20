__int64 __fastcall NtAlpcDeleteSectionView(void *a1, int a2, ULONG_PTR a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  *(_OWORD *)BugCheckParameter2 = 0LL;
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
      v6 = (volatile signed __int64 *)((char *)Object + 352);
      BugCheckParameter2[1] = 0LL;
      BugCheckParameter2[0] = a3;
      do
      {
        v8 = KeAbPreAcquire(v6, 0LL, 0LL);
        if ( _InterlockedCompareExchange64(v6, 17LL, 0LL) )
        {
          ExfAcquirePushLockSharedEx(v6, 0LL, v8, v6);
        }
        if ( v8 )
        {
          *(_BYTE *)(v8 + 10) = 1;
        }
        v10 = AlpcpEnumerateResourcesPort(Object, v7, v9, BugCheckParameter2);
        if ( _InterlockedCompareExchange64(v6, 0LL, 17LL) != 17 )
        {
          ExfReleasePushLockShared(v6);
        }
        KeAbPostRelease((ULONG_PTR)v6);
      }
      while ( v10 == -1073741267 );
      v11 = BugCheckParameter2[1];
      if ( BugCheckParameter2[1] )
      {
        v12 = AlpcpDeleteView(BugCheckParameter2[1]);
        AlpcpDereferenceBlobEx(v11);
        v5 = v12 == 0 ? 0xC0000056 : 0;
      }
      else
      {
        v5 = -1073741503;
      }
      ObfDereferenceObject(Object);
    }
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v5;
}
56 row(s)
